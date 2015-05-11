// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for utility functions.
 */
var util = {};

/**
 * Returns a function that console.log's its arguments, prefixed by |msg|.
 *
 * @param {string} msg The message prefix to use in the log.
 * @param {function(...string)=} opt_callback A function to invoke after
 *     logging.
 * @return {function(...string)} Function that logs.
 */
util.flog = function(msg, opt_callback) {
  return function() {
    var ary = Array.apply(null, arguments);
    console.log(msg + ': ' + ary.join(', '));
    if (opt_callback)
      opt_callback.apply(null, arguments);
  };
};

/**
 * Returns a function that throws an exception that includes its arguments
 * prefixed by |msg|.
 *
 * @param {string} msg The message prefix to use in the exception.
 * @return {function(...string)} Function that throws.
 */
util.ferr = function(msg) {
  return function() {
    var ary = Array.apply(null, arguments);
    throw new Error(msg + ': ' + ary.join(', '));
  };
};

/**
 * @param {string} name File error name.
 * @return {string} Translated file error string.
 */
util.getFileErrorString = function(name) {
  var candidateMessageFragment;
  switch (name) {
    case 'NotFoundError':
      candidateMessageFragment = 'NOT_FOUND';
      break;
    case 'SecurityError':
      candidateMessageFragment = 'SECURITY';
      break;
    case 'NotReadableError':
      candidateMessageFragment = 'NOT_READABLE';
      break;
    case 'NoModificationAllowedError':
      candidateMessageFragment = 'NO_MODIFICATION_ALLOWED';
      break;
    case 'InvalidStateError':
      candidateMessageFragment = 'INVALID_STATE';
      break;
    case 'InvalidModificationError':
      candidateMessageFragment = 'INVALID_MODIFICATION';
      break;
    case 'PathExistsError':
      candidateMessageFragment = 'PATH_EXISTS';
      break;
    case 'QuotaExceededError':
      candidateMessageFragment = 'QUOTA_EXCEEDED';
      break;
  }

  return loadTimeData.getString('FILE_ERROR_' + candidateMessageFragment) ||
      loadTimeData.getString('FILE_ERROR_GENERIC');
};

/**
 * Mapping table for FileError.code style enum to DOMError.name string.
 *
 * @enum {string}
 * @const
 */
util.FileError = Object.freeze({
  ABORT_ERR: 'AbortError',
  INVALID_MODIFICATION_ERR: 'InvalidModificationError',
  INVALID_STATE_ERR: 'InvalidStateError',
  NO_MODIFICATION_ALLOWED_ERR: 'NoModificationAllowedError',
  NOT_FOUND_ERR: 'NotFoundError',
  NOT_READABLE_ERR: 'NotReadable',
  PATH_EXISTS_ERR: 'PathExistsError',
  QUOTA_EXCEEDED_ERR: 'QuotaExceededError',
  TYPE_MISMATCH_ERR: 'TypeMismatchError',
  ENCODING_ERR: 'EncodingError',
});

/**
 * @param {string} str String to escape.
 * @return {string} Escaped string.
 */
util.htmlEscape = function(str) {
  return str.replace(/[<>&]/g, function(entity) {
    switch (entity) {
      case '<': return '&lt;';
      case '>': return '&gt;';
      case '&': return '&amp;';
    }
  });
};

/**
 * @param {string} str String to unescape.
 * @return {string} Unescaped string.
 */
util.htmlUnescape = function(str) {
  return str.replace(/&(lt|gt|amp);/g, function(entity) {
    switch (entity) {
      case '&lt;': return '<';
      case '&gt;': return '>';
      case '&amp;': return '&';
    }
  });
};

/**
 * Iterates the entries contained by dirEntry, and invokes callback once for
 * each entry. On completion, successCallback will be invoked.
 *
 * @param {DirectoryEntry} dirEntry The entry of the directory.
 * @param {function(Entry, function())} callback Invoked for each entry.
 * @param {function()} successCallback Invoked on completion.
 * @param {function(FileError)} errorCallback Invoked if an error is found on
 *     directory entry reading.
 */
util.forEachDirEntry = function(
    dirEntry, callback, successCallback, errorCallback) {
  var reader = dirEntry.createReader();
  var iterate = function() {
    reader.readEntries(function(entries) {
      if (entries.length == 0) {
        successCallback();
        return;
      }

      AsyncUtil.forEach(
          entries,
          function(forEachCallback, entry) {
            // Do not pass index nor entries.
            callback(entry, forEachCallback);
          },
          iterate);
    }, errorCallback);
  };
  iterate();
};

/**
 * Reads contents of directory.
 * @param {DirectoryEntry} root Root entry.
 * @param {string} path Directory path.
 * @param {function(Array.<Entry>)} callback List of entries passed to callback.
 */
