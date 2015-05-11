// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_HTML5FS_HTML5_FS_H_
#define LIBRARIES_NACL_IO_HTML5FS_HTML5_FS_H_

#include <pthread.h>

#include "nacl_io/filesystem.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/typed_fs_factory.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

class Node;

class Html5Fs : public Filesystem {
 public:
  virtual Error Access(const Path& path, int a_mode);
  virtual Error Open(const Path& path, int mode, ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int permissions);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

  PP_Resource filesystem_resource() { return filesystem_resource_; }

 protected:
  Html5Fs();

  virtual Error Init(const FsInitArgs& args);
  virtual void Destroy();

  Error BlockUntilFilesystemOpen();

 private:
  static void FilesystemOpenCallbackThunk(void* user_data, int32_t result);
  void FilesystemOpenCallback(int32_t result);

  PP_Resource filesystem_resource_;
  bool filesystem_open_has_result_;  // protected by lock_.
  Error filesystem_open_error_;      // protected by lock_.

  pthread_cond_t filesystem_open_cond_;
  sdk_util::SimpleLock filesysem_open_lock_;

  friend class TypedFsFactory<Html5Fs>;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_HTML5FS_HTML5_FS_H_
