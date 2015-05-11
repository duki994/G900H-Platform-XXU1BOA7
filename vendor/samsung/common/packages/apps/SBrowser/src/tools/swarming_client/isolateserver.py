#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Archives a set of files or directories to a server."""

__version__ = '0.3.2'

import functools
import hashlib
import json
import logging
import os
import re
import shutil
import stat
import sys
import tempfile
import threading
import time
import urllib
import urlparse
import zlib

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import file_path
from utils import net
from utils import threading_utils
from utils import tools

import auth


# Version of isolate protocol passed to the server in /handshake request.
ISOLATE_PROTOCOL_VERSION = '1.0'
# Version stored and expected in .isolated files.
ISOLATED_FILE_VERSION = '1.3'


# The number of files to check the isolate server per /pre-upload query.
# All files are sorted by likelihood of a change in the file content
# (currently file size is used to estimate this: larger the file -> larger the
# possibility it has changed). Then first ITEMS_PER_CONTAINS_QUERIES[0] files
# are taken and send to '/pre-upload', then next ITEMS_PER_CONTAINS_QUERIES[1],
# and so on. Numbers here is a trade-off; the more per request, the lower the
# effect of HTTP round trip latency and TCP-level chattiness. On the other hand,
# larger values cause longer lookups, increasing the initial latency to start
# uploading, which is especially an issue for large files. This value is
# optimized for the "few thousands files to look up with minimal number of large
# files missing" case.
ITEMS_PER_CONTAINS_QUERIES = [20, 20, 50, 50, 50, 100]


# A list of already compressed extension types that should not receive any
# compression before being uploaded.
ALREADY_COMPRESSED_TYPES = [
    '7z', 'avi', 'cur', 'gif', 'h264', 'jar', 'jpeg', 'jpg', 'pdf', 'png',
    'wav', 'zip'
]


# The file size to be used when we don't know the correct file size,
# generally used for .isolated files.
UNKNOWN_FILE_SIZE = None


# The size of each chunk to read when downloading and unzipping files.
ZIPPED_FILE_CHUNK = 16 * 1024

# Chunk size to use when doing disk I/O.
DISK_FILE_CHUNK = 1024 * 1024

# Chunk size to use when reading from network stream.
NET_IO_FILE_CHUNK = 16 * 1024


# Read timeout in seconds for downloads from isolate storage. If there's no
# response from the server within this timeout whole download will be aborted.
DOWNLOAD_READ_TIMEOUT = 60

# Maximum expected delay (in seconds) between successive file fetches
# in run_tha_test. If it takes longer than that, a deadlock might be happening
# and all stack frames for all threads are dumped to log.
DEADLOCK_TIMEOUT = 5 * 60


# The delay (in seconds) to wait between logging statements when retrieving
# the required files. This is intended to let the user (or buildbot) know that
# the program is still running.
DELAY_BETWEEN_UPDATES_IN_SECS = 30


# Sadly, hashlib uses 'sha1' instead of the standard 'sha-1' so explicitly
# specify the names here.
SUPPORTED_ALGOS = {
  'md5': hashlib.md5,
  'sha-1': hashlib.sha1,
  'sha-512': hashlib.sha512,
}


# Used for serialization.
SUPPORTED_ALGOS_REVERSE = dict((v, k) for k, v in SUPPORTED_ALGOS.iteritems())


DEFAULT_BLACKLIST = (
  # Temporary vim or python files.
  r'^.+\.(?:pyc|swp)$',
  # .git or .svn directory.
  r'^(?:.+' + re.escape(os.path.sep) + r'|)\.(?:git|svn)$',
)


# Chromium-specific.
DEFAULT_BLACKLIST += (
  r'^.+\.(?:run_test_cases)$',
  r'^(?:.+' + re.escape(os.path.sep) + r'|)testserver\.log$',
)


class Error(Exception):
  """Generic runtime error."""
  pass


class ConfigError(ValueError):
  """Generic failure to load a .isolated file."""
  pass


class MappingError(OSError):
  """Failed to recreate the tree."""
  pass


def is_valid_hash(value, algo):
  """Returns if the value is a valid hash for the corresponding algorithm."""
  size = 2 * algo().digest_size
  return bool(re.match(r'^[a-fA-F0-9]{%d}$' % size, value))


def hash_file(filepath, algo):
  """Calculates the hash of a file without reading it all in memory at once.

  |algo| should be one of hashlib hashing algorithm.
  """
  digest = algo()
  with open(filepath, 'rb') as f:
    while True:
      chunk = f.read(DISK_FILE_CHUNK)
      if not chunk:
        break
      digest.update(chunk)
  return digest.hexdigest()


def stream_read(stream, chunk_size):
  """Reads chunks from |stream| and yields them."""
  while True:
    data = stream.read(chunk_size)
    if not data:
      break
    yield data


def file_read(filepath, chunk_size=DISK_FILE_CHUNK, offset=0):
  """Yields file content in chunks of |chunk_size| starting from |offset|."""
  with open(filepath, 'rb') as f:
    if offset:
      f.seek(offset)
    while True:
      data = f.read(chunk_size)
      if not data:
        break
      yield data


def file_write(filepath, content_generator):
  """Writes file content as generated by content_generator.

  Creates the intermediary directory as needed.

  Returns the number of bytes written.

  Meant to be mocked out in unit tests.
  """
  filedir = os.path.dirname(filepath)
  if not os.path.isdir(filedir):
    os.makedirs(filedir)
  total = 0
  with open(filepath, 'wb') as f:
    for d in content_generator:
      total += len(d)
      f.write(d)
  return total


def zip_compress(content_generator, level=7):
  """Reads chunks from |content_generator| and yields zip compressed chunks."""
  compressor = zlib.compressobj(level)
  for chunk in content_generator:
    compressed = compressor.compress(chunk)
    if compressed:
      yield compressed
  tail = compressor.flush(zlib.Z_FINISH)
  if tail:
    yield tail


def zip_decompress(content_generator, chunk_size=DISK_FILE_CHUNK):
  """Reads zipped data from |content_generator| and yields decompressed data.

  Decompresses data in small chunks (no larger than |chunk_size|) so that
  zip bomb file doesn't cause zlib to preallocate huge amount of memory.

  Raises IOError if data is corrupted or incomplete.
  """
  decompressor = zlib.decompressobj()
  compressed_size = 0
  try:
    for chunk in content_generator:
      compressed_size += len(chunk)
      data = decompressor.decompress(chunk, chunk_size)
      if data:
        yield data
      while decompressor.unconsumed_tail:
        data = decompressor.decompress(decompressor.unconsumed_tail, chunk_size)
        if data:
          yield data
    tail = decompressor.flush()
    if tail:
      yield tail
  except zlib.error as e:
    raise IOError(
        'Corrupted zip stream (read %d bytes) - %s' % (compressed_size, e))
  # Ensure all data was read and decompressed.
  if decompressor.unused_data or decompressor.unconsumed_tail:
    raise IOError('Not all data was decompressed')


def get_zip_compression_level(filename):
  """Given a filename calculates the ideal zip compression level to use."""
  file_ext = os.path.splitext(filename)[1].lower()
  # TODO(csharp): Profile to find what compression level works best.
  return 0 if file_ext in ALREADY_COMPRESSED_TYPES else 7


def create_directories(base_directory, files):
  """Creates the directory structure needed by the given list of files."""
  logging.debug('create_directories(%s, %d)', base_directory, len(files))
  # Creates the tree of directories to create.
  directories = set(os.path.dirname(f) for f in files)
  for item in list(directories):
    while item:
      directories.add(item)
      item = os.path.dirname(item)
  for d in sorted(directories):
    if d:
      os.mkdir(os.path.join(base_directory, d))


def create_symlinks(base_directory, files):
  """Creates any symlinks needed by the given set of files."""
  for filepath, properties in files:
    if 'l' not in properties:
      continue
    if sys.platform == 'win32':
      # TODO(maruel): Create symlink via the win32 api.
      logging.warning('Ignoring symlink %s', filepath)
      continue
    outfile = os.path.join(base_directory, filepath)
    # os.symlink() doesn't exist on Windows.
    os.symlink(properties['l'], outfile)  # pylint: disable=E1101


def is_valid_file(filepath, size):
  """Determines if the given files appears valid.

  Currently it just checks the file's size.
  """
  if size == UNKNOWN_FILE_SIZE:
    return os.path.isfile(filepath)
  actual_size = os.stat(filepath).st_size
  if size != actual_size:
    logging.warning(
        'Found invalid item %s; %d != %d',
        os.path.basename(filepath), actual_size, size)
    return False
  return True


