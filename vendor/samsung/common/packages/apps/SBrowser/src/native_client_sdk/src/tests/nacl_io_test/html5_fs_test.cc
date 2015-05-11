// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>

#include <set>
#include <string>

#include <gmock/gmock.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/pp_directory_entry.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#if defined(WIN32)
#include <windows.h>  // For Sleep()
#endif

#include "fake_ppapi/fake_pepper_interface_html5_fs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/html5fs/html5_fs.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osunistd.h"
#include "nacl_io/pepper_interface_delegate.h"
#include "sdk_util/scoped_ref.h"
#include "mock_util.h"
#include "pepper_interface_mock.h"

using namespace nacl_io;
using namespace sdk_util;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace {

class Html5FsForTesting : public Html5Fs {
 public:
  Html5FsForTesting(StringMap_t& string_map, PepperInterface* ppapi) {
    FsInitArgs args;
    args.string_map = string_map;
    args.ppapi = ppapi;
    Error error = Init(args);
    EXPECT_EQ(0, error);
  }
};

class Html5FsTest : public ::testing::Test {
 public:
  Html5FsTest();

 protected:
  FakePepperInterfaceHtml5Fs ppapi_html5_;
  PepperInterfaceMock ppapi_mock_;
  PepperInterfaceDelegate ppapi_;
};

Html5FsTest::Html5FsTest()
    : ppapi_mock_(ppapi_html5_.GetInstance()),
      ppapi_(ppapi_html5_.GetInstance()) {
  // Default delegation to the html5 pepper interface.
  ppapi_.SetCoreInterfaceDelegate(ppapi_html5_.GetCoreInterface());
  ppapi_.SetFileSystemInterfaceDelegate(ppapi_html5_.GetFileSystemInterface());
  ppapi_.SetFileRefInterfaceDelegate(ppapi_html5_.GetFileRefInterface());
  ppapi_.SetFileIoInterfaceDelegate(ppapi_html5_.GetFileIoInterface());
  ppapi_.SetVarInterfaceDelegate(ppapi_html5_.GetVarInterface());
}

}  // namespace

TEST_F(Html5FsTest, FilesystemType) {
  const char* filesystem_type_strings[] = {"", "PERSISTENT", "TEMPORARY", NULL};
  PP_FileSystemType filesystem_type_values[] = {
      PP_FILESYSTEMTYPE_LOCALPERSISTENT,  // Default to persistent.
      PP_FILESYSTEMTYPE_LOCALPERSISTENT, PP_FILESYSTEMTYPE_LOCALTEMPORARY};

  const char* expected_size_strings[] = {"100", "12345", NULL};
  const int expected_size_values[] = {100, 12345};

  FileSystemInterfaceMock* filesystem_mock =
      ppapi_mock_.GetFileSystemInterface();

  FakeFileSystemInterface* filesystem_fake =
      static_cast<FakeFileSystemInterface*>(
          ppapi_html5_.GetFileSystemInterface());

  for (int i = 0; filesystem_type_strings[i] != NULL; ++i) {
    const char* filesystem_type_string = filesystem_type_strings[i];
    PP_FileSystemType expected_filesystem_type = filesystem_type_values[i];

    for (int j = 0; expected_size_strings[j] != NULL; ++j) {
      const char* expected_size_string = expected_size_strings[j];
      int64_t expected_expected_size = expected_size_values[j];

      ppapi_.SetFileSystemInterfaceDelegate(filesystem_mock);

      ON_CALL(*filesystem_mock, Create(_, _)).WillByDefault(
          Invoke(filesystem_fake, &FakeFileSystemInterface::Create));

      EXPECT_CALL(*filesystem_mock,
                  Create(ppapi_.GetInstance(), expected_filesystem_type));

      EXPECT_CALL(*filesystem_mock, Open(_, expected_expected_size, _))
          .WillOnce(DoAll(CallCallback<2>(int32_t(PP_OK)),
                          Return(int32_t(PP_OK_COMPLETIONPENDING))));

      StringMap_t map;
      map["type"] = filesystem_type_string;
      map["expected_size"] = expected_size_string;
      ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

      Mock::VerifyAndClearExpectations(&filesystem_mock);
    }
  }
}