util.readDirectory = function(root, path, callback) {
  var onError = function(e) {
    callback([], e);
  };
  root.getDirectory(path, {create: false}, function(entry) {
    var reader = entry.createReader();
    var r = [];
    var readNext = function() {
      reader.readEntries(function(results) {
        if (results.length == 0) {
          callback(r, null);
          return;
        }
        r.push.apply(r, results);
        readNext();
      }, onError);
    };
    readNext();
  }, onError);
};

/**
 * Utility function to resolve multiple directories with a single call.
 *
 * The successCallback will be invoked once for each directory object
 * found.  The errorCallback will be invoked once for each
 * path that could not be resolved.
 *
 * The successCallback is invoked with a null entry when all paths have
 * been processed.
 *
 * @param {DirEntry} dirEntry The base directory.
 * @param {Object} params The parameters to pass to the underlying
 *     getDirectory calls.
 * @param {Array.<string>} paths The list of directories to resolve.
 * @param {function(!DirEntry)} successCallback The function to invoke for
 *     each DirEntry found.  Also invoked once with null at the end of the
 *     process.
 * @param {function(FileError)} errorCallback The function to invoke
 *     for each path that cannot be resolved.
 */
util.getDirectories = function(dirEntry, params, paths, successCallback,
                               errorCallback) {

  // Copy the params array, since we're going to destroy it.
  params = [].slice.call(params);

  var onComplete = function() {
    successCallback(null);
  };

  var getNextDirectory = function() {
    var path = paths.shift();
    if (!path)
      return onComplete();

    dirEntry.getDirectory(
      path, params,
      function(entry) {
        successCallback(entry);
        getNextDirectory();
      },
      function(err) {
        errorCallback(err);
        getNextDirectory();
      });
  };

  getNextDirectory();
};

/**
 * Utility function to resolve multiple files with a single call.
 *
 * The successCallback will be invoked once for each directory object
 * found.  The errorCallback will be invoked once for each
 * path that could not be resolved.
 *
 * The successCallback is invoked with a null entry when all paths have
 * been processed.
 *
 * @param {DirEntry} dirEntry The base directory.
 * @param {Object} params The parameters to pass to the underlying
 *     getFile calls.
 * @param {Array.<string>} paths The list of files to resolve.
 * @param {function(!FileEntry)} successCallback The function to invoke for
 *     each FileEntry found.  Also invoked once with null at the end of the
 *     process.
 * @param {function(FileError)} errorCallback The function to invoke
 *     for each path that cannot be resolved.
 */
util.getFiles = function(dirEntry, params, paths, successCallback,
                         errorCallback) {
  // Copy the params array, since we're going to destroy it.
  params = [].slice.call(params);

  var onComplete = function() {
    successCallback(null);
  };

  var getNextFile = function() {
    var path = paths.shift();
    if (!path)
      return onComplete();

    dirEntry.getFile(
      path, params,
      function(entry) {
        successCallback(entry);
        getNextFile();
      },
      function(err) {
        errorCallback(err);
        getNextFile();
      });
  };

  getNextFile();
};

/**
 * Resolve a path to either a DirectoryEntry or a FileEntry, regardless of
 * whether the path is a directory or file.
 *
 * @param {DirectoryEntry} root The root of the filesystem to search.
 * @param {string} path The path to be resolved.
 * @param {function(Entry)} resultCallback Called back when a path is
 *     successfully resolved. Entry will be either a DirectoryEntry or
 *     a FileEntry.
 * @param {function(FileError)} errorCallback Called back if an unexpected
 *     error occurs while resolving the path.
 */
util.resolvePath = function(root, path, resultCallback, errorCallback) {
  if (path == '' || path == '/') {
    resultCallback(root);
    return;
  }

  root.getFile(
      path, {create: false},
      resultCallback,
      function(err) {
        if (err.name == util.FileError.TYPE_MISMATCH_ERR) {
          // Bah.  It's a directory, ask again.
          root.getDirectory(
              path, {create: false},
              resultCallback,
              errorCallback);
        } else {
          errorCallback(err);
        }
      });
};

/**
 * Renames the entry to newName.
 * @param {Entry} entry The entry to be renamed.
 * @param {string} newName The new name.
 * @param {function(Entry)} successCallback Callback invoked when the rename
 *     is successfully done.
 * @param {function(FileError)} errorCallback Callback invoked when an error
 *     is found.
 */