class WorkerPool(threading_utils.AutoRetryThreadPool):
  """Thread pool that automatically retries on IOError and runs a preconfigured
  function.
  """
  # Initial and maximum number of worker threads.
  INITIAL_WORKERS = 2
  MAX_WORKERS = 16
  RETRIES = 5

  def __init__(self):
    super(WorkerPool, self).__init__(
        [IOError],
        self.RETRIES,
        self.INITIAL_WORKERS,
        self.MAX_WORKERS,
        0,
        'remote')


class Item(object):
  """An item to push to Storage.

  It starts its life in a main thread, travels to 'contains' thread, then to
  'push' thread and then finally back to the main thread.

  It is never used concurrently from multiple threads.
  """

  def __init__(self, digest, size, is_isolated=False):
    self.digest = digest
    self.size = size
    self.is_isolated = is_isolated
    self.compression_level = 6
    self.push_state = None

  def content(self, chunk_size):
    """Iterable with content of this item in chunks of given size.

    Arguments:
      chunk_size: preferred size of the chunk to produce, may be ignored.
    """
    raise NotImplementedError()


class FileItem(Item):
  """A file to push to Storage."""

  def __init__(self, path, digest, size, is_isolated):
    super(FileItem, self).__init__(digest, size, is_isolated)
    self.path = path
    self.compression_level = get_zip_compression_level(path)

  def content(self, chunk_size):
    return file_read(self.path, chunk_size)


class BufferItem(Item):
  """A byte buffer to push to Storage."""

  def __init__(self, buf, algo, is_isolated=False):
    super(BufferItem, self).__init__(
        algo(buf).hexdigest(), len(buf), is_isolated)
    self.buffer = buf

  def content(self, _chunk_size):
    return [self.buffer]


class Storage(object):
  """Efficiently downloads or uploads large set of files via StorageApi."""

  def __init__(self, storage_api, use_zip):
    self.use_zip = use_zip
    self._storage_api = storage_api
    self._cpu_thread_pool = None
    self._net_thread_pool = None

  @property
  def cpu_thread_pool(self):
    """ThreadPool for CPU-bound tasks like zipping."""
    if self._cpu_thread_pool is None:
      self._cpu_thread_pool = threading_utils.ThreadPool(
          2, max(threading_utils.num_processors(), 2), 0, 'zip')
    return self._cpu_thread_pool

  @property
  def net_thread_pool(self):
    """AutoRetryThreadPool for IO-bound tasks, retries IOError."""
    if self._net_thread_pool is None:
      self._net_thread_pool = WorkerPool()
    return self._net_thread_pool

  def close(self):
    """Waits for all pending tasks to finish."""
    if self._cpu_thread_pool:
      self._cpu_thread_pool.join()
      self._cpu_thread_pool.close()
      self._cpu_thread_pool = None
    if self._net_thread_pool:
      self._net_thread_pool.join()
      self._net_thread_pool.close()
      self._net_thread_pool = None

  def __enter__(self):
    """Context manager interface."""
    return self

  def __exit__(self, _exc_type, _exc_value, _traceback):
    """Context manager interface."""
    self.close()
    return False

  def upload_tree(self, indir, infiles):
    """Uploads the given tree to the isolate server.

    Arguments:
      indir: root directory the infiles are based in.
      infiles: dict of files to upload from |indir|.

    Returns:
      List of items that were uploaded. All other items are already there.
    """
    logging.info('upload tree(indir=%s, files=%d)', indir, len(infiles))

    # Convert |indir| + |infiles| into a list of FileItem objects.
    # Filter out symlinks, since they are not represented by items on isolate
    # server side.
    items = [
        FileItem(
            path=os.path.join(indir, filepath),
            digest=metadata['h'],
            size=metadata['s'],
            is_isolated=metadata.get('priority') == '0')
        for filepath, metadata in infiles.iteritems()
        if 'l' not in metadata
    ]

    return self.upload_items(items)

  def upload_items(self, items):
    """Uploads bunch of items to the isolate server.

    Will upload only items that are missing.

    Arguments:
      items: list of Item instances that represents data to upload.

    Returns:
      List of items that were uploaded. All other items are already there.
    """
    # TODO(vadimsh): Optimize special case of len(items) == 1 that is frequently
    # used by swarming.py. There's no need to spawn multiple threads and try to
    # do stuff in parallel: there's nothing to parallelize. 'contains' check and
    # 'push' should be performed sequentially in the context of current thread.

    # For each digest keep only first Item that matches it. All other items
    # are just indistinguishable copies from the point of view of isolate
    # server (it doesn't care about paths at all, only content and digests).
    seen = {}
    duplicates = 0
    for item in items:
      if seen.setdefault(item.digest, item) is not item:
        duplicates += 1
    items = seen.values()
    if duplicates:
      logging.info('Skipped %d duplicated files', duplicates)

    # Enqueue all upload tasks.
    missing = set()
    channel = threading_utils.TaskChannel()
    for missing_item in self.get_missing_items(items):
      missing.add(missing_item)
      self.async_push(
          channel,
          WorkerPool.HIGH if missing_item.is_isolated else WorkerPool.MED,
          missing_item)

    uploaded = []
    # No need to spawn deadlock detector thread if there's nothing to upload.
    if missing:
      with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT) as detector:
        # Wait for all started uploads to finish.
        while len(uploaded) != len(missing):
          detector.ping()
          item = channel.pull()
          uploaded.append(item)
          logging.debug(
              'Uploaded %d / %d: %s', len(uploaded), len(missing), item.digest)
    logging.info('All files are uploaded')

    # Print stats.
    total = len(items)
    total_size = sum(f.size for f in items)
    logging.info(
        'Total:      %6d, %9.1fkb',
        total,
        total_size / 1024.)
    cache_hit = set(items) - missing
    cache_hit_size = sum(f.size for f in cache_hit)
    logging.info(
        'cache hit:  %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
        len(cache_hit),
        cache_hit_size / 1024.,
        len(cache_hit) * 100. / total,
        cache_hit_size * 100. / total_size if total_size else 0)
    cache_miss = missing
    cache_miss_size = sum(f.size for f in cache_miss)
    logging.info(
        'cache miss: %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
        len(cache_miss),
        cache_miss_size / 1024.,
        len(cache_miss) * 100. / total,
        cache_miss_size * 100. / total_size if total_size else 0)

    return uploaded

  def get_fetch_url(self, digest):
    """Returns an URL that can be used to fetch an item with given digest.

    Arguments:
      digest: hex digest of item to fetch.

    Returns:
      An URL or None if underlying protocol doesn't support this.
    """
    return self._storage_api.get_fetch_url(digest)

  def async_push(self, channel, priority, item):
    """Starts asynchronous push to the server in a parallel thread.

    Arguments:
      channel: TaskChannel that receives back |item| when upload ends.
      priority: thread pool task priority for the push.
      item: item to upload as instance of Item class.
    """
    def push(content):
      """Pushes an item and returns its id, to pass as a result to |channel|."""
      self._storage_api.push(item, content)
      return item

    # If zipping is not required, just start a push task.
    if not self.use_zip:
      self.net_thread_pool.add_task_with_channel(channel, priority, push,
          item.content(DISK_FILE_CHUNK))
      return

    # If zipping is enabled, zip in a separate thread.
    def zip_and_push():
      # TODO(vadimsh): Implement streaming uploads. Before it's done, assemble
      # content right here. It will block until all file is zipped.
      try:
        stream = zip_compress(item.content(ZIPPED_FILE_CHUNK),
            item.compression_level)
        data = ''.join(stream)
      except Exception as exc:
        logging.error('Failed to zip \'%s\': %s', item, exc)
        channel.send_exception()
        return
      self.net_thread_pool.add_task_with_channel(
          channel, priority, push, [data])
    self.cpu_thread_pool.add_task(priority, zip_and_push)

  def async_fetch(self, channel, priority, digest, size, sink):
    """Starts asynchronous fetch from the server in a parallel thread.

    Arguments:
      channel: TaskChannel that receives back |digest| when download ends.
      priority: thread pool task priority for the fetch.
      digest: hex digest of an item to download.
      size: expected size of the item (after decompression).
      sink: function that will be called as sink(generator).
    """
    def fetch():
      try:
        # Prepare reading pipeline.
        stream = self._storage_api.fetch(digest)
        if self.use_zip:
          stream = zip_decompress(stream, DISK_FILE_CHUNK)
        # Run |stream| through verifier that will assert its size.
        verifier = FetchStreamVerifier(stream, size)
        # Verified stream goes to |sink|.
        sink(verifier.run())
      except Exception as err:
        logging.error('Failed to fetch %s: %s', digest, err)
        raise
      return digest

    # Don't bother with zip_thread_pool for decompression. Decompression is
    # really fast and most probably IO bound anyway.
    self.net_thread_pool.add_task_with_channel(channel, priority, fetch)

  def get_missing_items(self, items):
    """Yields items that are missing from the server.

    Issues multiple parallel queries via StorageApi's 'contains' method.

    Arguments:
      items: a list of Item objects to check.

    Yields:
      Item objects that are missing from the server.
    """
    channel = threading_utils.TaskChannel()
    pending = 0
    # Enqueue all requests.
    for batch in self.batch_items_for_check(items):
      self.net_thread_pool.add_task_with_channel(channel, WorkerPool.HIGH,
          self._storage_api.contains, batch)
      pending += 1
    # Yield results as they come in.
    for _ in xrange(pending):
      for missing in channel.pull():
        yield missing

  @staticmethod
  def batch_items_for_check(items):
    """Splits list of items to check for existence on the server into batches.

    Each batch corresponds to a single 'exists?' query to the server via a call
    to StorageApi's 'contains' method.

    Arguments:
      items: a list of Item objects.

    Yields:
      Batches of items to query for existence in a single operation,
      each batch is a list of Item objects.
    """
    batch_count = 0
    batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[0]
    next_queries = []
    for item in sorted(items, key=lambda x: x.size, reverse=True):
      next_queries.append(item)
      if len(next_queries) == batch_size_limit:
        yield next_queries
        next_queries = []
        batch_count += 1
        batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[
            min(batch_count, len(ITEMS_PER_CONTAINS_QUERIES) - 1)]
    if next_queries:
      yield next_queries


