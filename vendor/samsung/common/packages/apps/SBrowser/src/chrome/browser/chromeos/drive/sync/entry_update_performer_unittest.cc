// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/entry_update_performer.h"

#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

class EntryUpdatePerformerTest : public file_system::OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    OperationTestBase::SetUp();
    performer_.reset(new EntryUpdatePerformer(blocking_task_runner(),
                                              observer(),
                                              scheduler(),
                                              metadata(),
                                              cache(),
                                              loader_controller()));
  }

  // Stores |content| to the cache and mark it as dirty.
  FileError StoreAndMarkDirty(const std::string& local_id,
                              const std::string& content) {
    base::FilePath path;
    if (!base::CreateTemporaryFileInDir(temp_dir(), &path) ||
        !google_apis::test_util::WriteStringToFile(path, content))
      return FILE_ERROR_FAILED;

    // Store the file to cache.
    FileError error = FILE_ERROR_FAILED;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner(),
        FROM_HERE,
        base::Bind(&FileCache::Store,
                   base::Unretained(cache()),
                   local_id, std::string(), path,
                   FileCache::FILE_OPERATION_COPY),
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();
    return error;
  }

  scoped_ptr<EntryUpdatePerformer> performer_;
};

TEST_F(EntryUpdatePerformerTest, UpdateEntry) {
  base::FilePath src_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/Sub Directory Folder"));

  ResourceEntry src_entry, dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));

  // Update local entry.
  base::Time new_last_modified = base::Time::FromInternalValue(
      src_entry.file_info().last_modified()) + base::TimeDelta::FromSeconds(1);
  base::Time new_last_accessed = base::Time::FromInternalValue(
      src_entry.file_info().last_accessed()) + base::TimeDelta::FromSeconds(2);

  src_entry.set_parent_local_id(dest_entry.local_id());
  src_entry.set_title("Moved" + src_entry.title());
  src_entry.mutable_file_info()->set_last_modified(
      new_last_modified.ToInternalValue());
  src_entry.mutable_file_info()->set_last_accessed(
      new_last_accessed.ToInternalValue());
  src_entry.set_metadata_edit_state(ResourceEntry::DIRTY);

  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::RefreshEntry,
                 base::Unretained(metadata()),
                 src_entry),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Perform server side update.
  error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      src_entry.local_id(),
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Verify the file is updated on the server.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> gdata_entry;
  fake_service()->GetResourceEntry(
      src_entry.resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  ASSERT_TRUE(gdata_entry);

  EXPECT_EQ(src_entry.title(), gdata_entry->title());
  EXPECT_EQ(new_last_modified, gdata_entry->updated_time());
  EXPECT_EQ(new_last_accessed, gdata_entry->last_viewed_time());

  const google_apis::Link* parent_link =
      gdata_entry->GetLinkByType(google_apis::Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);
  EXPECT_EQ(dest_entry.resource_id(),
            util::ExtractResourceIdFromUrl(parent_link->href()));
}

TEST_F(EntryUpdatePerformerTest, UpdateEntry_NotFound) {
  const std::string id = "this ID should result in NOT_FOUND";
  FileError error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      id, ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

TEST_F(EntryUpdatePerformerTest, UpdateEntry_ContentUpdate) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");

  const std::string local_id = GetLocalId(kFilePath);
  EXPECT_FALSE(local_id.empty());

  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  EXPECT_EQ(FILE_ERROR_OK, StoreAndMarkDirty(local_id, kTestFileContent));

  int64 original_changestamp =
      fake_service()->about_resource().largest_change_id();

  // The callback will be called upon completion of UpdateEntry().
  FileError error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Check that the server has received an update.
  EXPECT_LT(original_changestamp,
            fake_service()->about_resource().largest_change_id());

  // Check that the file size is updated to that of the updated content.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> server_entry;
  fake_service()->GetResourceEntry(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &server_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_EQ(static_cast<int64>(kTestFileContent.size()),
            server_entry->file_size());

  // Make sure that the cache is no longer dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());
}

TEST_F(EntryUpdatePerformerTest, UpdateEntry_ContentUpdateMd5Check) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");

  const std::string local_id = GetLocalId(kFilePath);
  EXPECT_FALSE(local_id.empty());

  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  EXPECT_EQ(FILE_ERROR_OK, StoreAndMarkDirty(local_id, kTestFileContent));

  int64 original_changestamp =
      fake_service()->about_resource().largest_change_id();

  // The callback will be called upon completion of UpdateEntry().
  FileError error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Check that the server has received an update.
  EXPECT_LT(original_changestamp,
            fake_service()->about_resource().largest_change_id());

  // Check that the file size is updated to that of the updated content.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> server_entry;
  fake_service()->GetResourceEntry(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &server_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_EQ(static_cast<int64>(kTestFileContent.size()),
            server_entry->file_size());

  // Make sure that the cache is no longer dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());

  // Again mark the cache file dirty.
  scoped_ptr<base::ScopedClosureRunner> file_closer;
  error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::OpenForWrite,
                 base::Unretained(cache()),
                 local_id,
                 &file_closer),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  file_closer.reset();

  // And call UpdateEntry again.
  // In this case, although the file is marked as dirty, but the content
  // hasn't been changed. Thus, the actual uploading should be skipped.
  original_changestamp = fake_service()->about_resource().largest_change_id();
  error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(original_changestamp,
            fake_service()->about_resource().largest_change_id());

  // Make sure that the cache is no longer dirty.
  success = false;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());
}

