// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "content/public/test/test_utils.h"
#include "google_apis/drive/test_util.h"

namespace extensions {

// This class contains chrome.filesystem API test specific to Chrome OS, namely,
// the integrated Google Drive support.
class FileSystemApiTestForDrive : public PlatformAppBrowserTest {
 public:
  FileSystemApiTestForDrive()
      : fake_drive_service_(NULL),
        integration_service_(NULL) {
  }

  // Sets up fake Drive service for tests (this has to be injected before the
  // real DriveIntegrationService instance is created.)
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();

    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    create_drive_integration_service_ =
        base::Bind(&FileSystemApiTestForDrive::CreateDriveIntegrationService,
                   base::Unretained(this));
    service_factory_for_test_.reset(
        new drive::DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }

  // Ensure the fake service's data is fetch in the local file system. This is
  // necessary because the fetch starts lazily upon the first read operation.
  virtual void SetUpOnMainThread() OVERRIDE {
    PlatformAppBrowserTest::SetUpOnMainThread();

    scoped_ptr<drive::ResourceEntry> entry;
    drive::FileError error = drive::FILE_ERROR_FAILED;
    integration_service_->file_system()->GetResourceEntry(
        base::FilePath::FromUTF8Unsafe("drive/root"),  // whatever
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    drive::test_util::RunBlockingPoolTask();
    ASSERT_EQ(drive::FILE_ERROR_OK, error);
  }

  virtual void TearDown() OVERRIDE {
    FileSystemChooseEntryFunction::StopSkippingPickerForTest();
    PlatformAppBrowserTest::TearDown();
  };

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    fake_drive_service_ = new drive::FakeDriveService;
    fake_drive_service_->LoadResourceListForWapi(
        "gdata/empty_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("drive/applist.json");

    SetUpTestFileHierarchy();

    integration_service_ = new drive::DriveIntegrationService(
        profile, NULL, fake_drive_service_, std::string(),
        test_cache_root_.path(), NULL);
    return integration_service_;
  }

  void SetUpTestFileHierarchy() {
    const std::string root = fake_drive_service_->GetRootResourceId();
    ASSERT_TRUE(AddTestFile("open_existing.txt", "Can you see me?", root));
    const std::string subdir = AddTestDirectory("subdir", root);
    ASSERT_FALSE(subdir.empty());
    ASSERT_TRUE(AddTestFile("open_existing.txt", "Can you see me?", subdir));
  }

  bool AddTestFile(const std::string& title,
                   const std::string& data,
                   const std::string& parent_id) {
    scoped_ptr<google_apis::ResourceEntry> resource_entry;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    fake_drive_service_->AddNewFile(
        "text/plain", data, parent_id, title, false,
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    content::RunAllPendingInMessageLoop();
    return error == google_apis::HTTP_CREATED && resource_entry;
  }

  std::string AddTestDirectory(const std::string& title,
                               const std::string& parent_id) {
    scoped_ptr<google_apis::ResourceEntry> resource_entry;
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    fake_drive_service_->AddNewDirectory(
        parent_id, title,
        drive::DriveServiceInterface::AddNewDirectoryOptions(),
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    content::RunAllPendingInMessageLoop();
    return error == google_apis::HTTP_CREATED && resource_entry ?
        resource_entry->resource_id() : "";
  }

  base::ScopedTempDir test_cache_root_;
  drive::FakeDriveService* fake_drive_service_;
  drive::DriveIntegrationService* integration_service_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  scoped_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenExistingFileTest) {
  base::FilePath test_file = drive::util::GetDriveMountPointPath(
      browser()->profile()).AppendASCII("root/open_existing.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenExistingFileWithWriteTest) {
  base::FilePath test_file = drive::util::GetDriveMountPointPath(
      browser()->profile()).AppendASCII("root/open_existing.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_existing_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryTest) {
  base::FilePath test_directory =
      drive::util::GetDriveMountPointPath(browser()->profile()).AppendASCII(
          "root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithWriteTest) {
  base::FilePath test_directory =
      drive::util::GetDriveMountPointPath(browser()->profile()).AppendASCII(
          "root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/file_system/open_directory_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithoutPermissionTest) {
  base::FilePath test_directory =
      drive::util::GetDriveMountPointPath(browser()->profile()).AppendASCII(
          "root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_without_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithOnlyWritePermissionTest) {
  base::FilePath test_directory =
      drive::util::GetDriveMountPointPath(browser()->profile()).AppendASCII(
          "root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_with_only_write"))
      << message_;
}

}  // namespace extensions