util.rename = function(entry, newName, successCallback, errorCallback) {
  entry.getParent(function(parent) {
    // Before moving, we need to check if there is an existing entry at
    // parent/newName, since moveTo will overwrite it.
    // Note that this way has some timing issue. After existing check,
    // a new entry may be create on background. However, there is no way not to
    // overwrite the existing file, unfortunately. The risk should be low,
    // assuming the unsafe period is very short.
    (entry.isFile ? parent.getFile : parent.getDirectory).call(
        parent, newName, {create: false},
        function(entry) {
          // The entry with the name already exists.
          errorCallback(util.createDOMError(util.FileError.PATH_EXISTS_ERR));
        },
        function(error) {
          if (error.name != util.FileError.NOT_FOUND_ERR) {
            // Unexpected error is found.
            errorCallback(error);
            return;
          }

          // No existing entry is found.
          entry.moveTo(parent, newName, successCallback, errorCallback);
        });
  }, errorCallback);
};

/**
 * Remove a file or a directory.
 * @param {Entry} entry The entry to remove.
 * @param {function()} onSuccess The success callback.
 * @param {function(FileError)} onError The error callback.
 */
util.removeFileOrDirectory = function(entry, onSuccess, onError) {
  if (entry.isDirectory)
    entry.removeRecursively(onSuccess, onError);
  else
    entry.remove(onSuccess, onError);
};

/**
 * Checks if an entry exists at |relativePath| in |dirEntry|.
 * If exists, tries to deduplicate the path by inserting parenthesized number,
 * such as " (1)", before the extension. If it still exists, tries the
 * deduplication again by increasing the number up to 10 times.
 * For example, suppose "file.txt" is given, "file.txt", "file (1).txt",
 * "file (2).txt", ..., "file (9).txt" will be tried.
 *
 * @param {DirectoryEntry} dirEntry The target directory entry.
 * @param {string} relativePath The path to be deduplicated.
 * @param {function(string)} onSuccess Called with the deduplicated path on
 *     success.
 * @param {function(FileError)} onError Called on error.
 */
util.deduplicatePath = function(dirEntry, relativePath, onSuccess, onError) {
  // The trial is up to 10.
  var MAX_RETRY = 10;

  // Crack the path into three part. The parenthesized number (if exists) will
  // be replaced by incremented number for retry. For example, suppose
  // |relativePath| is "file (10).txt", the second check path will be
  // "file (11).txt".
  var match = /^(.*?)(?: \((\d+)\))?(\.[^.]*?)?$/.exec(relativePath);
  var prefix = match[1];
  var copyNumber = match[2] ? parseInt(match[2], 10) : 0;
  var ext = match[3] ? match[3] : '';

  // The path currently checking the existence.
  var trialPath = relativePath;

  var onNotResolved = function(err) {
    // We expect to be unable to resolve the target file, since we're going
    // to create it during the copy.  However, if the resolve fails with
    // anything other than NOT_FOUND, that's trouble.
    if (err.name != util.FileError.NOT_FOUND_ERR) {
      onError(err);
      return;
    }

    // Found a path that doesn't exist.
    onSuccess(trialPath);
  };

  var numRetry = MAX_RETRY;
  var onResolved = function(entry) {
    if (--numRetry == 0) {
      // Hit the limit of the number of retrial.
      // Note that we cannot create FileError object directly, so here we use
      // Object.create instead.
      onError(util.createDOMError(util.FileError.PATH_EXISTS_ERR));
      return;
    }

    ++copyNumber;
    trialPath = prefix + ' (' + copyNumber + ')' + ext;
    util.resolvePath(dirEntry, trialPath, onResolved, onNotResolved);
  };

  // Check to see if the target exists.
  util.resolvePath(dirEntry, trialPath, onResolved, onNotResolved);
};

/**
 * Convert a number of bytes into a human friendly format, using the correct
 * number separators.
 *
 * @param {number} bytes The number of bytes.
 * @return {string} Localized string.
 */
util.bytesToString = function(bytes) {
  // Translation identifiers for size units.
  var UNITS = ['SIZE_BYTES',
               'SIZE_KB',
               'SIZE_MB',
               'SIZE_GB',
               'SIZE_TB',
               'SIZE_PB'];

  // Minimum values for the units above.
  var STEPS = [0,
               Math.pow(2, 10),
               Math.pow(2, 20),
               Math.pow(2, 30),
               Math.pow(2, 40),
               Math.pow(2, 50)];

  var str = function(n, u) {
    // TODO(rginda): Switch to v8Locale's number formatter when it's
    // available.
    return strf(u, n.toLocaleString());
  };

  var fmt = function(s, u) {
    var rounded = Math.round(bytes / s * 10) / 10;
    return str(rounded, u);
  };

  // Less than 1KB is displayed like '80 bytes'.
  if (bytes < STEPS[1]) {
    return str(bytes, UNITS[0]);
  }

  // Up to 1MB is displayed as rounded up number of KBs.
  if (bytes < STEPS[2]) {
    var rounded = Math.ceil(bytes / STEPS[1]);
    return str(rounded, UNITS[1]);
  }

  // This loop index is used outside the loop if it turns out |bytes|
  // requires the largest unit.
  var i;

  for (i = 2 /* MB */; i < UNITS.length - 1; i++) {
    if (bytes < STEPS[i + 1])
      return fmt(STEPS[i], UNITS[i]);
  }

  return fmt(STEPS[i], UNITS[i]);
};

