// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_DEVFS_DEV_FS_H_
#define LIBRARIES_NACL_IO_DEVFS_DEV_FS_H_

#include "nacl_io/filesystem.h"
#include "nacl_io/typed_fs_factory.h"

namespace nacl_io {

class Node;

class DevFs : public Filesystem {
 public:
  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path, int open_flags, ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int permissions);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

 protected:
  DevFs();

  virtual Error Init(const FsInitArgs& args);

 private:
  ScopedNode root_;

  friend class TypedFsFactory<DevFs>;
  DISALLOW_COPY_AND_ASSIGN(DevFs);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_DEVFS_DEV_FS_H_
