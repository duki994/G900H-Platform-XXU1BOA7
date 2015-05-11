// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_folder_finder.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/browser/media_galleries/media_scan_manager.h"
#include "chrome/browser/media_galleries/media_scan_manager_observer.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

class MockMediaFolderFinder : MediaFolderFinder {
 public:
  typedef base::Callback<void(MediaFolderFinderResultsCallback)>
      FindFoldersStartedCallback;

  static MediaFolderFinder* CreateMockMediaFolderFinder(
      const FindFoldersStartedCallback& started_callback,
      const base::Closure destruction_callback,
      const MediaFolderFinderResultsCallback& callback) {
    return new MockMediaFolderFinder(started_callback, destruction_callback,
                                     callback);
  }

  MockMediaFolderFinder(
      const FindFoldersStartedCallback& started_callback,
      const base::Closure destruction_callback,
      const MediaFolderFinderResultsCallback& callback)
      : MediaFolderFinder(callback),
        started_callback_(started_callback),
        destruction_callback_(destruction_callback),
        callback_(callback) {
  }
  virtual ~MockMediaFolderFinder() {
    destruction_callback_.Run();
  }

  virtual void StartScan() OVERRIDE {
    started_callback_.Run(callback_);
  }

 private:
  FindFoldersStartedCallback started_callback_;
  base::Closure destruction_callback_;
  MediaFolderFinderResultsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaFolderFinder);
};

}  // namespace

class TestMediaScanManager : public MediaScanManager {
 public:
  typedef base::Callback<MediaFolderFinder*(
      const MediaFolderFinder::MediaFolderFinderResultsCallback&)>
          MediaFolderFinderFactory;

  explicit TestMediaScanManager(const MediaFolderFinderFactory& factory) {
    SetMediaFolderFinderFactory(factory);
  }
  virtual ~TestMediaScanManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMediaScanManager);
};

class MediaScanManagerTest : public MediaScanManagerObserver,
                             public testing::Test {
 public:
  MediaScanManagerTest()
      : find_folders_start_count_(0),
        find_folders_destroy_count_(0),
        find_folders_success_(false),
        expected_gallery_count_(0),
        profile_(new TestingProfile()) {}

  virtual ~MediaScanManagerTest() {
    EXPECT_EQ(find_folders_start_count_, find_folders_destroy_count_);
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);

    gallery_prefs_ =
        MediaGalleriesPreferencesFactory::GetForProfile(profile_.get());
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        extensions::MediaGalleriesPermission::kReadPermission);
    extension_ = AddMediaGalleriesApp("read", read_permissions, profile_.get());

    ASSERT_TRUE(test_results_dir_.CreateUniqueTempDir());

    MockMediaFolderFinder::FindFoldersStartedCallback started_callback =
        base::Bind(&MediaScanManagerTest::OnFindFoldersStarted,
                   base::Unretained(this));
    base::Closure destruction_callback =
        base::Bind(&MediaScanManagerTest::OnFindFoldersDestroyed,
                   base::Unretained(this));
    TestMediaScanManager::MediaFolderFinderFactory factory =
        base::Bind(&MockMediaFolderFinder::CreateMockMediaFolderFinder,
                   started_callback, destruction_callback);
    media_scan_manager_.reset(new TestMediaScanManager(factory));
    media_scan_manager_->AddObserver(profile_.get(), this);
  }

  virtual void TearDown() OVERRIDE {
    media_scan_manager_->RemoveObserver(profile_.get());
    media_scan_manager_.reset();
    TestStorageMonitor::Destroy();
  }

  // Create a test folder in the test specific scoped temp dir and return the
  // final path.
  base::FilePath MakeTestFolder(const std::string& root_relative_path) {
    DCHECK(test_results_dir_.IsValid());
    base::FilePath path =
        test_results_dir_.path().AppendASCII(root_relative_path);
    if (!base::CreateDirectory(path)) {
      return base::FilePath();
    }
    return path;
  }

  // Create the specified path, and add it to preferences as a gallery.
  MediaGalleryPrefId AddGallery(const std::string& path,
                                MediaGalleryPrefInfo::Type type,
                                int audio_count,
                                int image_count,
                                int video_count) {
    base::FilePath full_path = MakeTestFolder(path);
    if (full_path.empty())
      return kInvalidMediaGalleryPrefId;
    MediaGalleryPrefInfo gallery_info;
    gallery_prefs_->LookUpGalleryByPath(full_path, &gallery_info);
    return gallery_prefs_->AddGallery(gallery_info.device_id,
                                      gallery_info.path,
                                      type,
                                      gallery_info.volume_label,
                                      gallery_info.vendor_name,
                                      gallery_info.model_name,
                                      gallery_info.total_size_in_bytes,
                                      gallery_info.last_attach_time,
                                      audio_count, image_count, video_count);
  }

  void SetFindFoldersResults(
      bool success,
      const MediaFolderFinder::MediaFolderFinderResults& results) {
    find_folders_success_ = success;
    find_folders_results_ = results;
  }

  void SetExpectedScanResults(int gallery_count,
                               const MediaGalleryScanResult& file_counts) {
    expected_gallery_count_ = gallery_count;
    expected_file_counts_ = file_counts;
  }

  void StartScan() {
    media_scan_manager_->StartScan(profile_.get(), extension_,
                                   true /* user_gesture */);
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_;
  }

  extensions::Extension* extension() {
    return extension_.get();
  }

  int FindFoldersStartCount() {
    return find_folders_start_count_;
  }

  int FindFolderDestroyCount() {
    return find_folders_destroy_count_;
  }

  void CheckFileCounts(MediaGalleryPrefId pref_id, int audio_count,
                       int image_count, int video_count) {
    if (!ContainsKey(gallery_prefs_->known_galleries(), pref_id)) {
      EXPECT_TRUE(false);
      return;
    }
    MediaGalleriesPrefInfoMap::const_iterator pref_info =
        gallery_prefs_->known_galleries().find(pref_id);
    EXPECT_EQ(audio_count, pref_info->second.audio_count);
    EXPECT_EQ(image_count, pref_info->second.image_count);
    EXPECT_EQ(video_count, pref_info->second.video_count);
  }

  // MediaScanMangerObserver implementation.
  virtual void OnScanFinished(
      const std::string& extension_id,
      int gallery_count,
      const MediaGalleryScanResult& file_counts) OVERRIDE {
    EXPECT_EQ(extension_->id(), extension_id);
    EXPECT_EQ(expected_gallery_count_, gallery_count);
    EXPECT_EQ(expected_file_counts_.audio_count, file_counts.audio_count);
    EXPECT_EQ(expected_file_counts_.image_count, file_counts.image_count);
    EXPECT_EQ(expected_file_counts_.video_count, file_counts.video_count);
  }

 private:
  void OnFindFoldersStarted(
      MediaFolderFinder::MediaFolderFinderResultsCallback callback) {
    find_folders_start_count_++;
    callback.Run(find_folders_success_, find_folders_results_);
  }

  void OnFindFoldersDestroyed() {
    find_folders_destroy_count_++;
  }

  scoped_ptr<TestMediaScanManager> media_scan_manager_;

  int find_folders_start_count_;
  int find_folders_destroy_count_;
  bool find_folders_success_;
  MediaFolderFinder::MediaFolderFinderResults find_folders_results_;

  int expected_gallery_count_;
  MediaGalleryScanResult expected_file_counts_;

  base::ScopedTempDir test_results_dir_;

  // Needed for extension service & friends to work.
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<extensions::Extension> extension_;

  EnsureMediaDirectoriesExists mock_gallery_locations_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  TestStorageMonitor monitor_;
  scoped_ptr<TestingProfile> profile_;
  MediaGalleriesPreferences* gallery_prefs_;

  DISALLOW_COPY_AND_ASSIGN(MediaScanManagerTest);
};