/**
 * Utility function to read specified range of bytes from file
 * @param {File} file The file to read.
 * @param {number} begin Starting byte(included).
 * @param {number} end Last byte(excluded).
 * @param {function(File, Uint8Array)} callback Callback to invoke.
 * @param {function(FileError)} onError Error handler.
 */
util.readFileBytes = function(file, begin, end, callback, onError) {
  var fileReader = new FileReader();
  fileReader.onerror = onError;
  fileReader.onloadend = function() {
    callback(file, new ByteReader(fileReader.result));
  };
  fileReader.readAsArrayBuffer(file.slice(begin, end));
};

/**
 * Write a blob to a file.
 * Truncates the file first, so the previous content is fully overwritten.
 * @param {FileEntry} entry File entry.
 * @param {Blob} blob The blob to write.
 * @param {function(Event)} onSuccess Completion callback. The first argument is
 *     a 'writeend' event.
 * @param {function(FileError)} onError Error handler.
 */
util.writeBlobToFile = function(entry, blob, onSuccess, onError) {
  var truncate = function(writer) {
    writer.onerror = onError;
    writer.onwriteend = write.bind(null, writer);
    writer.truncate(0);
  };

  var write = function(writer) {
    writer.onwriteend = onSuccess;
    writer.write(blob);
  };

  entry.createWriter(truncate, onError);
};

/**
 * Returns a string '[Ctrl-][Alt-][Shift-][Meta-]' depending on the event
 * modifiers. Convenient for writing out conditions in keyboard handlers.
 *
 * @param {Event} event The keyboard event.
 * @return {string} Modifiers.
 */
util.getKeyModifiers = function(event) {
  return (event.ctrlKey ? 'Ctrl-' : '') +
         (event.altKey ? 'Alt-' : '') +
         (event.shiftKey ? 'Shift-' : '') +
         (event.metaKey ? 'Meta-' : '');
};

/**
 * @param {HTMLElement} element Element to transform.
 * @param {Object} transform Transform object,
 *                           contains scaleX, scaleY and rotate90 properties.
 */
util.applyTransform = function(element, transform) {
  element.style.webkitTransform =
      transform ? 'scaleX(' + transform.scaleX + ') ' +
                  'scaleY(' + transform.scaleY + ') ' +
                  'rotate(' + transform.rotate90 * 90 + 'deg)' :
      '';
};

/**
 * Makes filesystem: URL from the path.
 * @param {string} path File or directory path.
 * @return {string} URL.
 */
util.makeFilesystemUrl = function(path) {
  path = path.split('/').map(encodeURIComponent).join('/');
  var prefix = 'external';
  return 'filesystem:' + chrome.runtime.getURL(prefix + path);
};

/**
 * Extracts path from filesystem: URL.
 * @param {string} url Filesystem URL.
 * @return {string} The path.
 */
util.extractFilePath = function(url) {
  var match =
      /^filesystem:[\w-]*:\/\/[\w]*\/(external|persistent|temporary)(\/.*)$/.
      exec(url);
  var path = match && match[2];
  if (!path) return null;
  return decodeURIComponent(path);
};

/**
 * Traverses a directory tree whose root is the given entry, and invokes
 * callback for each entry. Upon completion, successCallback will be called.
 * On error, errorCallback will be called.
 *
 * @param {Entry} entry The root entry.
 * @param {function(Entry):boolean} callback Callback invoked for each entry.
 *     If this returns false, entries under it won't be traversed. Note that
 *     its siblings (and their children) will be still traversed.
 * @param {function()} successCallback Called upon successful completion.
 * @param {function(error)} errorCallback Called upon error.
 */
util.traverseTree = function(entry, callback, successCallback, errorCallback) {
  if (!callback(entry)) {
    successCallback();
    return;
  }

  util.forEachDirEntry(
      entry,
      function(child, iterationCallback) {
        util.traverseTree(child, callback, iterationCallback, errorCallback);
      },
      successCallback,
      errorCallback);
};

/**
 * A shortcut function to create a child element with given tag and class.
 *
 * @param {HTMLElement} parent Parent element.
 * @param {string=} opt_className Class name.
 * @param {string=} opt_tag Element tag, DIV is omitted.
 * @return {Element} Newly created element.
 */
