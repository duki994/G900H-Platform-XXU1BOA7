// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_
#define TESTS_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_

#include "fake_ppapi/fake_pepper_interface.h"
#include "gmock/gmock.h"
#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/filesystem.h"

#define NULL_NODE ((Node*)NULL)

class DevFsForTesting : public nacl_io::DevFs {
 public:
  DevFsForTesting() {
    nacl_io::FsInitArgs args(1);
    args.ppapi = &pepper_;
    Init(args);
  }

  int num_nodes() { return (int)inode_pool_.size(); }
 private:
  FakePepperInterface pepper_;
};

#endif  // TESTS_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_