class FetchQueue(object):
  """Fetches items from Storage and places them into LocalCache.

  It manages multiple concurrent fetch operations. Acts as a bridge between
  Storage and LocalCache so that Storage and LocalCache don't depend on each
  other at all.
  """

  def __init__(self, storage, cache):
    self.storage = storage
    self.cache = cache
    self._channel = threading_utils.TaskChannel()
    self._pending = set()
    self._accessed = set()
    self._fetched = cache.cached_set()

  def add(self, priority, digest, size=UNKNOWN_FILE_SIZE):
    """Starts asynchronous fetch of item |digest|."""
    # Fetching it now?
    if digest in self._pending:
      return

    # Mark this file as in use, verify_all_cached will later ensure it is still
    # in cache.
    self._accessed.add(digest)

    # Already fetched? Notify cache to update item's LRU position.
    if digest in self._fetched:
      # 'touch' returns True if item is in cache and not corrupted.
      if self.cache.touch(digest, size):
        return
      # Item is corrupted, remove it from cache and fetch it again.
      self._fetched.remove(digest)
      self.cache.evict(digest)

    # TODO(maruel): It should look at the free disk space, the current cache
    # size and the size of the new item on every new item:
    # - Trim the cache as more entries are listed when free disk space is low,
    #   otherwise if the amount of data downloaded during the run > free disk
    #   space, it'll crash.
    # - Make sure there's enough free disk space to fit all dependencies of
    #   this run! If not, abort early.

    # Start fetching.
    self._pending.add(digest)
    self.storage.async_fetch(
        self._channel, priority, digest, size,
        functools.partial(self.cache.write, digest))

  def wait(self, digests):
    """Starts a loop that waits for at least one of |digests| to be retrieved.

    Returns the first digest retrieved.
    """
    # Flush any already fetched items.
    for digest in digests:
      if digest in self._fetched:
        return digest

    # Ensure all requested items are being fetched now.
    assert all(digest in self._pending for digest in digests), (
        digests, self._pending)

    # Wait for some requested item to finish fetching.
    while self._pending:
      digest = self._channel.pull()
      self._pending.remove(digest)
      self._fetched.add(digest)
      if digest in digests:
        return digest

    # Should never reach this point due to assert above.
    raise RuntimeError('Impossible state')

  def inject_local_file(self, path, algo):
    """Adds local file to the cache as if it was fetched from storage."""
    with open(path, 'rb') as f:
      data = f.read()
    digest = algo(data).hexdigest()
    self.cache.write(digest, [data])
    self._fetched.add(digest)
    return digest

  @property
  def pending_count(self):
    """Returns number of items to be fetched."""
    return len(self._pending)

  def verify_all_cached(self):
    """True if all accessed items are in cache."""
    return self._accessed.issubset(self.cache.cached_set())


class FetchStreamVerifier(object):
  """Verifies that fetched file is valid before passing it to the LocalCache."""

  def __init__(self, stream, expected_size):
    self.stream = stream
    self.expected_size = expected_size
    self.current_size = 0

  def run(self):
    """Generator that yields same items as |stream|.

    Verifies |stream| is complete before yielding a last chunk to consumer.

    Also wraps IOError produced by consumer into MappingError exceptions since
    otherwise Storage will retry fetch on unrelated local cache errors.
    """
    # Read one chunk ahead, keep it in |stored|.
    # That way a complete stream can be verified before pushing last chunk
    # to consumer.
    stored = None
    for chunk in self.stream:
      assert chunk is not None
      if stored is not None:
        self._inspect_chunk(stored, is_last=False)
        try:
          yield stored
        except IOError as exc:
          raise MappingError('Failed to store an item in cache: %s' % exc)
      stored = chunk
    if stored is not None:
      self._inspect_chunk(stored, is_last=True)
      try:
        yield stored
      except IOError as exc:
        raise MappingError('Failed to store an item in cache: %s' % exc)

  def _inspect_chunk(self, chunk, is_last):
    """Called for each fetched chunk before passing it to consumer."""
    self.current_size += len(chunk)
    if (is_last and (self.expected_size != UNKNOWN_FILE_SIZE) and
        (self.expected_size != self.current_size)):
      raise IOError('Incorrect file size: expected %d, got %d' % (
          self.expected_size, self.current_size))


class StorageApi(object):
  """Interface for classes that implement low-level storage operations."""

  def get_fetch_url(self, digest):
    """Returns an URL that can be used to fetch an item with given digest.

    Arguments:
      digest: hex digest of item to fetch.

    Returns:
      An URL or None if the protocol doesn't support this.
    """
    raise NotImplementedError()

  def fetch(self, digest, offset=0):
    """Fetches an object and yields its content.

    Arguments:
      digest: hash digest of item to download.
      offset: offset (in bytes) from the start of the file to resume fetch from.

    Yields:
      Chunks of downloaded item (as str objects).
    """
    raise NotImplementedError()

  def push(self, item, content):
    """Uploads an |item| with content generated by |content| generator.

    Arguments:
      item: Item object that holds information about an item being pushed.
      content: a generator that yields chunks to push.

    Returns:
      None.
    """
    raise NotImplementedError()

  def contains(self, items):
    """Checks for existence of given |items| on the server.

    Mutates |items| by assigning opaque implement specific object to Item's
    push_state attribute on missing entries in the datastore.

    Arguments:
      items: list of Item objects.

    Returns:
      A list of items missing on server as a list of Item objects.
    """
    raise NotImplementedError()


class _PushState(object):
  """State needed to call .push(), to be stored in Item.push_state.

  Note this needs to be a global class to support pickling.
  """

  def __init__(self, upload_url, finalize_url):
    self.upload_url = upload_url
    self.finalize_url = finalize_url
    self.uploaded = False
    self.finalized = False