util.createChild = function(parent, opt_className, opt_tag) {
  var child = parent.ownerDocument.createElement(opt_tag || 'div');
  if (opt_className)
    child.className = opt_className;
  parent.appendChild(child);
  return child;
};

/**
 * Updates the app state.
 *
 * @param {string} currentDirectoryURL Currently opened directory as an URL.
 *     If null the value is left unchanged.
 * @param {string} selectionURL Currently selected entry as an URL. If null the
 *     value is left unchanged.
 * @param {string|Object=} opt_param Additional parameters, to be stored. If
 *     null, then left unchanged.
 */
util.updateAppState = function(currentDirectoryURL, selectionURL, opt_param) {
  window.appState = window.appState || {};
  if (opt_param !== undefined && opt_param !== null)
    window.appState.params = opt_param;
  if (currentDirectoryURL !== null)
    window.appState.currentDirectoryURL = currentDirectoryURL;
  if (selectionURL !== null)
    window.appState.selectionURL = selectionURL;
  util.saveAppState();
};

/**
 * Returns a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getString(id).
 *
 * @param {string} id The id of the string to return.
 * @return {string} The translated string.
 */
function str(id) {
  return loadTimeData.getString(id);
}

/**
 * Returns a translated string with arguments replaced.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getStringF(id, ...).
 *
 * @param {string} id The id of the string to return.
 * @param {...string} var_args The values to replace into the string.
 * @return {string} The translated string with replaced values.
 */
function strf(id, var_args) {
  return loadTimeData.getStringF.apply(loadTimeData, arguments);
}

/**
 * Adapter object that abstracts away the the difference between Chrome app APIs
 * v1 and v2. Is only necessary while the migration to v2 APIs is in progress.
 * TODO(mtomasz): Clean up this. crbug.com/240606.
 */
util.platform = {
  /**
   * @return {boolean} True if Files.app is running as an open files or a select
   *     folder dialog. False otherwise.
   */
  runningInBrowser: function() {
    return !window.appID;
  },

  /**
   * @param {function(Object)} callback Function accepting a preference map.
   */
  getPreferences: function(callback) {
    chrome.storage.local.get(callback);
  },

  /**
   * @param {string} key Preference name.
   * @param {function(string)} callback Function accepting the preference value.
   */
  getPreference: function(key, callback) {
    chrome.storage.local.get(key, function(items) {
      callback(items[key]);
    });
  },

  /**
   * @param {string} key Preference name.
   * @param {string|Object} value Preference value.
   * @param {function()=} opt_callback Completion callback.
   */
  setPreference: function(key, value, opt_callback) {
    if (typeof value != 'string')
      value = JSON.stringify(value);

    var items = {};
    items[key] = value;
    chrome.storage.local.set(items, opt_callback);
  }
};

/**
 * Attach page load handler.
 * @param {function()} handler Application-specific load handler.
 */
util.addPageLoadHandler = function(handler) {
  document.addEventListener('DOMContentLoaded', function() {
    handler();
  });
};

/**
 * Save app launch data to the local storage.
 */
util.saveAppState = function() {
  if (window.appState)
    util.platform.setPreference(window.appID, window.appState);
};

/**
 *  AppCache is a persistent timestamped key-value storage backed by
 *  HTML5 local storage.
 *
 *  It is not designed for frequent access. In order to avoid costly
 *  localStorage iteration all data is kept in a single localStorage item.
 *  There is no in-memory caching, so concurrent access is _almost_ safe.
 *
 *  TODO(kaznacheev) Reimplement this based on Indexed DB.
 */
util.AppCache = function() {};

/**
 * Local storage key.
 */
util.AppCache.KEY = 'AppCache';

/**
 * Max number of items.
 */
util.AppCache.CAPACITY = 100;

/**
 * Default lifetime.
 */
util.AppCache.LIFETIME = 30 * 24 * 60 * 60 * 1000;  // 30 days.

/**
 * @param {string} key Key.
 * @param {function(number)} callback Callback accepting a value.
 */
util.AppCache.getValue = function(key, callback) {
  util.AppCache.read_(function(map) {
    var entry = map[key];
    callback(entry && entry.value);
  });
};

/**
 * Update the cache.
 *
 * @param {string} key Key.
 * @param {string} value Value. Remove the key if value is null.
 * @param {number=} opt_lifetime Maximum time to keep an item (in milliseconds).
 */
util.AppCache.update = function(key, value, opt_lifetime) {
  util.AppCache.read_(function(map) {
    if (value != null) {
      map[key] = {
        value: value,
        expire: Date.now() + (opt_lifetime || util.AppCache.LIFETIME)
      };
    } else if (key in map) {
      delete map[key];
    } else {
      return;  // Nothing to do.
    }
    util.AppCache.cleanup_(map);
    util.AppCache.write_(map);
  });
};

