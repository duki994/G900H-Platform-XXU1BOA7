# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mimetypes
import posixpath

from compiled_file_system import SingleFile
from directory_zipper import DirectoryZipper
from docs_server_utils import ToUnicode
from file_system import FileNotFoundError
from future import Gettable, Future
from path_canonicalizer import PathCanonicalizer
from path_util import AssertIsValid, Join, ToDirectory
from special_paths import SITE_VERIFICATION_FILE
from third_party.handlebar import Handlebar
from third_party.markdown import markdown


_MIMETYPE_OVERRIDES = {
  # SVG is not supported by mimetypes.guess_type on AppEngine.
  '.svg': 'image/svg+xml',
}


class ContentAndType(object):
  '''Return value from ContentProvider.GetContentAndType.
  '''

  def __init__(self, content, content_type):
    self.content = content
    self.content_type = content_type


class ContentProvider(object):
  '''Returns file contents correctly typed for their content-types (in the HTTP
  sense). Content-type is determined from Python's mimetype library which
  guesses based on the file extension.

  Typically the file contents will be either str (for binary content) or
  unicode (for text content). However, HTML files *may* be returned as
  Handlebar templates (if |supports_templates| is True on construction), in
  which case the caller will presumably want to Render them.

  Zip file are automatically created and returned for .zip file extensions if
  |supports_zip| is True.

  |default_extensions| is a list of file extensions which are queried when no
  file extension is given to GetCanonicalPath/GetContentAndType.  Typically
  this will include .html.
  '''

  def __init__(self,
               name,
               compiled_fs_factory,
               file_system,
               object_store_creator,
               default_extensions=(),
               supports_templates=False,
               supports_zip=False):
    # Public.
    self.name = name
    self.file_system = file_system
    # Private.
    self._content_cache = compiled_fs_factory.Create(file_system,
                                                     self._CompileContent,
                                                     ContentProvider)
    self._path_canonicalizer = PathCanonicalizer(file_system,
                                                 object_store_creator,
                                                 default_extensions)
    self._default_extensions = default_extensions
    self._supports_templates = supports_templates
    if supports_zip:
      self._directory_zipper = DirectoryZipper(compiled_fs_factory, file_system)
    else:
      self._directory_zipper = None

  @SingleFile
  def _CompileContent(self, path, text):
    assert text is not None, path
    _, ext = posixpath.splitext(path)
    mimetype = _MIMETYPE_OVERRIDES.get(ext, mimetypes.guess_type(path)[0])
    if ext == '.md':
      # See http://pythonhosted.org/Markdown/extensions
      # for details on "extensions=".
      content = markdown(ToUnicode(text),
                         extensions=('extra', 'headerid', 'sane_lists'))
      if self._supports_templates:
        content = Handlebar(content, name=path)
      mimetype = 'text/html'
    elif mimetype is None:
      content = text
      mimetype = 'text/plain'
    elif mimetype == 'text/html':
      content = ToUnicode(text)
      if self._supports_templates:
        content = Handlebar(content, name=path)
    elif (mimetype.startswith('text/') or
          mimetype in ('application/javascript', 'application/json')):
      content = ToUnicode(text)
    else:
      content = text
    return ContentAndType(content, mimetype)

  def GetCanonicalPath(self, path):
    '''Gets the canonical location of |path|. This class is tolerant of
    spelling errors and missing files that are in other directories, and this
    returns the correct/canonical path for those.

    For example, the canonical path of "browseraction" is probably
    "extensions/browserAction.html".

    Note that the canonical path is relative to this content provider i.e.
    given relative to |path|. It does not add the "serveFrom" prefix which
    would have been pulled out in ContentProviders, callers must do that
    themselves.
    '''
    AssertIsValid(path)
    base, ext = posixpath.splitext(path)
    if self._directory_zipper and ext == '.zip':
      # The canonical location of zip files is the canonical location of the
      # directory to zip + '.zip'.
      return self._path_canonicalizer.Canonicalize(base + '/').rstrip('/') + ext
    return self._path_canonicalizer.Canonicalize(path)

  def GetContentAndType(self, path):
    '''Returns the ContentAndType of the file at |path|.
    '''
    AssertIsValid(path)
    base, ext = posixpath.splitext(path)

    # Check for a zip file first, if zip is enabled.
    if self._directory_zipper and ext == '.zip':
      zip_future = self._directory_zipper.Zip(ToDirectory(base))
      return Future(delegate=Gettable(
          lambda: ContentAndType(zip_future.Get(), 'application/zip')))

    # If there is no file extension, look for a file with one of the default
    # extensions.
    #
    # Note that it would make sense to guard this on Exists(path), since a file
    # without an extension may actually exist, but it's such an uncommon case
    # it hardly seems worth the potential performance hit.
    if not ext:
      for default_ext in self._default_extensions:
        if self.file_system.Exists(path + default_ext).Get():
          path += default_ext
          break

    return self._content_cache.GetFromFile(path)

  def Cron(self):
    futures = [self._path_canonicalizer.Cron()]
    for root, _, files in self.file_system.Walk(''):
      for f in files:
        futures.append(self.GetContentAndType(Join(root, f)))
        # Also cache the extension-less version of the file if needed.
        base, ext = posixpath.splitext(f)
        if f != SITE_VERIFICATION_FILE and ext in self._default_extensions:
          futures.append(self.GetContentAndType(Join(root, base)))
      # TODO(kalman): Cache .zip files for each directory (if supported).
    return Future(delegate=Gettable(lambda: [f.Get() for f in futures]))

  def __repr__(self):
    return 'ContentProvider of <%s>' % repr(self.file_system)