class IsolateServer(StorageApi):
  """StorageApi implementation that downloads and uploads to Isolate Server.

  It uploads and downloads directly from Google Storage whenever appropriate.
  """

  def __init__(self, base_url, namespace):
    super(IsolateServer, self).__init__()
    assert base_url.startswith('http'), base_url
    self.base_url = base_url.rstrip('/')
    self.namespace = namespace
    self._lock = threading.Lock()
    self._server_caps = None

  @staticmethod
  def _generate_handshake_request():
    """Returns a dict to be sent as handshake request body."""
    # TODO(vadimsh): Set 'pusher' and 'fetcher' according to intended usage.
    return {
        'client_app_version': __version__,
        'fetcher': True,
        'protocol_version': ISOLATE_PROTOCOL_VERSION,
        'pusher': True,
    }

  @staticmethod
  def _validate_handshake_response(caps):
    """Validates and normalizes handshake response."""
    logging.info('Protocol version: %s', caps['protocol_version'])
    logging.info('Server version: %s', caps['server_app_version'])
    if caps.get('error'):
      raise MappingError(caps['error'])
    if not caps['access_token']:
      raise ValueError('access_token is missing')
    return caps

  @property
  def _server_capabilities(self):
    """Performs handshake with the server if not yet done.

    Returns:
      Server capabilities dictionary as returned by /handshake endpoint.

    Raises:
      MappingError if server rejects the handshake.
    """
    # TODO(maruel): Make this request much earlier asynchronously while the
    # files are being enumerated.
    with self._lock:
      if self._server_caps is None:
        request_body = json.dumps(
            self._generate_handshake_request(), separators=(',', ':'))
        response = net.url_read(
            url=self.base_url + '/content-gs/handshake',
            data=request_body,
            content_type='application/json',
            method='POST')
        if response is None:
          raise MappingError('Failed to perform handshake.')
        try:
          caps = json.loads(response)
          if not isinstance(caps, dict):
            raise ValueError('Expecting JSON dict')
          self._server_caps = self._validate_handshake_response(caps)
        except (ValueError, KeyError, TypeError) as exc:
          # KeyError exception has very confusing str conversion: it's just a
          # missing key value and nothing else. So print exception class name
          # as well.
          raise MappingError('Invalid handshake response (%s): %s' % (
              exc.__class__.__name__, exc))
      return self._server_caps

  def get_fetch_url(self, digest):
    assert isinstance(digest, basestring)
    return '%s/content-gs/retrieve/%s/%s' % (
        self.base_url, self.namespace, digest)

  def fetch(self, digest, offset=0):
    source_url = self.get_fetch_url(digest)
    logging.debug('download_file(%s, %d)', source_url, offset)

    # Because the app engine DB is only eventually consistent, retry 404 errors
    # because the file might just not be visible yet (even though it has been
    # uploaded).
    connection = net.url_open(
        source_url,
        retry_404=True,
        read_timeout=DOWNLOAD_READ_TIMEOUT,
        headers={'Range': 'bytes=%d-' % offset} if offset else None)

    if not connection:
      raise IOError('Request failed - %s' % source_url)

    # If |offset| is used, verify server respects it by checking Content-Range.
    if offset:
      content_range = connection.get_header('Content-Range')
      if not content_range:
        raise IOError('Missing Content-Range header')

      # 'Content-Range' format is 'bytes <offset>-<last_byte_index>/<size>'.
      # According to a spec, <size> can be '*' meaning "Total size of the file
      # is not known in advance".
      try:
        match = re.match(r'bytes (\d+)-(\d+)/(\d+|\*)', content_range)
        if not match:
          raise ValueError()
        content_offset = int(match.group(1))
        last_byte_index = int(match.group(2))
        size = None if match.group(3) == '*' else int(match.group(3))
      except ValueError:
        raise IOError('Invalid Content-Range header: %s' % content_range)

      # Ensure returned offset equals requested one.
      if offset != content_offset:
        raise IOError('Expecting offset %d, got %d (Content-Range is %s)' % (
            offset, content_offset, content_range))

      # Ensure entire tail of the file is returned.
      if size is not None and last_byte_index + 1 != size:
        raise IOError('Incomplete response. Content-Range: %s' % content_range)

    return stream_read(connection, NET_IO_FILE_CHUNK)

  def push(self, item, content):
    assert isinstance(item, Item)
    assert isinstance(item.push_state, _PushState)
    assert not item.push_state.finalized

    # TODO(vadimsh): Do not read from |content| generator when retrying push.
    # If |content| is indeed a generator, it can not be re-winded back
    # to the beginning of the stream. A retry will find it exhausted. A possible
    # solution is to wrap |content| generator with some sort of caching
    # restartable generator. It should be done alongside streaming support
    # implementation.

    # This push operation may be a retry after failed finalization call below,
    # no need to reupload contents in that case.
    if not item.push_state.uploaded:
      # A cheezy way to avoid memcpy of (possibly huge) file, until streaming
      # upload support is implemented.
      if isinstance(content, list) and len(content) == 1:
        content = content[0]
      else:
        content = ''.join(content)
      # PUT file to |upload_url|.
      response = net.url_read(
          url=item.push_state.upload_url,
          data=content,
          content_type='application/octet-stream',
          method='PUT')
      if response is None:
        raise IOError('Failed to upload a file %s to %s' % (
            item.digest, item.push_state.upload_url))
      item.push_state.uploaded = True
    else:
      logging.info(
          'A file %s already uploaded, retrying finalization only', item.digest)

    # Optionally notify the server that it's done.
    if item.push_state.finalize_url:
      # TODO(vadimsh): Calculate MD5 or CRC32C sum while uploading a file and
      # send it to isolated server. That way isolate server can verify that
      # the data safely reached Google Storage (GS provides MD5 and CRC32C of
      # stored files).
      response = net.url_read(
          url=item.push_state.finalize_url,
          data='',
          content_type='application/json',
          method='POST')
      if response is None:
        raise IOError('Failed to finalize an upload of %s' % item.digest)
    item.push_state.finalized = True

  def contains(self, items):
    logging.info('Checking existence of %d files...', len(items))

    # Request body is a json encoded list of dicts.
    body = [
        {
          'h': item.digest,
          's': item.size,
          'i': int(item.is_isolated),
        } for item in items
    ]

    query_url = '%s/content-gs/pre-upload/%s?token=%s' % (
        self.base_url,
        self.namespace,
        urllib.quote(self._server_capabilities['access_token']))
    response_body = net.url_read(
        url=query_url,
        data=json.dumps(body, separators=(',', ':')),
        content_type='application/json',
        method='POST')
    if response_body is None:
      raise MappingError('Failed to execute /pre-upload query')

    # Response body is a list of push_urls (or null if file is already present).
    try:
      response = json.loads(response_body)
      if not isinstance(response, list):
        raise ValueError('Expecting response with json-encoded list')
      if len(response) != len(items):
        raise ValueError(
            'Incorrect number of items in the list, expected %d, '
            'but got %d' % (len(items), len(response)))
    except ValueError as err:
      raise MappingError(
          'Invalid response from server: %s, body is %s' % (err, response_body))

    # Pick Items that are missing, attach _PushState to them.
    missing_items = []
    for i, push_urls in enumerate(response):
      if push_urls:
        assert len(push_urls) == 2, str(push_urls)
        item = items[i]
        assert item.push_state is None
        item.push_state = _PushState(push_urls[0], push_urls[1])
        missing_items.append(item)
    logging.info('Queried %d files, %d cache hit',
        len(items), len(items) - len(missing_items))
    return missing_items


class FileSystem(StorageApi):
  """StorageApi implementation that fetches data from the file system.

  The common use case is a NFS/CIFS file server that is mounted locally that is
  used to fetch the file on a local partition.
  """

  def __init__(self, base_path):
    super(FileSystem, self).__init__()
    self.base_path = base_path

  def get_fetch_url(self, digest):
    return None

  def fetch(self, digest, offset=0):
    assert isinstance(digest, basestring)
    return file_read(os.path.join(self.base_path, digest), offset=offset)

  def push(self, item, content):
    assert isinstance(item, Item)
    file_write(os.path.join(self.base_path, item.digest), content)

  def contains(self, items):
    return [
        item for item in items
        if not os.path.exists(os.path.join(self.base_path, item.digest))
    ]


class LocalCache(object):
  """Local cache that stores objects fetched via Storage.

  It can be accessed concurrently from multiple threads, so it should protect
  its internal state with some lock.
  """
  cache_dir = None

  def __enter__(self):
    """Context manager interface."""
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    """Context manager interface."""
    return False

  def cached_set(self):
    """Returns a set of all cached digests (always a new object)."""
    raise NotImplementedError()

  def touch(self, digest, size):
    """Ensures item is not corrupted and updates its LRU position.

    Arguments:
      digest: hash digest of item to check.
      size: expected size of this item.

    Returns:
      True if item is in cache and not corrupted.
    """
    raise NotImplementedError()

  def evict(self, digest):
    """Removes item from cache if it's there."""
    raise NotImplementedError()

  def read(self, digest):
    """Returns contents of the cached item as a single str."""
    raise NotImplementedError()

  def write(self, digest, content):
    """Reads data from |content| generator and stores it in cache."""
    raise NotImplementedError()

  def hardlink(self, digest, dest, file_mode):
    """Ensures file at |dest| has same content as cached |digest|.

    If file_mode is provided, it is used to set the executable bit if
    applicable.
    """
    raise NotImplementedError()


class MemoryCache(LocalCache):
  """LocalCache implementation that stores everything in memory."""

  def __init__(self):
    super(MemoryCache, self).__init__()
    # Let's not assume dict is thread safe.
    self._lock = threading.Lock()
    self._contents = {}

  def cached_set(self):
    with self._lock:
      return set(self._contents)

  def touch(self, digest, size):
    with self._lock:
      return digest in self._contents

  def evict(self, digest):
    with self._lock:
      self._contents.pop(digest, None)

  def read(self, digest):
    with self._lock:
      return self._contents[digest]

  def write(self, digest, content):
    # Assemble whole stream before taking the lock.
    data = ''.join(content)
    with self._lock:
      self._contents[digest] = data

  def hardlink(self, digest, dest, file_mode):
    """Since data is kept in memory, there is no filenode to hardlink."""
    file_write(dest, [self.read(digest)])
    if file_mode is not None:
      # Ignores all other bits.
      os.chmod(dest, file_mode & 0500)


def get_hash_algo(_namespace):
  """Return hash algorithm class to use when uploading to given |namespace|."""
  # TODO(vadimsh): Implement this at some point.
  return hashlib.sha1