/**
 * @param {function(Object)} callback Callback accepting a map of timestamped
 *   key-value pairs.
 * @private
 */
util.AppCache.read_ = function(callback) {
  util.platform.getPreference(util.AppCache.KEY, function(json) {
    if (json) {
      try {
        callback(JSON.parse(json));
      } catch (e) {
        // The local storage item somehow got messed up, start fresh.
      }
    }
    callback({});
  });
};

/**
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
util.AppCache.write_ = function(map) {
  util.platform.setPreference(util.AppCache.KEY, JSON.stringify(map));
};

/**
 * Remove over-capacity and obsolete items.
 *
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
util.AppCache.cleanup_ = function(map) {
  // Sort keys by ascending timestamps.
  var keys = [];
  for (var key in map) {
    if (map.hasOwnProperty(key))
      keys.push(key);
  }
  keys.sort(function(a, b) { return map[a].expire > map[b].expire });

  var cutoff = Date.now();

  var obsolete = 0;
  while (obsolete < keys.length &&
         map[keys[obsolete]].expire < cutoff) {
    obsolete++;
  }

  var overCapacity = Math.max(0, keys.length - util.AppCache.CAPACITY);

  var itemsToDelete = Math.max(obsolete, overCapacity);
  for (var i = 0; i != itemsToDelete; i++) {
    delete map[keys[i]];
  }
};

/**
 * Load an image.
 *
 * @param {Image} image Image element.
 * @param {string} url Source url.
 * @param {Object=} opt_options Hash array of options, eg. width, height,
 *     maxWidth, maxHeight, scale, cache.
 * @param {function()=} opt_isValid Function returning false iff the task
 *     is not valid and should be aborted.
 * @return {?number} Task identifier or null if fetched immediately from
 *     cache.
 */
util.loadImage = function(image, url, opt_options, opt_isValid) {
  return ImageLoaderClient.loadToImage(url,
                                      image,
                                      opt_options || {},
                                      function() {},
                                      function() { image.onerror(); },
                                      opt_isValid);
};

/**
 * Cancels loading an image.
 * @param {number} taskId Task identifier returned by util.loadImage().
 */
util.cancelLoadImage = function(taskId) {
  ImageLoaderClient.getInstance().cancel(taskId);
};

/**
 * Finds proerty descriptor in the object prototype chain.
 * @param {Object} object The object.
 * @param {string} propertyName The property name.
 * @return {Object} Property descriptor.
 */
util.findPropertyDescriptor = function(object, propertyName) {
  for (var p = object; p; p = Object.getPrototypeOf(p)) {
    var d = Object.getOwnPropertyDescriptor(p, propertyName);
    if (d)
      return d;
  }
  return null;
};

/**
 * Calls inherited property setter (useful when property is
 * overriden).
 * @param {Object} object The object.
 * @param {string} propertyName The property name.
 * @param {*} value Value to set.
 */
util.callInheritedSetter = function(object, propertyName, value) {
  var d = util.findPropertyDescriptor(Object.getPrototypeOf(object),
                                      propertyName);
  d.set.call(object, value);
};

/**
 * Returns true if the board of the device matches the given prefix.
 * @param {string} boardPrefix The board prefix to match against.
 *     (ex. "x86-mario". Prefix is used as the actual board name comes with
 *     suffix like "x86-mario-something".
 * @return {boolean} True if the board of the device matches the given prefix.
 */
util.boardIs = function(boardPrefix) {
  // The board name should be lower-cased, but making it case-insensitive for
  // backward compatibility just in case.
  var board = str('CHROMEOS_RELEASE_BOARD');
  var pattern = new RegExp('^' + boardPrefix, 'i');
  return board.match(pattern) != null;
};

/**
 * Adds an isFocused method to the current window object.
 */
util.addIsFocusedMethod = function() {
  var focused = true;

  window.addEventListener('focus', function() {
    focused = true;
  });

  window.addEventListener('blur', function() {
    focused = false;
  });

  /**
   * @return {boolean} True if focused.
   */
  window.isFocused = function() {
    return focused;
  };
};

/**
 * Makes a redirect to the specified Files.app's window from another window.
 * @param {number} id Window id.
 * @param {string} url Target url.
 * @return {boolean} True if the window has been found. False otherwise.
 */
util.redirectMainWindow = function(id, url) {
  // TODO(mtomasz): Implement this for Apps V2, once the photo importer is
  // restored.
  return false;
};

/**
 * Checks, if the Files.app's window is in a full screen mode.
 *
 * @param {AppWindow} appWindow App window to be maximized.
 * @return {boolean} True if the full screen mode is enabled.
 */
