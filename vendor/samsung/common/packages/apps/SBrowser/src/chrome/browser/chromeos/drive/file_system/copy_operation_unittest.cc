// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include "base/file_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class CopyOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
   OperationTestBase::SetUp();
   operation_.reset(new CopyOperation(
       blocking_task_runner(),
       observer(),
       scheduler(),
       metadata(),
       cache(),
       util::GetIdentityResourceIdCanonicalizer()));
  }

  scoped_ptr<CopyOperation> operation_;
};

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_RegularFile) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.txt");
  const base::FilePath remote_dest_path(
      FILE_PATH_LITERAL("drive/root/remote.txt"));

  // Prepare a local file.
  ASSERT_TRUE(
      google_apis::test_util::WriteStringToFile(local_src_path, "hello"));
  // Confirm that the remote file does not exist.
  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // TransferFileFromLocalToRemote stores a copy of the local file in the cache,
  // marks it dirty and requests the observer to upload the file.
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));
  EXPECT_EQ(1U, observer()->updated_local_ids().count(
      GetLocalId(remote_dest_path)));
  FileCacheEntry cache_entry;
  bool found = false;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 GetLocalId(remote_dest_path),
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&found));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(found);
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_entry.is_dirty());

  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(
      remote_dest_path.DirName()));
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_Overwrite) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.txt");
  const base::FilePath remote_dest_path(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));

  // Prepare a local file.
  EXPECT_TRUE(
      google_apis::test_util::WriteStringToFile(local_src_path, "hello"));
  // Confirm that the remote file exists.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // TransferFileFromLocalToRemote stores a copy of the local file in the cache,
  // marks it dirty and requests the observer to upload the file.
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));
  EXPECT_EQ(1U, observer()->updated_local_ids().count(entry.local_id()));
  FileCacheEntry cache_entry;
  bool found = false;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()), entry.local_id(), &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&found));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(found);
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_entry.is_dirty());

  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(
      remote_dest_path.DirName()));
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_HostedDocument) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.gdoc");
  const base::FilePath remote_dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/Document 1 excludeDir-test.gdoc"));

  // Prepare a local file, which is a json file of a hosted document, which
  // matches "Document 1" in root_feed.json.
  ASSERT_TRUE(util::CreateGDocFile(
      local_src_path,
      GURL("https://3_document_self_link/document:5_document_resource_id"),
      "document:5_document_resource_id"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));

  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_TRUE(
      observer()->get_changed_paths().count(remote_dest_path.DirName()));
}


TEST_F(CopyOperationTest, CopyNotExistingFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/Dummy file.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Test.log"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

TEST_F(CopyOperationTest, CopyFileToNonExistingDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Dummy/Test.log"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path.DirName(), &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

// Test the case where the parent of the destination path is an existing file,
// not a directory.
TEST_F(CopyOperationTest, CopyFileToInvalidPath) {
  base::FilePath src_path(FILE_PATH_LITERAL(
      "drive/root/Document 1 excludeDir-test.gdoc"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Duplicate Name.txt/Document 1 excludeDir-test.gdoc"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path.DirName(), &entry));
  ASSERT_FALSE(entry.file_info().is_directory());

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

TEST_F(CopyOperationTest, CopyDirtyFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/New File.txt"));

  ResourceEntry src_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));

  // Store a dirty cache file.
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir(), &temp_file));
  std::string contents = "test content";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(temp_file, contents));
  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::Store,
                 base::Unretained(cache()),
                 src_entry.local_id(),
                 std::string(),
                 temp_file,
                 internal::FileCache::FILE_OPERATION_MOVE),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Copy.
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ(ResourceEntry::DIRTY, dest_entry.metadata_edit_state());

  EXPECT_EQ(1u, observer()->updated_local_ids().size());
  EXPECT_TRUE(observer()->updated_local_ids().count(dest_entry.local_id()));
  EXPECT_EQ(1u, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(dest_path.DirName()));

  // Copied cache file should be dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 dest_entry.local_id(),
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);
  EXPECT_TRUE(cache_entry.is_dirty());

  // File contents should match.
  base::FilePath cache_file_path;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetFile,
                 base::Unretained(cache()),
                 dest_entry.local_id(),
                 &cache_file_path),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  std::string copied_contents;
  EXPECT_TRUE(base::ReadFileToString(cache_file_path, &copied_contents));
  EXPECT_EQ(contents, copied_contents);
}

TEST_F(CopyOperationTest, CopyFileOverwriteFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/SubDirectory File 1.txt"));

  ResourceEntry old_dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &old_dest_entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry new_dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &new_dest_entry));

  EXPECT_EQ(1u, observer()->updated_local_ids().size());
  EXPECT_TRUE(observer()->updated_local_ids().count(old_dest_entry.local_id()));
  EXPECT_EQ(1u, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(dest_path.DirName()));
}

TEST_F(CopyOperationTest, CopyFileOverwriteDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Directory 1"));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_INVALID_OPERATION, error);
}

TEST_F(CopyOperationTest, CopyDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/Directory 1"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/New Directory"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_TRUE(entry.file_info().is_directory());
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path.DirName(), &entry));
  ASSERT_TRUE(entry.file_info().is_directory());

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   false,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_FILE, error);
}

TEST_F(CopyOperationTest, PreserveLastModified) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/File 2.txt"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_OK,
            GetLocalResourceEntry(dest_path.DirName(), &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   true,  // Preserve last modified.
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  ResourceEntry entry2;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &entry2));
  EXPECT_EQ(entry.file_info().last_modified(),
            entry2.file_info().last_modified());
}

}  // namespace file_system
}  // namespace drive