def is_namespace_with_compression(namespace):
  """Returns True if given |namespace| stores compressed objects."""
  return namespace.endswith(('-gzip', '-deflate'))


def get_storage_api(file_or_url, namespace):
  """Returns an object that implements StorageApi interface."""
  if file_path.is_url(file_or_url):
    return IsolateServer(file_or_url, namespace)
  else:
    return FileSystem(file_or_url)


def get_storage(file_or_url, namespace):
  """Returns Storage class configured with appropriate StorageApi instance."""
  return Storage(
      get_storage_api(file_or_url, namespace),
      is_namespace_with_compression(namespace))


def expand_symlinks(indir, relfile):
  """Follows symlinks in |relfile|, but treating symlinks that point outside the
  build tree as if they were ordinary directories/files. Returns the final
  symlink-free target and a list of paths to symlinks encountered in the
  process.

  The rule about symlinks outside the build tree is for the benefit of the
  Chromium OS ebuild, which symlinks the output directory to an unrelated path
  in the chroot.

  Fails when a directory loop is detected, although in theory we could support
  that case.
  """
  is_directory = relfile.endswith(os.path.sep)
  done = indir
  todo = relfile.strip(os.path.sep)
  symlinks = []

  while todo:
    pre_symlink, symlink, post_symlink = file_path.split_at_symlink(
        done, todo)
    if not symlink:
      todo = file_path.fix_native_path_case(done, todo)
      done = os.path.join(done, todo)
      break
    symlink_path = os.path.join(done, pre_symlink, symlink)
    post_symlink = post_symlink.lstrip(os.path.sep)
    # readlink doesn't exist on Windows.
    # pylint: disable=E1101
    target = os.path.normpath(os.path.join(done, pre_symlink))
    symlink_target = os.readlink(symlink_path)
    if os.path.isabs(symlink_target):
      # Absolute path are considered a normal directories. The use case is
      # generally someone who puts the output directory on a separate drive.
      target = symlink_target
    else:
      # The symlink itself could be using the wrong path case.
      target = file_path.fix_native_path_case(target, symlink_target)

    if not os.path.exists(target):
      raise MappingError(
          'Symlink target doesn\'t exist: %s -> %s' % (symlink_path, target))
    target = file_path.get_native_path_case(target)
    if not file_path.path_starts_with(indir, target):
      done = symlink_path
      todo = post_symlink
      continue
    if file_path.path_starts_with(target, symlink_path):
      raise MappingError(
          'Can\'t map recursive symlink reference %s -> %s' %
          (symlink_path, target))
    logging.info('Found symlink: %s -> %s', symlink_path, target)
    symlinks.append(os.path.relpath(symlink_path, indir))
    # Treat the common prefix of the old and new paths as done, and start
    # scanning again.
    target = target.split(os.path.sep)
    symlink_path = symlink_path.split(os.path.sep)
    prefix_length = 0
    for target_piece, symlink_path_piece in zip(target, symlink_path):
      if target_piece == symlink_path_piece:
        prefix_length += 1
      else:
        break
    done = os.path.sep.join(target[:prefix_length])
    todo = os.path.join(
        os.path.sep.join(target[prefix_length:]), post_symlink)

  relfile = os.path.relpath(done, indir)
  relfile = relfile.rstrip(os.path.sep) + is_directory * os.path.sep
  return relfile, symlinks


def expand_directory_and_symlink(indir, relfile, blacklist, follow_symlinks):
  """Expands a single input. It can result in multiple outputs.

  This function is recursive when relfile is a directory.

  Note: this code doesn't properly handle recursive symlink like one created
  with:
    ln -s .. foo
  """
  if os.path.isabs(relfile):
    raise MappingError('Can\'t map absolute path %s' % relfile)

  infile = file_path.normpath(os.path.join(indir, relfile))
  if not infile.startswith(indir):
    raise MappingError('Can\'t map file %s outside %s' % (infile, indir))

  filepath = os.path.join(indir, relfile)
  native_filepath = file_path.get_native_path_case(filepath)
  if filepath != native_filepath:
    # Special case './'.
    if filepath != native_filepath + '.' + os.path.sep:
      # Give up enforcing strict path case on OSX. Really, it's that sad. The
      # case where it happens is very specific and hard to reproduce:
      # get_native_path_case(
      #    u'Foo.framework/Versions/A/Resources/Something.nib') will return
      # u'Foo.framework/Versions/A/resources/Something.nib', e.g. lowercase 'r'.
      #
      # Note that this is really something deep in OSX because running
      # ls Foo.framework/Versions/A
      # will print out 'Resources', while file_path.get_native_path_case()
      # returns a lower case 'r'.
      #
      # So *something* is happening under the hood resulting in the command 'ls'
      # and Carbon.File.FSPathMakeRef('path').FSRefMakePath() to disagree.  We
      # have no idea why.
      if sys.platform != 'darwin':
        raise MappingError(
            'File path doesn\'t equal native file path\n%s != %s' %
            (filepath, native_filepath))

  symlinks = []
  if follow_symlinks:
    relfile, symlinks = expand_symlinks(indir, relfile)

  if relfile.endswith(os.path.sep):
    if not os.path.isdir(infile):
      raise MappingError(
          '%s is not a directory but ends with "%s"' % (infile, os.path.sep))

    # Special case './'.
    if relfile.startswith('.' + os.path.sep):
      relfile = relfile[2:]
    outfiles = symlinks
    try:
      for filename in os.listdir(infile):
        inner_relfile = os.path.join(relfile, filename)
        if blacklist and blacklist(inner_relfile):
          continue
        if os.path.isdir(os.path.join(indir, inner_relfile)):
          inner_relfile += os.path.sep
        outfiles.extend(
            expand_directory_and_symlink(indir, inner_relfile, blacklist,
                                         follow_symlinks))
      return outfiles
    except OSError as e:
      raise MappingError(
          'Unable to iterate over directory %s.\n%s' % (infile, e))
  else:
    # Always add individual files even if they were blacklisted.
    if os.path.isdir(infile):
      raise MappingError(
          'Input directory %s must have a trailing slash' % infile)

    if not os.path.isfile(infile):
      raise MappingError('Input file %s doesn\'t exist' % infile)

    return symlinks + [relfile]


def process_input(filepath, prevdict, read_only, flavor, algo):
  """Processes an input file, a dependency, and return meta data about it.

  Behaviors:
  - Retrieves the file mode, file size, file timestamp, file link
    destination if it is a file link and calcultate the SHA-1 of the file's
    content if the path points to a file and not a symlink.

  Arguments:
    filepath: File to act on.
    prevdict: the previous dictionary. It is used to retrieve the cached sha-1
              to skip recalculating the hash. Optional.
    read_only: If 1 or 2, the file mode is manipulated. In practice, only save
               one of 4 modes: 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r). On
               windows, mode is not set since all files are 'executable' by
               default.
    flavor:    One isolated flavor, like 'linux', 'mac' or 'win'.
    algo:      Hashing algorithm used.

  Returns:
    The necessary data to create a entry in the 'files' section of an .isolated
    file.
  """
  out = {}
  # TODO(csharp): Fix crbug.com/150823 and enable the touched logic again.
  # if prevdict.get('T') == True:
  #   # The file's content is ignored. Skip the time and hard code mode.
  #   if get_flavor() != 'win':
  #     out['m'] = stat.S_IRUSR | stat.S_IRGRP
  #   out['s'] = 0
  #   out['h'] = algo().hexdigest()
  #   out['T'] = True
  #   return out

  # Always check the file stat and check if it is a link. The timestamp is used
  # to know if the file's content/symlink destination should be looked into.
  # E.g. only reuse from prevdict if the timestamp hasn't changed.
  # There is the risk of the file's timestamp being reset to its last value
  # manually while its content changed. We don't protect against that use case.
  try:
    filestats = os.lstat(filepath)
  except OSError:
    # The file is not present.
    raise MappingError('%s is missing' % filepath)
  is_link = stat.S_ISLNK(filestats.st_mode)

  if flavor != 'win':
    # Ignore file mode on Windows since it's not really useful there.
    filemode = stat.S_IMODE(filestats.st_mode)
    # Remove write access for group and all access to 'others'.
    filemode &= ~(stat.S_IWGRP | stat.S_IRWXO)
    if read_only:
      filemode &= ~stat.S_IWUSR
    if filemode & stat.S_IXUSR:
      filemode |= stat.S_IXGRP
    else:
      filemode &= ~stat.S_IXGRP
    if not is_link:
      out['m'] = filemode

  # Used to skip recalculating the hash or link destination. Use the most recent
  # update time.
  # TODO(maruel): Save it in the .state file instead of .isolated so the
  # .isolated file is deterministic.
  out['t'] = int(round(filestats.st_mtime))

  if not is_link:
    out['s'] = filestats.st_size
    # If the timestamp wasn't updated and the file size is still the same, carry
    # on the sha-1.
    if (prevdict.get('t') == out['t'] and
        prevdict.get('s') == out['s']):
      # Reuse the previous hash if available.
      out['h'] = prevdict.get('h')
    if not out.get('h'):
      out['h'] = hash_file(filepath, algo)
  else:
    # If the timestamp wasn't updated, carry on the link destination.
    if prevdict.get('t') == out['t']:
      # Reuse the previous link destination if available.
      out['l'] = prevdict.get('l')
    if out.get('l') is None:
      # The link could be in an incorrect path case. In practice, this only
      # happen on OSX on case insensitive HFS.
      # TODO(maruel): It'd be better if it was only done once, in
      # expand_directory_and_symlink(), so it would not be necessary to do again
      # here.
      symlink_value = os.readlink(filepath)  # pylint: disable=E1101
      filedir = file_path.get_native_path_case(os.path.dirname(filepath))
      native_dest = file_path.fix_native_path_case(filedir, symlink_value)
      out['l'] = os.path.relpath(native_dest, filedir)
  return out