util.isFullScreen = function(appWindow) {
  if (appWindow) {
    return appWindow.isFullscreen();
  } else {
    console.error('App window not passed. Unable to check status of ' +
                  'the full screen mode.');
    return false;
  }
};

/**
 * Toggles the full screen mode.
 *
 * @param {AppWindow} appWindow App window to be maximized.
 * @param {boolean} enabled True for enabling, false for disabling.
 */
util.toggleFullScreen = function(appWindow, enabled) {
  if (appWindow) {
    if (enabled)
      appWindow.fullscreen();
    else
      appWindow.restore();
    return;
  }

  console.error(
      'App window not passed. Unable to toggle the full screen mode.');
};

/**
 * The type of a file operation.
 * @enum {string}
 * @const
 */
util.FileOperationType = Object.freeze({
  COPY: 'COPY',
  MOVE: 'MOVE',
  ZIP: 'ZIP',
});

/**
 * The type of a file operation error.
 * @enum {number}
 * @const
 */
util.FileOperationErrorType = Object.freeze({
  UNEXPECTED_SOURCE_FILE: 0,
  TARGET_EXISTS: 1,
  FILESYSTEM_ERROR: 2,
});

/**
 * The kind of an entry changed event.
 * @enum {number}
 * @const
 */
util.EntryChangedKind = Object.freeze({
  CREATED: 0,
  DELETED: 1,
});

/**
 * Obtains whether an entry is fake or not.
 * @param {!Entry|!Object} entry Entry or a fake entry.
 * @return {boolean} True if the given entry is fake.
 */
util.isFakeEntry = function(entry) {
  return !('getParent' in entry);
};

/**
 * Creates an instance of UserDOMError with given error name that looks like a
 * FileError except that it does not have the deprecated FileError.code member.
 *
 * TODO(uekawa): remove reference to FileError.
 *
 * @param {string} name Error name for the file error.
 * @return {UserDOMError} FileError instance
 */
util.createDOMError = function(name) {
  return new util.UserDOMError(name);
};

/**
 * Creates a DOMError-like object to be used in place of returning file errors.
 *
 * @param {string} name Error name for the file error.
 * @constructor
 */
util.UserDOMError = function(name) {
  /**
   * @type {string}
   * @private
   */
  this.name_ = name;
  Object.freeze(this);
};

util.UserDOMError.prototype = {
  /**
   * @return {string} File error name.
   */
  get name() {
    return this.name_;
  }
};

/**
 * Compares two entries.
 * @param {Entry|Object} entry1 The entry to be compared. Can be a fake.
 * @param {Entry|Object} entry2 The entry to be compared. Can be a fake.
 * @return {boolean} True if the both entry represents a same file or
 *     directory. Returns true if both entries are null.
 */
util.isSameEntry = function(entry1, entry2) {
  // Currently, we can assume there is only one root.
  // When we support multi-file system, we need to look at filesystem, too.
  return (entry1 && entry2 && entry1.toURL() === entry2.toURL()) ||
      (!entry1 && !entry2);
};

/**
 * Checks if the child entry is a descendant of another entry. If the entries
 * point to the same file or directory, then returns false.
 *
 * @param {DirectoryEntry|Object} ancestorEntry The ancestor directory entry.
 *     Can be a fake.
 * @param {Entry|Object} childEntry The child entry. Can be a fake.
 * @return {boolean} True if the child entry is contained in the ancestor path.
 */
util.isDescendantEntry = function(ancestorEntry, childEntry) {
  if (!ancestorEntry.isDirectory)
    return false;

  // TODO(mtomasz): Do not work on URLs. Instead consider comparing file systems
  // and paths.
  if (util.isSameEntry(ancestorEntry, childEntry))
    return false;
  if (childEntry.toURL().indexOf(ancestorEntry.toURL() + '/') !== 0)
    return false;

  return true;
};

/**
 * Visit the URL.
 *
 * If the browser is opening, the url is opened in a new tag, otherwise the url
 * is opened in a new window.
 *
 * @param {string} url URL to visit.
 */
util.visitURL = function(url) {
  window.open(url);
};

/**
 * Returns normalized current locale, or default locale - 'en'.
 * @return {string} Current locale
 */
util.getCurrentLocaleOrDefault = function() {
  // chrome.i18n.getMessage('@@ui_locale') can't be used in packed app.
  // Instead, we pass it from C++-side with strings.
  return str('UI_LOCALE') || 'en';
};

/**
 * Converts array of entries to an array of corresponding URLs.
 * @param {Array.<Entry>} entries Input array of entries.
 * @return {Array.<string>} Output array of URLs.
 */