TEST_F(Html5FsTest, Access) {
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddEmptyFile("/foo", NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ASSERT_EQ(0, fs->Access(Path("/foo"), R_OK | W_OK | X_OK));
  ASSERT_EQ(ENOENT, fs->Access(Path("/bar"), F_OK));
}

TEST_F(Html5FsTest, Mkdir) {
  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  // mkdir at the root should return EEXIST, not EACCES.
  EXPECT_EQ(EEXIST, fs->Mkdir(Path("/"), 0644));

  Path path("/foo");
  ASSERT_EQ(ENOENT, fs->Access(path, F_OK));
  ASSERT_EQ(0, fs->Mkdir(path, 0644));

  struct stat stat;
  ScopedNode node;
  ASSERT_EQ(0, fs->Open(path, O_RDONLY, &node));
  EXPECT_EQ(0, node->GetStat(&stat));
  EXPECT_EQ(S_IFDIR, stat.st_mode & S_IFDIR);
}

TEST_F(Html5FsTest, Remove) {
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddEmptyFile("/foo", NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  Path path("/foo");
  ASSERT_EQ(0, fs->Access(path, F_OK));
  ASSERT_EQ(0, fs->Remove(path));
  EXPECT_EQ(ENOENT, fs->Access(path, F_OK));
}

// Unlink + Rmdir forward to Remove unconditionally, which will not fail if the
// file type is wrong.
TEST_F(Html5FsTest, DISABLED_Unlink) {
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddEmptyFile("/file", NULL));
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddDirectory("/dir", NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ASSERT_EQ(EISDIR, fs->Unlink(Path("/dir")));
  EXPECT_EQ(0, fs->Unlink(Path("/file")));
  EXPECT_EQ(ENOENT, fs->Access(Path("/file"), F_OK));
  EXPECT_EQ(0, fs->Access(Path("/dir"), F_OK));
}

// Unlink + Rmdir forward to Remove unconditionally, which will not fail if the
// file type is wrong.
TEST_F(Html5FsTest, DISABLED_Rmdir) {
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddEmptyFile("/file", NULL));
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddDirectory("/dir", NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ASSERT_EQ(ENOTDIR, fs->Rmdir(Path("/file")));
  EXPECT_EQ(0, fs->Rmdir(Path("/dir")));
  EXPECT_EQ(ENOENT, fs->Access(Path("/dir"), F_OK));
  EXPECT_EQ(0, fs->Access(Path("/file"), F_OK));
}

TEST_F(Html5FsTest, Rename) {
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddEmptyFile("/foo", NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  Path path("/foo");
  Path newpath("/bar");
  ASSERT_EQ(0, fs->Access(path, F_OK));
  ASSERT_EQ(0, fs->Rename(path, newpath));
  EXPECT_EQ(ENOENT, fs->Access(path, F_OK));
  EXPECT_EQ(0, fs->Access(newpath, F_OK));
}

TEST_F(Html5FsTest, OpenForCreate) {
  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  Path path("/foo");
  EXPECT_EQ(ENOENT, fs->Access(path, F_OK));

  ScopedNode node;
  ASSERT_EQ(0, fs->Open(path, O_CREAT | O_RDWR, &node));

  // Write some data.
  char contents[] = "contents";
  int bytes_written = 0;
  EXPECT_EQ(0, node->Write(HandleAttr(), &contents[0], strlen(contents),
                           &bytes_written));
  EXPECT_EQ(strlen(contents), bytes_written);

  // Create again.
  ASSERT_EQ(0, fs->Open(path, O_CREAT, &node));

  // Check that the file still has data.
  size_t size;
  EXPECT_EQ(0, node->GetSize(&size));
  EXPECT_EQ(strlen(contents), size);

  // Open exclusively.
  EXPECT_EQ(EEXIST, fs->Open(path, O_CREAT | O_EXCL, &node));

  // Try to truncate without write access.
  EXPECT_EQ(EINVAL, fs->Open(path, O_CREAT | O_TRUNC, &node));

  // Open and truncate.
  ASSERT_EQ(0, fs->Open(path, O_CREAT | O_TRUNC | O_WRONLY, &node));

  // File should be empty.
  EXPECT_EQ(0, node->GetSize(&size));
  EXPECT_EQ(0, size);
}

TEST_F(Html5FsTest, Read) {
  const char contents[] = "contents";
  ASSERT_TRUE(
      ppapi_html5_.filesystem_template()->AddFile("/file", contents, NULL));
  ASSERT_TRUE(ppapi_html5_.filesystem_template()->AddDirectory("/dir", NULL));
  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ScopedNode node;
  ASSERT_EQ(0, fs->Open(Path("/file"), O_RDONLY, &node));

  char buffer[10] = {0};
  int bytes_read = 0;
  HandleAttr attr;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  ASSERT_EQ(strlen(contents), bytes_read);
  ASSERT_STREQ(contents, buffer);

  // Read nothing past the end of the file.
  attr.offs = 100;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  ASSERT_EQ(0, bytes_read);

  // Read part of the data.
  attr.offs = 4;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  ASSERT_EQ(strlen(contents) - 4, bytes_read);
  buffer[bytes_read] = 0;
  ASSERT_STREQ("ents", buffer);

  // Writing should fail.
  int bytes_written = 1;  // Set to a non-zero value.
  attr.offs = 0;
  ASSERT_EQ(EACCES,
            node->Write(attr, &buffer[0], sizeof(buffer), &bytes_written));
  ASSERT_EQ(0, bytes_written);

  // Reading from a directory should fail.
  ASSERT_EQ(0, fs->Open(Path("/dir"), O_RDONLY, &node));
  ASSERT_EQ(EISDIR, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
}

TEST_F(Html5FsTest, Write) {
  const char contents[] = "contents";
  EXPECT_TRUE(
      ppapi_html5_.filesystem_template()->AddFile("/file", contents, NULL));
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddDirectory("/dir", NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ScopedNode node;
  ASSERT_EQ(0, fs->Open(Path("/file"), O_WRONLY, &node));

  // Reading should fail.
  char buffer[10];
  int bytes_read = 1;  // Set to a non-zero value.
  HandleAttr attr;
  EXPECT_EQ(EACCES, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  EXPECT_EQ(0, bytes_read);

  // Reopen as read-write.
  ASSERT_EQ(0, fs->Open(Path("/file"), O_RDWR, &node));

  int bytes_written = 1;  // Set to a non-zero value.
  attr.offs = 3;
  EXPECT_EQ(0, node->Write(attr, "struct", 6, &bytes_written));
  EXPECT_EQ(6, bytes_written);

  attr.offs = 0;
  EXPECT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  EXPECT_EQ(9, bytes_read);
  buffer[bytes_read] = 0;
  EXPECT_STREQ("construct", buffer);

  // Writing to a directory should fail.
  EXPECT_EQ(0, fs->Open(Path("/dir"), O_RDWR, &node));
  EXPECT_EQ(EISDIR, node->Write(attr, &buffer[0], sizeof(buffer), &bytes_read));
}

TEST_F(Html5FsTest, GetStat) {
  const int creation_time = 1000;
  const int access_time = 2000;
  const int modified_time = 3000;
  const char contents[] = "contents";

  // Create fake file.
  FakeHtml5FsNode* fake_node;
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddFile(
      "/file", contents, &fake_node));
  fake_node->set_creation_time(creation_time);
  fake_node->set_last_access_time(access_time);
  fake_node->set_last_modified_time(modified_time);

  // Create fake directory.
  EXPECT_TRUE(
      ppapi_html5_.filesystem_template()->AddDirectory("/dir", &fake_node));
  fake_node->set_creation_time(creation_time);
  fake_node->set_last_access_time(access_time);
  fake_node->set_last_modified_time(modified_time);

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ScopedNode node;
  ASSERT_EQ(0, fs->Open(Path("/file"), O_RDONLY, &node));

  struct stat statbuf;
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(S_IFREG, statbuf.st_mode & S_IFMT);
  EXPECT_EQ(S_IRUSR | S_IRGRP | S_IROTH |
            S_IWUSR | S_IWGRP | S_IWOTH, statbuf.st_mode & ~S_IFMT);
  EXPECT_EQ(strlen(contents), statbuf.st_size);
  EXPECT_EQ(access_time, statbuf.st_atime);
  EXPECT_EQ(creation_time, statbuf.st_ctime);
  EXPECT_EQ(modified_time, statbuf.st_mtime);

  // Test Get* and Isa* methods.
  size_t size;
  EXPECT_EQ(0, node->GetSize(&size));
  EXPECT_EQ(strlen(contents), size);
  EXPECT_FALSE(node->IsaDir());
  EXPECT_TRUE(node->IsaFile());
  EXPECT_FALSE(node->IsaTTY());

  // GetStat on a directory...
  EXPECT_EQ(0, fs->Open(Path("/dir"), O_RDONLY, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(S_IFDIR, statbuf.st_mode & S_IFMT);
  EXPECT_EQ(S_IRUSR | S_IRGRP | S_IROTH |
            S_IWUSR | S_IWGRP | S_IWOTH, statbuf.st_mode & ~S_IFMT);
  EXPECT_EQ(0, statbuf.st_size);
  EXPECT_EQ(access_time, statbuf.st_atime);
  EXPECT_EQ(creation_time, statbuf.st_ctime);
  EXPECT_EQ(modified_time, statbuf.st_mtime);

  // Test Get* and Isa* methods.
  EXPECT_EQ(0, node->GetSize(&size));
  EXPECT_EQ(0, size);
  EXPECT_TRUE(node->IsaDir());
  EXPECT_FALSE(node->IsaFile());
  EXPECT_FALSE(node->IsaTTY());
}

TEST_F(Html5FsTest, FTruncate) {
  const char contents[] = "contents";
  EXPECT_TRUE(
      ppapi_html5_.filesystem_template()->AddFile("/file", contents, NULL));
  EXPECT_TRUE(ppapi_html5_.filesystem_template()->AddDirectory("/dir", NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ScopedNode node;
  ASSERT_EQ(0, fs->Open(Path("/file"), O_RDWR, &node));

  HandleAttr attr;
  char buffer[10] = {0};
  int bytes_read = 0;

  // First make the file shorter...
  EXPECT_EQ(0, node->FTruncate(4));
  EXPECT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  EXPECT_EQ(4, bytes_read);
  buffer[bytes_read] = 0;
  EXPECT_STREQ("cont", buffer);

  // Now make the file longer...
  EXPECT_EQ(0, node->FTruncate(8));
  EXPECT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  EXPECT_EQ(8, bytes_read);
  buffer[bytes_read] = 0;
  EXPECT_STREQ("cont\0\0\0\0", buffer);

  // Ftruncate should fail for a directory.
  EXPECT_EQ(0, fs->Open(Path("/dir"), O_RDONLY, &node));
  EXPECT_EQ(EISDIR, node->FTruncate(4));
}

TEST_F(Html5FsTest, GetDents) {
  const char contents[] = "contents";
  EXPECT_TRUE(
      ppapi_html5_.filesystem_template()->AddFile("/file", contents, NULL));

  StringMap_t map;
  ScopedRef<Html5FsForTesting> fs(new Html5FsForTesting(map, &ppapi_));

  ScopedNode root;
  ASSERT_EQ(0, fs->Open(Path("/"), O_RDONLY, &root));

  ScopedNode node;
  ASSERT_EQ(0, fs->Open(Path("/file"), O_RDWR, &node));

  // Should fail for regular files.
  const size_t kMaxDirents = 5;
  dirent dirents[kMaxDirents];
  int bytes_read = 1;  // Set to a non-zero value.

  memset(&dirents[0], 0, sizeof(dirents));
  EXPECT_EQ(ENOTDIR,
            node->GetDents(0, &dirents[0], sizeof(dirents), &bytes_read));
  EXPECT_EQ(0, bytes_read);

  // Should work with root directory.
  // +2 to test a size that is not a multiple of sizeof(dirent).
  // Expect it to round down.
  memset(&dirents[0], 0, sizeof(dirents));
  EXPECT_EQ(
      0, root->GetDents(0, &dirents[0], sizeof(dirent) * 3 + 2, &bytes_read));

  {
    size_t num_dirents = bytes_read / sizeof(dirent);
    EXPECT_EQ(3, num_dirents);
    EXPECT_EQ(sizeof(dirent) * num_dirents, bytes_read);

    std::multiset<std::string> dirnames;
    for (size_t i = 0; i < num_dirents; ++i) {
      EXPECT_EQ(sizeof(dirent), dirents[i].d_off);
      EXPECT_EQ(sizeof(dirent), dirents[i].d_reclen);
      dirnames.insert(dirents[i].d_name);
    }

    EXPECT_EQ(1, dirnames.count("file"));
    EXPECT_EQ(1, dirnames.count("."));
    EXPECT_EQ(1, dirnames.count(".."));
  }

  // Add another file...
  ASSERT_EQ(0, fs->Open(Path("/file2"), O_CREAT, &node));

  // Read the root directory again.
  memset(&dirents[0], 0, sizeof(dirents));
  EXPECT_EQ(0, root->GetDents(0, &dirents[0], sizeof(dirents), &bytes_read));

  {
    size_t num_dirents = bytes_read / sizeof(dirent);
    EXPECT_EQ(4, num_dirents);
    EXPECT_EQ(sizeof(dirent) * num_dirents, bytes_read);

    std::multiset<std::string> dirnames;
    for (size_t i = 0; i < num_dirents; ++i) {
      EXPECT_EQ(sizeof(dirent), dirents[i].d_off);
      EXPECT_EQ(sizeof(dirent), dirents[i].d_reclen);
      dirnames.insert(dirents[i].d_name);
    }

    EXPECT_EQ(1, dirnames.count("file"));
    EXPECT_EQ(1, dirnames.count("file2"));
    EXPECT_EQ(1, dirnames.count("."));
    EXPECT_EQ(1, dirnames.count(".."));
  }
}