def save_isolated(isolated, data):
  """Writes one or multiple .isolated files.

  Note: this reference implementation does not create child .isolated file so it
  always returns an empty list.

  Returns the list of child isolated files that are included by |isolated|.
  """
  # Make sure the data is valid .isolated data by 'reloading' it.
  algo = SUPPORTED_ALGOS[data['algo']]
  load_isolated(json.dumps(data), data.get('flavor'), algo)
  tools.write_json(isolated, data, True)
  return []



def upload_tree(base_url, indir, infiles, namespace):
  """Uploads the given tree to the given url.

  Arguments:
    base_url:  The base url, it is assume that |base_url|/has/ can be used to
               query if an element was already uploaded, and |base_url|/store/
               can be used to upload a new element.
    indir:     Root directory the infiles are based in.
    infiles:   dict of files to upload from |indir| to |base_url|.
    namespace: The namespace to use on the server.
  """
  with get_storage(base_url, namespace) as storage:
    storage.upload_tree(indir, infiles)
  return 0


def load_isolated(content, os_flavor, algo):
  """Verifies the .isolated file is valid and loads this object with the json
  data.

  Arguments:
  - content: raw serialized content to load.
  - os_flavor: OS to load this file on. Optional.
  - algo: hashlib algorithm class. Used to confirm the algorithm matches the
          algorithm used on the Isolate Server.
  """
  try:
    data = json.loads(content)
  except ValueError:
    raise ConfigError('Failed to parse: %s...' % content[:100])

  if not isinstance(data, dict):
    raise ConfigError('Expected dict, got %r' % data)

  # Check 'version' first, since it could modify the parsing after.
  # TODO(maruel): Drop support for unversioned .isolated file around Jan 2014.
  value = data.get('version', ISOLATED_FILE_VERSION)
  if not isinstance(value, basestring):
    raise ConfigError('Expected string, got %r' % value)
  if not re.match(r'^(\d+)\.(\d+)$', value):
    raise ConfigError('Expected a compatible version, got %r' % value)
  if value.split('.', 1)[0] != ISOLATED_FILE_VERSION.split('.', 1)[0]:
    raise ConfigError(
        'Expected compatible \'%s\' version, got %r' %
        (ISOLATED_FILE_VERSION, value))

  if algo is None:
    # TODO(maruel): Remove the default around Jan 2014.
    # Default the algorithm used in the .isolated file itself, falls back to
    # 'sha-1' if unspecified.
    algo = SUPPORTED_ALGOS_REVERSE[data.get('algo', 'sha-1')]

  for key, value in data.iteritems():
    if key == 'algo':
      if not isinstance(value, basestring):
        raise ConfigError('Expected string, got %r' % value)
      if value not in SUPPORTED_ALGOS:
        raise ConfigError(
            'Expected one of \'%s\', got %r' %
            (', '.join(sorted(SUPPORTED_ALGOS)), value))
      if value != SUPPORTED_ALGOS_REVERSE[algo]:
        raise ConfigError(
            'Expected \'%s\', got %r' % (SUPPORTED_ALGOS_REVERSE[algo], value))

    elif key == 'command':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty command')
      for subvalue in value:
        if not isinstance(subvalue, basestring):
          raise ConfigError('Expected string, got %r' % subvalue)

    elif key == 'files':
      if not isinstance(value, dict):
        raise ConfigError('Expected dict, got %r' % value)
      for subkey, subvalue in value.iteritems():
        if not isinstance(subkey, basestring):
          raise ConfigError('Expected string, got %r' % subkey)
        if not isinstance(subvalue, dict):
          raise ConfigError('Expected dict, got %r' % subvalue)
        for subsubkey, subsubvalue in subvalue.iteritems():
          if subsubkey == 'l':
            if not isinstance(subsubvalue, basestring):
              raise ConfigError('Expected string, got %r' % subsubvalue)
          elif subsubkey == 'm':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'h':
            if not is_valid_hash(subsubvalue, algo):
              raise ConfigError('Expected sha-1, got %r' % subsubvalue)
          elif subsubkey == 's':
            if not isinstance(subsubvalue, (int, long)):
              raise ConfigError('Expected int or long, got %r' % subsubvalue)
          else:
            raise ConfigError('Unknown subsubkey %s' % subsubkey)
        if bool('h' in subvalue) == bool('l' in subvalue):
          raise ConfigError(
              'Need only one of \'h\' (sha-1) or \'l\' (link), got: %r' %
              subvalue)
        if bool('h' in subvalue) != bool('s' in subvalue):
          raise ConfigError(
              'Both \'h\' (sha-1) and \'s\' (size) should be set, got: %r' %
              subvalue)
        if bool('s' in subvalue) == bool('l' in subvalue):
          raise ConfigError(
              'Need only one of \'s\' (size) or \'l\' (link), got: %r' %
              subvalue)
        if bool('l' in subvalue) and bool('m' in subvalue):
          raise ConfigError(
              'Cannot use \'m\' (mode) and \'l\' (link), got: %r' %
              subvalue)

    elif key == 'includes':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty includes list')
      for subvalue in value:
        if not is_valid_hash(subvalue, algo):
          raise ConfigError('Expected sha-1, got %r' % subvalue)

    elif key == 'read_only':
      if not value in (0, 1, 2):
        raise ConfigError('Expected 0, 1 or 2, got %r' % value)

    elif key == 'relative_cwd':
      if not isinstance(value, basestring):
        raise ConfigError('Expected string, got %r' % value)

    elif key == 'os':
      if os_flavor and value != os_flavor:
        raise ConfigError(
            'Expected \'os\' to be \'%s\' but got \'%s\'' %
            (os_flavor, value))

    elif key == 'version':
      # Already checked above.
      pass

    else:
      raise ConfigError('Unknown key %r' % key)

  # Automatically fix os.path.sep if necessary. While .isolated files are always
  # in the the native path format, someone could want to download an .isolated
  # tree from another OS.
  wrong_path_sep = '/' if os.path.sep == '\\' else '\\'
  if 'files' in data:
    data['files'] = dict(
        (k.replace(wrong_path_sep, os.path.sep), v)
        for k, v in data['files'].iteritems())
    for v in data['files'].itervalues():
      if 'l' in v:
        v['l'] = v['l'].replace(wrong_path_sep, os.path.sep)
  if 'relative_cwd' in data:
    data['relative_cwd'] = data['relative_cwd'].replace(
        wrong_path_sep, os.path.sep)
  return data


class IsolatedFile(object):
  """Represents a single parsed .isolated file."""
  def __init__(self, obj_hash, algo):
    """|obj_hash| is really the sha-1 of the file."""
    logging.debug('IsolatedFile(%s)' % obj_hash)
    self.obj_hash = obj_hash
    self.algo = algo
    # Set once all the left-side of the tree is parsed. 'Tree' here means the
    # .isolate and all the .isolated files recursively included by it with
    # 'includes' key. The order of each sha-1 in 'includes', each representing a
    # .isolated file in the hash table, is important, as the later ones are not
    # processed until the firsts are retrieved and read.
    self.can_fetch = False

    # Raw data.
    self.data = {}
    # A IsolatedFile instance, one per object in self.includes.
    self.children = []

    # Set once the .isolated file is loaded.
    self._is_parsed = False
    # Set once the files are fetched.
    self.files_fetched = False

  def load(self, os_flavor, content):
    """Verifies the .isolated file is valid and loads this object with the json
    data.
    """
    logging.debug('IsolatedFile.load(%s)' % self.obj_hash)
    assert not self._is_parsed
    self.data = load_isolated(content, os_flavor, self.algo)
    self.children = [
        IsolatedFile(i, self.algo) for i in self.data.get('includes', [])
    ]
    self._is_parsed = True

  def fetch_files(self, fetch_queue, files):
    """Adds files in this .isolated file not present in |files| dictionary.

    Preemptively request files.

    Note that |files| is modified by this function.
    """
    assert self.can_fetch
    if not self._is_parsed or self.files_fetched:
      return
    logging.debug('fetch_files(%s)' % self.obj_hash)
    for filepath, properties in self.data.get('files', {}).iteritems():
      # Root isolated has priority on the files being mapped. In particular,
      # overriden files must not be fetched.
      if filepath not in files:
        files[filepath] = properties
        if 'h' in properties:
          # Preemptively request files.
          logging.debug('fetching %s' % filepath)
          fetch_queue.add(WorkerPool.MED, properties['h'], properties['s'])
    self.files_fetched = True