util.entriesToURLs = function(entries) {
  // TODO(mtomasz): Make all callers use entries instead of URLs, and then
  // remove this utility function.
  console.warn('Converting entries to URLs is deprecated.');
  return entries.map(function(entry) {
     return entry.toURL();
  });
};

/**
 * Converts array of URLs to an array of corresponding Entries.
 *
 * @param {Array.<string>} urls Input array of URLs.
 * @param {function(Array.<Entry>, Array.<URL>)} callback Completion callback
 *     with array of success Entries and failure URLs.
 */
util.URLsToEntries = function(urls, callback) {
  var result = [];
  var failureUrl = [];
  AsyncUtil.forEach(
      urls,
      function(forEachCallback, url) {
        webkitResolveLocalFileSystemURL(url, function(entry) {
          result.push(entry);
          forEachCallback();
        }, function() {
          // Not an error. Possibly, the file is not accessible anymore.
          console.warn('Failed to resolve the file with url: ' + url + '.');
          failureUrl.push(url);
          forEachCallback();
        });
      },
      callback.bind(null, result, failureUrl));
};

/**
 * Returns whether the window is teleported or not.
 * @param {DOMWindow} window Window.
 * @return {Promise.<boolean>} Whether the window is teleported or not.
 */
util.isTeleported = function(window) {
  return new Promise(function(onFulfilled) {
    window.chrome.fileBrowserPrivate.getProfiles(function(profiles,
                                                          currentId,
                                                          displayedId) {
      onFullfilled(currentId !== displayedId);
    });
  });
};

/**
 * Sets up and shows the alert to inform a user the task is opened in the
 * desktop of the running profile.
 *
 * TODO(hirono): Move the function from the util namespace.
 * @param {cr.ui.AlertDialog} alertDialog Alert dialog to be shown.
 * @param {Array.<Entry>} entries List of opened entries.
 */
util.showOpenInOtherDesktopAlert = function(alertDialog, entries) {
  if (!entries.length)
    return;
  chrome.fileBrowserPrivate.getProfiles(function(profiles,
                                                 currentId,
                                                 displayedId) {
    // Find strings.
    var displayName;
    for (var i = 0; i < profiles.length; i++) {
      if (profiles[i].profileId === currentId) {
        displayName = profiles[i].displayName;
        break;
      }
    }
    if (!displayName) {
      console.warn('Display name is not found.');
      return;
    }

    var title = entries.size > 1 ?
        entries[0].name + '\u2026' /* ellipsis */ : entries[0].name;
    var message = strf(entries.size > 1 ?
                       'OPEN_IN_OTHER_DESKTOP_MESSAGE_PLURAL' :
                       'OPEN_IN_OTHER_DESKTOP_MESSAGE',
                       displayName,
                       currentId);

    // Show the dialog.
    alertDialog.showWithTitle(title, message);
  }.bind(this));
};

/**
 * Error type of VolumeManager.
 * @enum {string}
 * @const
 */
util.VolumeError = Object.freeze({
  /* Internal errors */
  NOT_MOUNTED: 'not_mounted',
  TIMEOUT: 'timeout',

  /* System events */
  UNKNOWN: 'error_unknown',
  INTERNAL: 'error_internal',
  UNKNOWN_FILESYSTEM: 'error_unknown_filesystem',
  UNSUPPORTED_FILESYSTEM: 'error_unsupported_filesystem',
  INVALID_ARCHIVE: 'error_invalid_archive',
  AUTHENTICATION: 'error_authentication',
  PATH_UNMOUNTED: 'error_path_unmounted'
});

/**
 * List of connection types of drive.
 *
 * Keep this in sync with the kDriveConnectionType* constants in
 * private_api_dirve.cc.
 *
 * @enum {string}
 * @const
 */
util.DriveConnectionType = Object.freeze({
  OFFLINE: 'offline',  // Connection is offline or drive is unavailable.
  METERED: 'metered',  // Connection is metered. Should limit traffic.
  ONLINE: 'online'     // Connection is online.
});

/**
 * List of reasons of DriveConnectionType.
 *
 * Keep this in sync with the kDriveConnectionReason constants in
 * private_api_drive.cc.
 *
 * @enum {string}
 * @const
 */
util.DriveConnectionReason = Object.freeze({
  NOT_READY: 'not_ready',    // Drive is not ready or authentication is failed.
  NO_NETWORK: 'no_network',  // Network connection is unavailable.
  NO_SERVICE: 'no_service'   // Drive service is unavailable.
});

/**
 * The type of each volume.
 * @enum {string}
 * @const
 */
util.VolumeType = Object.freeze({
  DRIVE: 'drive',
  DOWNLOADS: 'downloads',
  REMOVABLE: 'removable',
  ARCHIVE: 'archive',
  CLOUD_DEVICE: 'cloud_device'
});