TEST_F(EntryUpdatePerformerTest, UpdateEntry_OpenedForWrite) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");

  const std::string local_id = GetLocalId(kFilePath);
  EXPECT_FALSE(local_id.empty());

  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  EXPECT_EQ(FILE_ERROR_OK, StoreAndMarkDirty(local_id, kTestFileContent));

  // Emulate a situation where someone is writing to the file.
  scoped_ptr<base::ScopedClosureRunner> file_closer;
  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::OpenForWrite,
                 base::Unretained(cache()),
                 local_id,
                 &file_closer),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Update. This should not clear the dirty bit.
  error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Make sure that the cache is still dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);
  EXPECT_TRUE(cache_entry.is_dirty());

  // Close the file.
  file_closer.reset();

  // Update. This should clear the dirty bit.
  error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Make sure that the cache is no longer dirty.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());
}

TEST_F(EntryUpdatePerformerTest, UpdateEntry_UploadNewFile) {
  // Create a new file locally.
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/New File.txt"));

  ResourceEntry parent;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kFilePath.DirName(), &parent));

  ResourceEntry entry;
  entry.set_parent_local_id(parent.local_id());
  entry.set_title(kFilePath.BaseName().AsUTF8Unsafe());
  entry.mutable_file_specific_info()->set_content_mime_type("text/plain");
  entry.set_metadata_edit_state(ResourceEntry::DIRTY);

  FileError error = FILE_ERROR_FAILED;
  std::string local_id;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::AddEntry,
                 base::Unretained(metadata()),
                 entry,
                 &local_id),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Update. This should result in creating a new file on the server.
  error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // The entry got a resource ID.
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kFilePath, &entry));
  EXPECT_FALSE(entry.resource_id().empty());
  EXPECT_EQ(ResourceEntry::CLEAN, entry.metadata_edit_state());

  // Make sure that the cache is no longer dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());

  // Make sure that we really created a file.
  google_apis::GDataErrorCode status = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> resource_entry;
  fake_service()->GetResourceEntry(
      entry.resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status,
                                                       &resource_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(resource_entry);
  EXPECT_FALSE(resource_entry->is_folder());
}

TEST_F(EntryUpdatePerformerTest, UpdateEntry_CreateDirectory) {
  // Create a new directory locally.
  const base::FilePath kPath(FILE_PATH_LITERAL("drive/root/New Directory"));

  ResourceEntry parent;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kPath.DirName(), &parent));

  ResourceEntry entry;
  entry.set_parent_local_id(parent.local_id());
  entry.set_title(kPath.BaseName().AsUTF8Unsafe());
  entry.mutable_file_info()->set_is_directory(true);
  entry.set_metadata_edit_state(ResourceEntry::DIRTY);

  FileError error = FILE_ERROR_FAILED;
  std::string local_id;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::AddEntry,
                 base::Unretained(metadata()),
                 entry,
                 &local_id),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Update. This should result in creating a new directory on the server.
  error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // The entry got a resource ID.
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kPath, &entry));
  EXPECT_FALSE(entry.resource_id().empty());
  EXPECT_EQ(ResourceEntry::CLEAN, entry.metadata_edit_state());

  // Make sure that we really created a directory.
  google_apis::GDataErrorCode status = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> resource_entry;
  fake_service()->GetResourceEntry(
      entry.resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&status,
                                                       &resource_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, status);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_folder());
}

}  // namespace internal
}  // namespace drive