class Settings(object):
  """Results of a completely parsed .isolated file."""
  def __init__(self):
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None
    # The main .isolated file, a IsolatedFile instance.
    self.root = None

  def load(self, fetch_queue, root_isolated_hash, os_flavor, algo):
    """Loads the .isolated and all the included .isolated asynchronously.

    It enables support for "included" .isolated files. They are processed in
    strict order but fetched asynchronously from the cache. This is important so
    that a file in an included .isolated file that is overridden by an embedding
    .isolated file is not fetched needlessly. The includes are fetched in one
    pass and the files are fetched as soon as all the ones on the left-side
    of the tree were fetched.

    The prioritization is very important here for nested .isolated files.
    'includes' have the highest priority and the algorithm is optimized for both
    deep and wide trees. A deep one is a long link of .isolated files referenced
    one at a time by one item in 'includes'. A wide one has a large number of
    'includes' in a single .isolated file. 'left' is defined as an included
    .isolated file earlier in the 'includes' list. So the order of the elements
    in 'includes' is important.
    """
    self.root = IsolatedFile(root_isolated_hash, algo)

    # Isolated files being retrieved now: hash -> IsolatedFile instance.
    pending = {}
    # Set of hashes of already retrieved items to refuse recursive includes.
    seen = set()

    def retrieve(isolated_file):
      h = isolated_file.obj_hash
      if h in seen:
        raise ConfigError('IsolatedFile %s is retrieved recursively' % h)
      assert h not in pending
      seen.add(h)
      pending[h] = isolated_file
      fetch_queue.add(WorkerPool.HIGH, h)

    retrieve(self.root)

    while pending:
      item_hash = fetch_queue.wait(pending)
      item = pending.pop(item_hash)
      item.load(os_flavor, fetch_queue.cache.read(item_hash))
      if item_hash == root_isolated_hash:
        # It's the root item.
        item.can_fetch = True

      for new_child in item.children:
        retrieve(new_child)

      # Traverse the whole tree to see if files can now be fetched.
      self._traverse_tree(fetch_queue, self.root)

    def check(n):
      return all(check(x) for x in n.children) and n.files_fetched
    assert check(self.root)

    self.relative_cwd = self.relative_cwd or ''

  def _traverse_tree(self, fetch_queue, node):
    if node.can_fetch:
      if not node.files_fetched:
        self._update_self(fetch_queue, node)
      will_break = False
      for i in node.children:
        if not i.can_fetch:
          if will_break:
            break
          # Automatically mark the first one as fetcheable.
          i.can_fetch = True
          will_break = True
        self._traverse_tree(fetch_queue, i)

  def _update_self(self, fetch_queue, node):
    node.fetch_files(fetch_queue, self.files)
    # Grabs properties.
    if not self.command and node.data.get('command'):
      # Ensure paths are correctly separated on windows.
      self.command = node.data['command']
      if self.command:
        self.command[0] = self.command[0].replace('/', os.path.sep)
        self.command = tools.fix_python_path(self.command)
    if self.read_only is None and node.data.get('read_only') is not None:
      self.read_only = node.data['read_only']
    if (self.relative_cwd is None and
        node.data.get('relative_cwd') is not None):
      self.relative_cwd = node.data['relative_cwd']


def fetch_isolated(
    isolated_hash, storage, cache, algo, outdir, os_flavor, require_command):
  """Aggressively downloads the .isolated file(s), then download all the files.

  Arguments:
    isolated_hash: hash of the root *.isolated file.
    storage: Storage class that communicates with isolate storage.
    cache: LocalCache class that knows how to store and map files locally.
    algo: hash algorithm to use.
    outdir: Output directory to map file tree to.
    os_flavor: OS flavor to choose when reading sections of *.isolated file.
    require_command: Ensure *.isolated specifies a command to run.

  Returns:
    Settings object that holds details about loaded *.isolated file.
  """
  with cache:
    fetch_queue = FetchQueue(storage, cache)
    settings = Settings()

    with tools.Profiler('GetIsolateds'):
      # Optionally support local files by manually adding them to cache.
      if not is_valid_hash(isolated_hash, algo):
        isolated_hash = fetch_queue.inject_local_file(isolated_hash, algo)

      # Load all *.isolated and start loading rest of the files.
      settings.load(fetch_queue, isolated_hash, os_flavor, algo)
      if require_command and not settings.command:
        # TODO(vadimsh): All fetch operations are already enqueue and there's no
        # easy way to cancel them.
        raise ConfigError('No command to run')

    with tools.Profiler('GetRest'):
      # Create file system hierarchy.
      if not os.path.isdir(outdir):
        os.makedirs(outdir)
      create_directories(outdir, settings.files)
      create_symlinks(outdir, settings.files.iteritems())

      # Ensure working directory exists.
      cwd = os.path.normpath(os.path.join(outdir, settings.relative_cwd))
      if not os.path.isdir(cwd):
        os.makedirs(cwd)

      # Multimap: digest -> list of pairs (path, props).
      remaining = {}
      for filepath, props in settings.files.iteritems():
        if 'h' in props:
          remaining.setdefault(props['h'], []).append((filepath, props))

      # Now block on the remaining files to be downloaded and mapped.
      logging.info('Retrieving remaining files (%d of them)...',
          fetch_queue.pending_count)
      last_update = time.time()
      with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT) as detector:
        while remaining:
          detector.ping()

          # Wait for any item to finish fetching to cache.
          digest = fetch_queue.wait(remaining)

          # Link corresponding files to a fetched item in cache.
          for filepath, props in remaining.pop(digest):
            cache.hardlink(
                digest, os.path.join(outdir, filepath), props.get('m'))

          # Report progress.
          duration = time.time() - last_update
          if duration > DELAY_BETWEEN_UPDATES_IN_SECS:
            msg = '%d files remaining...' % len(remaining)
            print msg
            logging.info(msg)
            last_update = time.time()

  # Cache could evict some items we just tried to fetch, it's a fatal error.
  if not fetch_queue.verify_all_cached():
    raise MappingError('Cache is too small to hold all requested files')
  return settings


def directory_to_metadata(root, algo, blacklist):
  """Returns the FileItem list and .isolated metadata for a directory."""
  root = file_path.get_native_path_case(root)
  metadata = dict(
      (relpath, process_input(
        os.path.join(root, relpath), {}, False, sys.platform, algo))
      for relpath in expand_directory_and_symlink(
        root, './', blacklist, True)
  )
  for v in metadata.itervalues():
    v.pop('t')
  items = [
      FileItem(
          path=os.path.join(root, relpath),
          digest=meta['h'],
          size=meta['s'],
          is_isolated=relpath.endswith('.isolated'))
      for relpath, meta in metadata.iteritems() if 'h' in meta
  ]
  return items, metadata


def archive_files_to_storage(storage, algo, files, blacklist):
  """Stores every entries and returns the relevant data.

  Arguments:
    storage: a Storage object that communicates with the remote object store.
    algo: an hashlib class to hash content. Usually hashlib.sha1.
    files: list of file paths to upload. If a directory is specified, a
           .isolated file is created and its hash is returned.
    blacklist: function that returns True if a file should be omitted.
  """
  assert all(isinstance(i, unicode) for i in files), files
  if len(files) != len(set(map(os.path.abspath, files))):
    raise Error('Duplicate entries found.')

  results = []
  # The temporary directory is only created as needed.
  tempdir = None
  try:
    # TODO(maruel): Yield the files to a worker thread.
    items_to_upload = []
    for f in files:
      try:
        filepath = os.path.abspath(f)
        if os.path.isdir(filepath):
          # Uploading a whole directory.
          items, metadata = directory_to_metadata(filepath, algo, blacklist)

          # Create the .isolated file.
          if not tempdir:
            tempdir = tempfile.mkdtemp(prefix='isolateserver')
          handle, isolated = tempfile.mkstemp(dir=tempdir, suffix='.isolated')
          os.close(handle)
          data = {
              'algo': SUPPORTED_ALGOS_REVERSE[algo],
              'files': metadata,
              'version': ISOLATED_FILE_VERSION,
          }
          save_isolated(isolated, data)
          h = hash_file(isolated, algo)
          items_to_upload.extend(items)
          items_to_upload.append(
              FileItem(
                  path=isolated,
                  digest=h,
                  size=os.stat(isolated).st_size,
                  is_isolated=True))
          results.append((h, f))

        elif os.path.isfile(filepath):
          h = hash_file(filepath, algo)
          items_to_upload.append(
            FileItem(
                path=filepath,
                digest=h,
                size=os.stat(filepath).st_size,
                is_isolated=f.endswith('.isolated')))
          results.append((h, f))
        else:
          raise Error('%s is neither a file or directory.' % f)
      except OSError:
        raise Error('Failed to process %s.' % f)
    # Technically we would care about which files were uploaded but we don't
    # much in practice.
    _uploaded_files = storage.upload_items(items_to_upload)
    return results
  finally:
    if tempdir:
      shutil.rmtree(tempdir)