TEST_F(MediaScanManagerTest, SingleResult) {
  size_t galleries_before = gallery_prefs()->known_galleries().size();
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  file_counts.image_count = 2;
  file_counts.video_count = 3;
  base::FilePath path = MakeTestFolder("found_media_folder");

  MediaFolderFinder::MediaFolderFinderResults found_folders;
  found_folders[path] = file_counts;
  SetFindFoldersResults(true, found_folders);

  SetExpectedScanResults(1 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 1, gallery_prefs()->known_galleries().size());
}

TEST_F(MediaScanManagerTest, Containers) {
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 1;
  base::FilePath path;
  std::set<base::FilePath> expected_galleries;
  std::set<base::FilePath> bad_galleries;
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  size_t galleries_before = gallery_prefs()->known_galleries().size();

  // Should manifest as a gallery in result1.
  path = MakeTestFolder("dir1/result1");
  expected_galleries.insert(path);
  found_folders[path] = file_counts;

  // Should manifest as a gallery in dir2.
  path = MakeTestFolder("dir2/result2");
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  path = MakeTestFolder("dir2/result3");
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  expected_galleries.insert(path.DirName());

  // Should manifest as a two galleries: result4 and result5.
  path = MakeTestFolder("dir3/other");
  bad_galleries.insert(path);
  path = MakeTestFolder("dir3/result4");
  expected_galleries.insert(path);
  found_folders[path] = file_counts;
  path = MakeTestFolder("dir3/result5");
  expected_galleries.insert(path);
  found_folders[path] = file_counts;

  // Should manifest as a gallery in dir4.
  path = MakeTestFolder("dir4/other");
  bad_galleries.insert(path);
  path = MakeTestFolder("dir4/result6");
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  path = MakeTestFolder("dir4/result7");
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  path = MakeTestFolder("dir4/result8");
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  path = MakeTestFolder("dir4/result9");
  bad_galleries.insert(path);
  found_folders[path] = file_counts;
  expected_galleries.insert(path.DirName());

  SetFindFoldersResults(true, found_folders);

  file_counts.audio_count = 9;
  SetExpectedScanResults(5 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 5, gallery_prefs()->known_galleries().size());

  std::set<base::FilePath> found_galleries;
  for (MediaGalleriesPrefInfoMap::const_iterator it =
           gallery_prefs()->known_galleries().begin();
       it != gallery_prefs()->known_galleries().end();
       ++it) {
    found_galleries.insert(it->second.AbsolutePath());
    DCHECK(!ContainsKey(bad_galleries, it->second.AbsolutePath()));
  }
  for (std::set<base::FilePath>::const_iterator it = expected_galleries.begin();
       it != expected_galleries.end();
       ++it) {
    DCHECK(ContainsKey(found_galleries, *it));
  }
}

