// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Type of a root directory.
 * @enum {string}
 * @const
 */
var RootType = Object.freeze({
  // Root of local directory.
  DOWNLOADS: 'downloads',

  // Root of mounted archive file.
  ARCHIVE: 'archive',

  // Root of removal volume.
  REMOVABLE: 'removable',

  // Root of drive directory.
  DRIVE: 'drive',

  // Root for privet storage volume.
  CLOUD_DEVICE: 'cloud_device',

  // Root for entries that is not located under RootType.DRIVE. e.g. shared
  // files.
  DRIVE_OTHER: 'drive_other',

  // Fake root for offline available files on the drive.
  DRIVE_OFFLINE: 'drive_offline',

  // Fake root for shared files on the drive.
  DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',

  // Fake root for recent files on the drive.
  DRIVE_RECENT: 'drive_recent'
});

var PathUtil = {};

/**
 * Returns the localized name for the root type. If not available, then returns
 * null.
 *
 * @param {RootType} rootType The root type.
 * @return {?string} The localized name, or null if not available.
 */
PathUtil.getRootTypeLabel = function(rootType) {
  var str = function(id) {
    return loadTimeData.getString(id);
  };

  switch (rootType) {
    case RootType.DOWNLOADS:
      return str('DOWNLOADS_DIRECTORY_LABEL');
    case RootType.DRIVE:
      return str('DRIVE_MY_DRIVE_LABEL');
    case RootType.DRIVE_OFFLINE:
      return str('DRIVE_OFFLINE_COLLECTION_LABEL');
    case RootType.DRIVE_SHARED_WITH_ME:
      return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');
    case RootType.DRIVE_RECENT:
      return str('DRIVE_RECENT_COLLECTION_LABEL');
  }

  // Translation not found.
  return null;
};

/**
 * Extracts the extension of the path.
 *
 * Examples:
 * PathUtil.splitExtension('abc.ext') -> ['abc', '.ext']
 * PathUtil.splitExtension('a/b/abc.ext') -> ['a/b/abc', '.ext']
 * PathUtil.splitExtension('a/b') -> ['a/b', '']
 * PathUtil.splitExtension('.cshrc') -> ['', '.cshrc']
 * PathUtil.splitExtension('a/b.backup/hoge') -> ['a/b.backup/hoge', '']
 *
 * @param {string} path Path to be extracted.
 * @return {Array.<string>} Filename and extension of the given path.
 */
PathUtil.splitExtension = function(path) {
  var dotPosition = path.lastIndexOf('.');
  if (dotPosition <= path.lastIndexOf('/'))
    dotPosition = -1;

  var filename = dotPosition != -1 ? path.substr(0, dotPosition) : path;
  var extension = dotPosition != -1 ? path.substr(dotPosition) : '';
  return [filename, extension];
};