def archive(out, namespace, files, blacklist):
  if files == ['-']:
    files = sys.stdin.readlines()

  if not files:
    raise Error('Nothing to upload')

  files = [f.decode('utf-8') for f in files]
  algo = get_hash_algo(namespace)
  blacklist = tools.gen_blacklist(blacklist)
  with get_storage(out, namespace) as storage:
    results = archive_files_to_storage(storage, algo, files, blacklist)
  print('\n'.join('%s %s' % (r[0], r[1]) for r in results))


@subcommand.usage('<file1..fileN> or - to read from stdin')
def CMDarchive(parser, args):
  """Archives data to the server.

  If a directory is specified, a .isolated file is created the whole directory
  is uploaded. Then this .isolated file can be included in another one to run
  commands.

  The commands output each file that was processed with its content hash. For
  directories, the .isolated generated for the directory is listed as the
  directory entry itself.
  """
  add_isolate_server_options(parser, False)
  parser.add_option(
      '--blacklist',
      action='append', default=list(DEFAULT_BLACKLIST),
      help='List of regexp to use as blacklist filter when uploading '
           'directories')
  options, files = parser.parse_args(args)
  process_isolate_server_options(parser, options)
  try:
    archive(options.isolate_server, options.namespace, files, options.blacklist)
  except Error as e:
    parser.error(e.args[0])
  return 0


def CMDdownload(parser, args):
  """Download data from the server.

  It can either download individual files or a complete tree from a .isolated
  file.
  """
  add_isolate_server_options(parser, True)
  parser.add_option(
      '-i', '--isolated', metavar='HASH',
      help='hash of an isolated file, .isolated file content is discarded, use '
           '--file if you need it')
  parser.add_option(
      '-f', '--file', metavar='HASH DEST', default=[], action='append', nargs=2,
      help='hash and destination of a file, can be used multiple times')
  parser.add_option(
      '-t', '--target', metavar='DIR', default=os.getcwd(),
      help='destination directory')
  options, args = parser.parse_args(args)
  process_isolate_server_options(parser, options)
  if args:
    parser.error('Unsupported arguments: %s' % args)
  if bool(options.isolated) == bool(options.file):
    parser.error('Use one of --isolated or --file, and only one.')

  options.target = os.path.abspath(options.target)

  remote = options.isolate_server or options.indir
  with get_storage(remote, options.namespace) as storage:
    # Fetching individual files.
    if options.file:
      channel = threading_utils.TaskChannel()
      pending = {}
      for digest, dest in options.file:
        pending[digest] = dest
        storage.async_fetch(
            channel,
            WorkerPool.MED,
            digest,
            UNKNOWN_FILE_SIZE,
            functools.partial(file_write, os.path.join(options.target, dest)))
      while pending:
        fetched = channel.pull()
        dest = pending.pop(fetched)
        logging.info('%s: %s', fetched, dest)

    # Fetching whole isolated tree.
    if options.isolated:
      settings = fetch_isolated(
          isolated_hash=options.isolated,
          storage=storage,
          cache=MemoryCache(),
          algo=get_hash_algo(options.namespace),
          outdir=options.target,
          os_flavor=None,
          require_command=False)
      rel = os.path.join(options.target, settings.relative_cwd)
      print('To run this test please run from the directory %s:' %
            os.path.join(options.target, rel))
      print('  ' + ' '.join(settings.command))

  return 0


@subcommand.usage('<file1..fileN> or - to read from stdin')
def CMDhashtable(parser, args):
  """Archives data to a hashtable on the file system.

  If a directory is specified, a .isolated file is created the whole directory
  is uploaded. Then this .isolated file can be included in another one to run
  commands.

  The commands output each file that was processed with its content hash. For
  directories, the .isolated generated for the directory is listed as the
  directory entry itself.
  """
  add_outdir_options(parser)
  parser.add_option(
      '--blacklist',
      action='append', default=list(DEFAULT_BLACKLIST),
      help='List of regexp to use as blacklist filter when uploading '
           'directories')
  options, files = parser.parse_args(args)
  process_outdir_options(parser, options, os.getcwd())
  try:
    # Do not compress files when archiving to the file system.
    archive(options.outdir, 'default', files, options.blacklist)
  except Error as e:
    parser.error(e.args[0])
  return 0


def add_isolate_server_options(parser, add_indir):
  """Adds --isolate-server and --namespace options to parser.

  Includes --indir if desired.
  """
  parser.add_option(
      '-I', '--isolate-server',
      metavar='URL', default=os.environ.get('ISOLATE_SERVER', ''),
      help='URL of the Isolate Server to use. Defaults to the environment '
           'variable ISOLATE_SERVER if set. No need to specify https://, this '
           'is assumed.')
  parser.add_option(
      '--namespace', default='default-gzip',
      help='The namespace to use on the Isolate Server, default: %default')
  if add_indir:
    parser.add_option(
        '--indir', metavar='DIR',
        help='Directory used to store the hashtable instead of using an '
             'isolate server.')


def process_isolate_server_options(parser, options):
  """Processes the --isolate-server and --indir options and aborts if neither is
  specified.
  """
  has_indir = hasattr(options, 'indir')
  if not options.isolate_server:
    if not has_indir:
      parser.error('--isolate-server is required.')
    elif not options.indir:
      parser.error('Use one of --indir or --isolate-server.')
  else:
    if has_indir and options.indir:
      parser.error('Use only one of --indir or --isolate-server.')

  if options.isolate_server:
    parts = urlparse.urlparse(options.isolate_server, 'https')
    if parts.query:
      parser.error('--isolate-server doesn\'t support query parameter.')
    if parts.fragment:
      parser.error('--isolate-server doesn\'t support fragment in the url.')
    # urlparse('foo.com') will result in netloc='', path='foo.com', which is not
    # what is desired here.
    new = list(parts)
    if not new[1] and new[2]:
      new[1] = new[2].rstrip('/')
      new[2] = ''
    new[2] = new[2].rstrip('/')
    options.isolate_server = urlparse.urlunparse(new)
    return

  if file_path.is_url(options.indir):
    parser.error('Can\'t use an URL for --indir.')
  options.indir = unicode(options.indir).replace('/', os.path.sep)
  options.indir = os.path.abspath(
      os.path.normpath(os.path.join(os.getcwd(), options.indir)))
  if not os.path.isdir(options.indir):
    parser.error('Path given to --indir must exist.')



def add_outdir_options(parser):
  """Adds --outdir, which is orthogonal to --isolate-server.

  Note: On upload, separate commands are used between 'archive' and 'hashtable'.
  On 'download', the same command can download from either an isolate server or
  a file system.
  """
  parser.add_option(
      '-o', '--outdir', metavar='DIR',
      help='Directory used to recreate the tree.')


def process_outdir_options(parser, options, cwd):
  if not options.outdir:
    parser.error('--outdir is required.')
  if file_path.is_url(options.outdir):
    parser.error('Can\'t use an URL for --outdir.')
  options.outdir = unicode(options.outdir).replace('/', os.path.sep)
  # outdir doesn't need native path case since tracing is never done from there.
  options.outdir = os.path.abspath(
      os.path.normpath(os.path.join(cwd, options.outdir)))
  # In theory, we'd create the directory outdir right away. Defer doing it in
  # case there's errors in the command line.


class OptionParserIsolateServer(tools.OptionParserWithLogging):
  def __init__(self, **kwargs):
    tools.OptionParserWithLogging.__init__(
        self,
        version=__version__,
        prog=os.path.basename(sys.modules[__name__].__file__),
        **kwargs)
    auth.add_auth_options(self)

  def parse_args(self, *args, **kwargs):
    options, args = tools.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    auth.process_auth_options(self, options)
    return options, args


def main(args):
  dispatcher = subcommand.CommandDispatcher(__name__)
  try:
    return dispatcher.execute(OptionParserIsolateServer(), args)
  except Exception as e:
    tools.report_error(e)
    return 1


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