TEST_F(MediaScanManagerTest, UpdateExistingScanResults) {
  size_t galleries_before = gallery_prefs()->known_galleries().size();

  MediaGalleryPrefId ungranted_scan =
      AddGallery("uscan", MediaGalleryPrefInfo::kScanResult, 1, 0, 0);
  MediaGalleryPrefId granted_scan =
      AddGallery("gscan", MediaGalleryPrefInfo::kScanResult, 0, 2, 0);
  gallery_prefs()->SetGalleryPermissionForExtension(*extension(), granted_scan,
                                                    true);
  EXPECT_EQ(galleries_before + 2, gallery_prefs()->known_galleries().size());

  // Run once with no scan results. "uscan" should go away and "gscan" should
  // have its scan counts updated.
  MediaFolderFinder::MediaFolderFinderResults found_folders;
  SetFindFoldersResults(true, found_folders);

  MediaGalleryScanResult file_counts;
  SetExpectedScanResults(0 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 1, gallery_prefs()->known_galleries().size());
  CheckFileCounts(granted_scan, 0, 0, 0);

  MediaGalleryPrefId id =
      AddGallery("uscan", MediaGalleryPrefInfo::kScanResult, 1, 1, 1);
  EXPECT_NE(id, ungranted_scan);
  ungranted_scan = id;

  // Add scan results near the existing scan results.
  file_counts.audio_count = 0;
  file_counts.image_count = 0;
  file_counts.video_count = 7;
  base::FilePath path = MakeTestFolder("uscan");
  found_folders[path] = file_counts;

  file_counts.video_count = 11;
  path = MakeTestFolder("gscan/dir1");
  found_folders[path] = file_counts;

  SetFindFoldersResults(true, found_folders);
  file_counts.video_count = 7;
  SetExpectedScanResults(1 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 2, gallery_prefs()->known_galleries().size());
  CheckFileCounts(granted_scan, 0, 0, 11);
  // The new scan result should be one more than it's previous id.
  CheckFileCounts(ungranted_scan + 1, 0, 0, 7);
}

TEST_F(MediaScanManagerTest, UpdateExistingCounts) {
  size_t galleries_before = gallery_prefs()->known_galleries().size();

  MediaGalleryPrefId auto_id =
      AddGallery("auto", MediaGalleryPrefInfo::kAutoDetected, 1, 0, 0);
  MediaGalleryPrefId user_id =
      AddGallery("user", MediaGalleryPrefInfo::kUserAdded, 0, 2, 0);
  MediaGalleryPrefId scan_id =
      AddGallery("scan", MediaGalleryPrefInfo::kScanResult, 0, 0, 3);
  // Grant permission so this one isn't removed and readded.
  gallery_prefs()->SetGalleryPermissionForExtension(*extension(), scan_id,
                                                    true);
  CheckFileCounts(auto_id, 1, 0, 0);
  CheckFileCounts(user_id, 0, 2, 0);
  CheckFileCounts(scan_id, 0, 0, 3);

  MediaFolderFinder::MediaFolderFinderResults found_folders;
  MediaGalleryScanResult file_counts;
  file_counts.audio_count = 4;
  base::FilePath path = MakeTestFolder("auto/dir1");
  found_folders[path] = file_counts;

  file_counts.audio_count = 6;
  path = MakeTestFolder("scan");
  found_folders[path] = file_counts;

  file_counts.audio_count = 5;
  path = MakeTestFolder("user/dir2");
  found_folders[path] = file_counts;

  SetFindFoldersResults(true, found_folders);

  file_counts.audio_count = 0;
  SetExpectedScanResults(0 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 3, gallery_prefs()->known_galleries().size());
  CheckFileCounts(auto_id, 4, 0, 0);
  CheckFileCounts(user_id, 5, 0, 0);
  CheckFileCounts(scan_id, 6, 0, 0);

  EXPECT_EQ(1U, found_folders.erase(path));
  SetFindFoldersResults(true, found_folders);
  SetExpectedScanResults(0 /*gallery_count*/, file_counts);
  StartScan();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, FindFolderDestroyCount());
  EXPECT_EQ(galleries_before + 3, gallery_prefs()->known_galleries().size());
  CheckFileCounts(auto_id, 4, 0, 0);
  CheckFileCounts(user_id, 0, 0, 0);
  CheckFileCounts(scan_id, 6, 0, 0);
}
