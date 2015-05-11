// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleriesPreferences unit tests.

#include "chrome/browser/media_galleries/media_galleries_preferences.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "grit/generated_resources.h"
#include "sync/api/string_ordinal.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using base::ASCIIToUTF16;

namespace {

class MockGalleryChangeObserver
    : public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  explicit MockGalleryChangeObserver(MediaGalleriesPreferences* pref)
      : pref_(pref),
        notifications_(0) {}
  virtual ~MockGalleryChangeObserver() {}

  int notifications() const { return notifications_;}

 private:
  // MediaGalleriesPreferences::GalleryChangeObserver implementation.
  virtual void OnPermissionAdded(MediaGalleriesPreferences* pref,
                                 const std::string& extension_id,
                                 MediaGalleryPrefId pref_id) OVERRIDE {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  virtual void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                                   const std::string& extension_id,
                                   MediaGalleryPrefId pref_id) OVERRIDE {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  virtual void OnGalleryAdded(MediaGalleriesPreferences* pref,
                              MediaGalleryPrefId pref_id) OVERRIDE {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  virtual void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                                MediaGalleryPrefId pref_id) OVERRIDE {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  virtual void OnGalleryInfoUpdated(MediaGalleriesPreferences* pref,
                                    MediaGalleryPrefId pref_id) OVERRIDE {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  MediaGalleriesPreferences* pref_;
  int notifications_;

  DISALLOW_COPY_AND_ASSIGN(MockGalleryChangeObserver);
};

}  // namespace

class MediaGalleriesPreferencesTest : public testing::Test {
 public:
  typedef std::map<std::string /*device id*/, MediaGalleryPrefIdSet>
      DeviceIdPrefIdsMap;

  MediaGalleriesPreferencesTest()
      : profile_(new TestingProfile()),
        default_galleries_count_(0) {
  }

  virtual ~MediaGalleriesPreferencesTest() {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);

    gallery_prefs_.reset(new MediaGalleriesPreferences(profile_.get()));
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    // Load the default galleries into the expectations.
    const MediaGalleriesPrefInfoMap& known_galleries =
        gallery_prefs_->known_galleries();
    if (known_galleries.size()) {
      ASSERT_EQ(3U, known_galleries.size());
      default_galleries_count_ = 3;
      MediaGalleriesPrefInfoMap::const_iterator it;
      for (it = known_galleries.begin(); it != known_galleries.end(); ++it) {
        expected_galleries_[it->first] = it->second;
        if (it->second.type == MediaGalleryPrefInfo::kAutoDetected)
          expected_galleries_for_all.insert(it->first);
      }
    }

    std::vector<std::string> all_permissions;
    all_permissions.push_back(
        extensions::MediaGalleriesPermission::kReadPermission);
    all_permissions.push_back(
        extensions::MediaGalleriesPermission::kAllAutoDetectedPermission);
    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        extensions::MediaGalleriesPermission::kReadPermission);

    all_permission_extension =
        AddMediaGalleriesApp("all", all_permissions, profile_.get());
    regular_permission_extension =
        AddMediaGalleriesApp("regular", read_permissions, profile_.get());
    no_permissions_extension =
        AddMediaGalleriesApp("no", read_permissions, profile_.get());
  }

  virtual void TearDown() OVERRIDE {
    Verify();
    TestStorageMonitor::Destroy();
  }

  void Verify() {
    const MediaGalleriesPrefInfoMap& known_galleries =
        gallery_prefs_->known_galleries();
    EXPECT_EQ(expected_galleries_.size(), known_galleries.size());
    for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries.begin();
         it != known_galleries.end();
         ++it) {
      VerifyGalleryInfo(it->second, it->first);
      if (it->second.type != MediaGalleryPrefInfo::kAutoDetected &&
          it->second.type != MediaGalleryPrefInfo::kBlackListed) {
        if (!ContainsKey(expected_galleries_for_all, it->first) &&
            !ContainsKey(expected_galleries_for_regular, it->first)) {
          EXPECT_FALSE(gallery_prefs_->NonAutoGalleryHasPermission(it->first));
        } else {
          EXPECT_TRUE(gallery_prefs_->NonAutoGalleryHasPermission(it->first));
        }
      }
    }

    for (DeviceIdPrefIdsMap::const_iterator it = expected_device_map.begin();
         it != expected_device_map.end();
         ++it) {
      MediaGalleryPrefIdSet actual_id_set =
          gallery_prefs_->LookUpGalleriesByDeviceId(it->first);
      EXPECT_EQ(it->second, actual_id_set);
    }

    std::set<MediaGalleryPrefId> galleries_for_all =
        gallery_prefs_->GalleriesForExtension(*all_permission_extension.get());
    EXPECT_EQ(expected_galleries_for_all, galleries_for_all);

    std::set<MediaGalleryPrefId> galleries_for_regular =
        gallery_prefs_->GalleriesForExtension(
            *regular_permission_extension.get());
    EXPECT_EQ(expected_galleries_for_regular, galleries_for_regular);

    std::set<MediaGalleryPrefId> galleries_for_no =
        gallery_prefs_->GalleriesForExtension(*no_permissions_extension.get());
    EXPECT_EQ(0U, galleries_for_no.size());
  }

  void VerifyGalleryInfo(const MediaGalleryPrefInfo& actual,
                         MediaGalleryPrefId expected_id) const {
    MediaGalleriesPrefInfoMap::const_iterator in_expectation =
      expected_galleries_.find(expected_id);
    ASSERT_FALSE(in_expectation == expected_galleries_.end())  << expected_id;
    EXPECT_EQ(in_expectation->second.pref_id, actual.pref_id);
    EXPECT_EQ(in_expectation->second.display_name, actual.display_name);
    EXPECT_EQ(in_expectation->second.device_id, actual.device_id);
    EXPECT_EQ(in_expectation->second.path.value(), actual.path.value());
    EXPECT_EQ(in_expectation->second.type, actual.type);
    EXPECT_EQ(in_expectation->second.audio_count, actual.audio_count);
    EXPECT_EQ(in_expectation->second.image_count, actual.image_count);
    EXPECT_EQ(in_expectation->second.video_count, actual.video_count);
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_.get();
  }

  uint64 default_galleries_count() {
    return default_galleries_count_;
  }

  void AddGalleryExpectation(MediaGalleryPrefId id, base::string16 display_name,
                             std::string device_id,
                             base::FilePath relative_path,
                             MediaGalleryPrefInfo::Type type) {
    expected_galleries_[id].pref_id = id;
    expected_galleries_[id].display_name = display_name;
    expected_galleries_[id].device_id = device_id;
    expected_galleries_[id].path = relative_path.NormalizePathSeparators();
    expected_galleries_[id].type = type;

    if (type == MediaGalleryPrefInfo::kAutoDetected)
      expected_galleries_for_all.insert(id);

    expected_device_map[device_id].insert(id);
  }

  void AddScanResultExpectation(MediaGalleryPrefId id,
                                base::string16 display_name,
                                std::string device_id,
                                base::FilePath relative_path,
                                int audio_count,
                                int image_count,
                                int video_count) {
    AddGalleryExpectation(id, display_name, device_id, relative_path,
                          MediaGalleryPrefInfo::kScanResult);
    expected_galleries_[id].audio_count = audio_count;
    expected_galleries_[id].image_count = image_count;
    expected_galleries_[id].video_count = video_count;
  }

  MediaGalleryPrefId AddGalleryWithNameV0(const std::string& device_id,
                                          const base::string16& display_name,
                                          const base::FilePath& relative_path,
                                          bool user_added) {
    MediaGalleryPrefInfo::Type type =
        user_added ? MediaGalleryPrefInfo::kUserAdded
                   : MediaGalleryPrefInfo::kAutoDetected;
    return gallery_prefs()->AddGalleryInternal(
        device_id, display_name, relative_path, type,
        base::string16(), base::string16(), base::string16(), 0, base::Time(),
        false, 0, 0, 0, 0);
  }

  MediaGalleryPrefId AddGalleryWithNameV1(const std::string& device_id,
                                          const base::string16& display_name,
                                          const base::FilePath& relative_path,
                                          bool user_added) {
    MediaGalleryPrefInfo::Type type =
        user_added ? MediaGalleryPrefInfo::kUserAdded
                   : MediaGalleryPrefInfo::kAutoDetected;
    return gallery_prefs()->AddGalleryInternal(
        device_id, display_name, relative_path, type,
        base::string16(), base::string16(), base::string16(), 0, base::Time(),
        false, 0, 0, 0, 1);
  }

  MediaGalleryPrefId AddGalleryWithNameV2(const std::string& device_id,
                                          const base::string16& display_name,
                                          const base::FilePath& relative_path,
                                          MediaGalleryPrefInfo::Type type) {
    return gallery_prefs()->AddGalleryInternal(
        device_id, display_name, relative_path, type,
        base::string16(), base::string16(), base::string16(), 0, base::Time(),
        false, 0, 0, 0, 2);
  }

  MediaGalleryPrefId AddFixedGalleryWithExepectation(
      const std::string& path_name, const std::string& name,
      MediaGalleryPrefInfo::Type type) {
    base::FilePath path = MakeMediaGalleriesTestingPath(path_name);
    StorageInfo info;
    base::FilePath relative_path;
    MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
    info.set_name(ASCIIToUTF16(name));
    MediaGalleryPrefId id = AddGalleryWithNameV2(info.device_id(), info.name(),
                                                relative_path, type);
    AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                          type);
    Verify();
    return id;
  }

  bool UpdateDeviceIDForSingletonType(const std::string& device_id) {
    return gallery_prefs()->UpdateDeviceIDForSingletonType(device_id);
  }

  scoped_refptr<extensions::Extension> all_permission_extension;
  scoped_refptr<extensions::Extension> regular_permission_extension;
  scoped_refptr<extensions::Extension> no_permissions_extension;

  std::set<MediaGalleryPrefId> expected_galleries_for_all;
  std::set<MediaGalleryPrefId> expected_galleries_for_regular;

  DeviceIdPrefIdsMap expected_device_map;

  MediaGalleriesPrefInfoMap expected_galleries_;

 private:
  // Needed for extension service & friends to work.
  content::TestBrowserThreadBundle thread_bundle_;

  EnsureMediaDirectoriesExists mock_gallery_locations_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  TestStorageMonitor monitor_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<MediaGalleriesPreferences> gallery_prefs_;

  uint64 default_galleries_count_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPreferencesTest);
};

TEST_F(MediaGalleriesPreferencesTest, GalleryManagement) {
  MediaGalleryPrefId auto_id, user_added_id, scan_id, id;
  base::FilePath path;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detected gallery.
  path = MakeMediaGalleriesTestingPath("new_auto");
  StorageInfo info;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Add it as other types, nothing should happen.
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(auto_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(auto_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(auto_id, id);

  // Add a new user added gallery.
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewUserGallery"));
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 2UL, id);
  user_added_id = id;
  const std::string user_added_device_id = info.device_id();
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Add it as other types, nothing should happen.
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(user_added_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(user_added_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(user_added_id, id);
  Verify();

  // Add a new scan result gallery.
  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewScanGallery"));
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 3UL, id);
  scan_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  // Add it as other types, nothing should happen.
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(scan_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(scan_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(scan_id, id);
  Verify();

  // Lookup some galleries.
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_auto"), NULL));
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_user"), NULL));
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_scan"), NULL));
  EXPECT_FALSE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("other"), NULL));

  // Check that we always get the gallery info.
  MediaGalleryPrefInfo gallery_info;
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_auto"), &gallery_info));
  VerifyGalleryInfo(gallery_info, auto_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_user"), &gallery_info));
  VerifyGalleryInfo(gallery_info, user_added_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_scan"), &gallery_info));
  VerifyGalleryInfo(gallery_info, scan_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);

  path = MakeMediaGalleriesTestingPath("other");
  EXPECT_FALSE(gallery_prefs()->LookUpGalleryByPath(path, &gallery_info));
  EXPECT_EQ(kInvalidMediaGalleryPrefId, gallery_info.pref_id);

  StorageInfo other_info;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &other_info, &relative_path);
  EXPECT_EQ(other_info.device_id(), gallery_info.device_id);
  EXPECT_EQ(relative_path.value(), gallery_info.path.value());

  // Remove an auto added gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_id);
  Verify();

  // Remove a scan result (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(scan_id);
  expected_galleries_[scan_id].type = MediaGalleryPrefInfo::kRemovedScan;
  Verify();

  // Remove a user added gallery and it should go away.
  gallery_prefs()->ForgetGalleryById(user_added_id);
  expected_galleries_.erase(user_added_id);
  expected_device_map[user_added_device_id].erase(user_added_id);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, ForgetAndErase) {
  MediaGalleryPrefId user_erase =
      AddFixedGalleryWithExepectation("user_erase", "UserErase",
                                     MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 1UL, user_erase);
  MediaGalleryPrefId user_forget =
      AddFixedGalleryWithExepectation("user_forget", "UserForget",
                                      MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 2UL, user_forget);

  MediaGalleryPrefId auto_erase =
      AddFixedGalleryWithExepectation("auto_erase", "AutoErase",
                                      MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(default_galleries_count() + 3UL, auto_erase);
  MediaGalleryPrefId auto_forget =
      AddFixedGalleryWithExepectation("auto_forget", "AutoForget",
                                      MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(default_galleries_count() + 4UL, auto_forget);

  MediaGalleryPrefId scan_erase =
      AddFixedGalleryWithExepectation("scan_erase", "ScanErase",
                                      MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 5UL, scan_erase);
  MediaGalleryPrefId scan_forget =
      AddFixedGalleryWithExepectation("scan_forget", "ScanForget",
                                      MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 6UL, scan_forget);

  Verify();
  std::string device_id;

  gallery_prefs()->ForgetGalleryById(user_forget);
  device_id = expected_galleries_[user_forget].device_id;
  expected_galleries_.erase(user_forget);
  expected_device_map[device_id].erase(user_forget);
  Verify();

  gallery_prefs()->ForgetGalleryById(auto_forget);
  expected_galleries_[auto_forget].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_forget);
  Verify();

  gallery_prefs()->ForgetGalleryById(scan_forget);
  expected_galleries_[scan_forget].type = MediaGalleryPrefInfo::kRemovedScan;
  Verify();

  gallery_prefs()->EraseGalleryById(user_erase);
  device_id = expected_galleries_[user_erase].device_id;
  expected_galleries_.erase(user_erase);
  expected_device_map[device_id].erase(user_erase);
  Verify();

  gallery_prefs()->EraseGalleryById(auto_erase);
  device_id = expected_galleries_[auto_erase].device_id;
  expected_galleries_.erase(auto_erase);
  expected_device_map[device_id].erase(auto_erase);
  expected_galleries_for_all.erase(auto_erase);
  Verify();

  gallery_prefs()->EraseGalleryById(scan_erase);
  device_id = expected_galleries_[scan_erase].device_id;
  expected_galleries_.erase(scan_erase);
  expected_device_map[device_id].erase(scan_erase);
  Verify();

  // Also erase the previously forgetten ones to check erasing blacklisted ones.
  gallery_prefs()->EraseGalleryById(auto_forget);
  device_id = expected_galleries_[auto_forget].device_id;
  expected_galleries_.erase(auto_forget);
  expected_device_map[device_id].erase(auto_forget);
  Verify();

  gallery_prefs()->EraseGalleryById(scan_forget);
  device_id = expected_galleries_[scan_forget].device_id;
  expected_galleries_.erase(scan_forget);
  expected_device_map[device_id].erase(scan_forget);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, AddGalleryWithVolumeMetadata) {
  MediaGalleryPrefId id;
  StorageInfo info;
  base::FilePath path;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add a new auto detected gallery.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  id = gallery_prefs()->AddGallery(info.device_id(), relative_path,
                                   MediaGalleryPrefInfo::kAutoDetected,
                                   ASCIIToUTF16("volume label"),
                                   ASCIIToUTF16("vendor name"),
                                   ASCIIToUTF16("model name"),
                                   1000000ULL, now, 0, 0, 0);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, base::string16(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  MediaGalleryPrefInfo gallery_info;
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_auto"), &gallery_info));
  EXPECT_TRUE(gallery_info.volume_metadata_valid);
  EXPECT_EQ(ASCIIToUTF16("volume label"), gallery_info.volume_label);
  EXPECT_EQ(ASCIIToUTF16("vendor name"), gallery_info.vendor_name);
  EXPECT_EQ(ASCIIToUTF16("model name"), gallery_info.model_name);
  EXPECT_EQ(1000000ULL, gallery_info.total_size_in_bytes);
  // Note: we put the microseconds time into a double, so there'll
  // be some possible rounding errors. If it's less than 100, we don't
  // care.
  EXPECT_LE(abs(now.ToInternalValue() -
                gallery_info.last_attach_time.ToInternalValue()), 100);
}

TEST_F(MediaGalleriesPreferencesTest, ReplaceGalleryWithVolumeMetadata) {
  MediaGalleryPrefId id, metadata_id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add an auto detected gallery in the prefs version 0 format.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  id = AddGalleryWithNameV0(info.device_id(), info.name(),
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  metadata_id = gallery_prefs()->AddGallery(info.device_id(),
                                            relative_path,
                                            MediaGalleryPrefInfo::kAutoDetected,
                                            ASCIIToUTF16("volume label"),
                                            ASCIIToUTF16("vendor name"),
                                            ASCIIToUTF16("model name"),
                                            1000000ULL, now, 0, 0, 0);
  EXPECT_EQ(id, metadata_id);
  AddGalleryExpectation(id, base::string16(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);

  // Make sure the display_name is set to empty now, as the metadata
  // upgrade should set the manual override name empty.
  Verify();
}

// Whenever an "AutoDetected" gallery is removed, it is moved to a black listed
// state.  When the gallery is added again, the black listed state is updated
// back to the "AutoDetected" type.
TEST_F(MediaGalleriesPreferencesTest, AutoAddedBlackListing) {
  MediaGalleryPrefId auto_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detect gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  id = AddGalleryWithNameV1(info.device_id(), info.name(),
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Remove an auto added gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_id);
  Verify();

  // Try adding the gallery again automatically and it should be a no-op.
  id = AddGalleryWithNameV1(info.device_id(), info.name(),
                            relative_path, false /*auto*/);
  EXPECT_EQ(auto_id, id);
  Verify();

  // Add the gallery again as a user action.
  id = gallery_prefs()->AddGalleryByPath(path,
                                         MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(auto_id, id);
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();
}

// Whenever a "ScanResult" gallery is removed, it is moved to a black listed
// state.  When the gallery is added again, the black listed state is updated
// back to the "ScanResult" type.
TEST_F(MediaGalleriesPreferencesTest, ScanResultBlackListing) {
  MediaGalleryPrefId scan_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a new scan result gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewScanGallery"));
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  scan_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  // Remove a scan result gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(scan_id);
  expected_galleries_[scan_id].type = MediaGalleryPrefInfo::kRemovedScan;
  expected_galleries_for_all.erase(scan_id);
  Verify();

  // Try adding the gallery again as a scan result it should be a no-op.
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(scan_id, id);
  Verify();

  // Add the gallery again as a user action.
  id = gallery_prefs()->AddGalleryByPath(path,
                                         MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(scan_id, id);
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, UpdateGalleryNameV2) {
  // Add a new auto detect gallery to test with.
  base::FilePath path = MakeMediaGalleriesTestingPath("new_auto");
  StorageInfo info;
  base::FilePath relative_path;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  MediaGalleryPrefId id =
      AddGalleryWithNameV2(info.device_id(), info.name(),
                           relative_path, MediaGalleryPrefInfo::kAutoDetected);
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Won't override the name -- don't change any expectation.
  info.set_name(base::string16());
  AddGalleryWithNameV2(info.device_id(), info.name(), relative_path,
                       MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  info.set_name(ASCIIToUTF16("NewName"));
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kAutoDetected);
  // Note: will really just update the existing expectation.
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, GalleryPermissions) {
  MediaGalleryPrefId auto_id, user_added_id, to_blacklist_id, scan_id,
                     to_scan_remove_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add some galleries to test with.
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewUserGallery"));
  id = AddGalleryWithNameV1(info.device_id(), info.name(),
                            relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  user_added_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  id = AddGalleryWithNameV1(info.device_id(), info.name(),
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 2UL, id);
  auto_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  path = MakeMediaGalleriesTestingPath("to_blacklist");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("ToBlacklistGallery"));
  id = AddGalleryWithNameV1(info.device_id(), info.name(),
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 3UL, id);
  to_blacklist_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewScanGallery"));
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 4UL, id);
  scan_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  path = MakeMediaGalleriesTestingPath("to_scan_remove");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("ToScanRemoveGallery"));
  id = AddGalleryWithNameV2(info.device_id(), info.name(),
                            relative_path, MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 5UL, id);
  to_scan_remove_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  // Remove permission for all galleries from the all-permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), auto_id, false);
  expected_galleries_for_all.erase(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), user_added_id, false);
  expected_galleries_for_all.erase(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_blacklist_id, false);
  expected_galleries_for_all.erase(to_blacklist_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), scan_id, false);
  expected_galleries_for_all.erase(scan_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_scan_remove_id, false);
  expected_galleries_for_all.erase(to_scan_remove_id);
  Verify();

  // Add permission back for all galleries to the all-permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), auto_id, true);
  expected_galleries_for_all.insert(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), user_added_id, true);
  expected_galleries_for_all.insert(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_blacklist_id, true);
  expected_galleries_for_all.insert(to_blacklist_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), scan_id, true);
  expected_galleries_for_all.insert(scan_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_scan_remove_id, true);
  expected_galleries_for_all.insert(to_scan_remove_id);
  Verify();

  // Add permission for all galleries to the regular permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), auto_id, true);
  expected_galleries_for_regular.insert(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), user_added_id, true);
  expected_galleries_for_regular.insert(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), to_blacklist_id, true);
  expected_galleries_for_regular.insert(to_blacklist_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), scan_id, true);
  expected_galleries_for_regular.insert(scan_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), to_scan_remove_id, true);
  expected_galleries_for_regular.insert(to_scan_remove_id);
  Verify();

  // Blacklist the to be black listed gallery
  gallery_prefs()->ForgetGalleryById(to_blacklist_id);
  expected_galleries_[to_blacklist_id].type =
      MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(to_blacklist_id);
  expected_galleries_for_regular.erase(to_blacklist_id);
  Verify();

  gallery_prefs()->ForgetGalleryById(to_scan_remove_id);
  expected_galleries_[to_scan_remove_id].type =
      MediaGalleryPrefInfo::kRemovedScan;
  expected_galleries_for_all.erase(to_scan_remove_id);
  expected_galleries_for_regular.erase(to_scan_remove_id);
  Verify();

  // Remove permission for all galleries to the regular permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), auto_id, false);
  expected_galleries_for_regular.erase(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), user_added_id, false);
  expected_galleries_for_regular.erase(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), scan_id, false);
  expected_galleries_for_regular.erase(scan_id);
  Verify();

  // Add permission for an invalid gallery id.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), 9999L, true);
  Verify();
}

// What an existing gallery is added again, update the gallery information if
// needed.
TEST_F(MediaGalleriesPreferencesTest, UpdateGalleryDetails) {
  MediaGalleryPrefId auto_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detect gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  id = AddGalleryWithNameV1(info.device_id(), info.name(),
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Update the device name and add the gallery again.
  info.set_name(ASCIIToUTF16("AutoGallery2"));
  id = AddGalleryWithNameV1(info.device_id(), info.name(),
                            relative_path, false /*auto*/);
  EXPECT_EQ(auto_id, id);
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, MultipleGalleriesPerDevices) {
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a regular gallery
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewUserGallery"));
  MediaGalleryPrefId user_added_id =
      AddGalleryWithNameV1(info.device_id(), info.name(),
                           relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 1UL, user_added_id);
  AddGalleryExpectation(user_added_id, info.name(), info.device_id(),
                        relative_path, MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Find it by device id and fail to find something related.
  MediaGalleryPrefIdSet pref_id_set;
  pref_id_set = gallery_prefs()->LookUpGalleriesByDeviceId(info.device_id());
  EXPECT_EQ(1U, pref_id_set.size());
  EXPECT_TRUE(pref_id_set.find(user_added_id) != pref_id_set.end());

  MediaStorageUtil::GetDeviceInfoFromPath(
      MakeMediaGalleriesTestingPath("new_user/foo"), &info, &relative_path);
  pref_id_set = gallery_prefs()->LookUpGalleriesByDeviceId(info.device_id());
  EXPECT_EQ(0U, pref_id_set.size());

  // Add some galleries on the same device.
  relative_path = base::FilePath(FILE_PATH_LITERAL("path1/on/device1"));
  info.set_name(ASCIIToUTF16("Device1Path1"));
  std::string device_id = "path:device1";
  MediaGalleryPrefId dev1_path1_id = AddGalleryWithNameV1(
      device_id, info.name(), relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 2UL, dev1_path1_id);
  AddGalleryExpectation(dev1_path1_id, info.name(), device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path2/on/device1"));
  info.set_name(ASCIIToUTF16("Device1Path2"));
  MediaGalleryPrefId dev1_path2_id = AddGalleryWithNameV1(
      device_id, info.name(), relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 3UL, dev1_path2_id);
  AddGalleryExpectation(dev1_path2_id, info.name(), device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path1/on/device2"));
  info.set_name(ASCIIToUTF16("Device2Path1"));
  device_id = "path:device2";
  MediaGalleryPrefId dev2_path1_id = AddGalleryWithNameV1(
      device_id, info.name(), relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 4UL, dev2_path1_id);
  AddGalleryExpectation(dev2_path1_id, info.name(), device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path2/on/device2"));
  info.set_name(ASCIIToUTF16("Device2Path2"));
  MediaGalleryPrefId dev2_path2_id = AddGalleryWithNameV1(
      device_id, info.name(), relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 5UL, dev2_path2_id);
  AddGalleryExpectation(dev2_path2_id, info.name(), device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Check that adding one of them again works as expected.
  MediaGalleryPrefId id = AddGalleryWithNameV1(
      device_id, info.name(), relative_path, true /*user*/);
  EXPECT_EQ(dev2_path2_id, id);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, GalleryChangeObserver) {
  // Start with one observer.
  MockGalleryChangeObserver observer1(gallery_prefs());
  gallery_prefs()->AddGalleryChangeObserver(&observer1);

  // Add a new auto detected gallery.
  base::FilePath path = MakeMediaGalleriesTestingPath("new_auto");
  StorageInfo info;
  base::FilePath relative_path;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  MediaGalleryPrefId auto_id = AddGalleryWithNameV1(
      info.device_id(), info.name(), relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, auto_id);
  AddGalleryExpectation(auto_id, info.name(), info.device_id(),
                        relative_path, MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(1, observer1.notifications());

  // Add a second observer.
  MockGalleryChangeObserver observer2(gallery_prefs());
  gallery_prefs()->AddGalleryChangeObserver(&observer2);

  // Add a new user added gallery.
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewUserGallery"));
  MediaGalleryPrefId user_added_id =
      AddGalleryWithNameV1(info.device_id(), info.name(),
                           relative_path, true /*user*/);
  AddGalleryExpectation(user_added_id, info.name(), info.device_id(),
                        relative_path, MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 2UL, user_added_id);
  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(1, observer2.notifications());

  // Remove the first observer.
  gallery_prefs()->RemoveGalleryChangeObserver(&observer1);

  // Remove an auto added gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_id);

  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(2, observer2.notifications());

  // Remove a user added gallery and it should go away.
  gallery_prefs()->ForgetGalleryById(user_added_id);
  expected_galleries_.erase(user_added_id);
  expected_device_map[info.device_id()].erase(user_added_id);

  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(3, observer2.notifications());
}

TEST_F(MediaGalleriesPreferencesTest, UpdateSingletonDeviceIdType) {
  MediaGalleryPrefId id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detect gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  info.set_name(ASCIIToUTF16("NewAutoGallery"));
  info.set_device_id(StorageInfo::MakeDeviceId(StorageInfo::ITUNES,
                                               path.AsUTF8Unsafe()));
  id = AddGalleryWithNameV2(info.device_id(), info.name(), relative_path,
                            MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, info.name(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Update the device id.
  MockGalleryChangeObserver observer(gallery_prefs());
  gallery_prefs()->AddGalleryChangeObserver(&observer);

  path = MakeMediaGalleriesTestingPath("updated_path");
  std::string updated_device_id =
      StorageInfo::MakeDeviceId(StorageInfo::ITUNES, path.AsUTF8Unsafe());
  EXPECT_TRUE(UpdateDeviceIDForSingletonType(updated_device_id));
  AddGalleryExpectation(id, info.name(), updated_device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  expected_device_map[info.device_id()].erase(id);
  expected_device_map[updated_device_id].insert(id);
  Verify();
  EXPECT_EQ(1, observer.notifications());

  // No gallery for type.
  std::string new_device_id =
      StorageInfo::MakeDeviceId(StorageInfo::PICASA, path.AsUTF8Unsafe());
  EXPECT_FALSE(UpdateDeviceIDForSingletonType(new_device_id));
}

TEST_F(MediaGalleriesPreferencesTest, ScanResults) {
  MediaGalleryPrefId id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add a new scan result gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  id = gallery_prefs()->AddGallery(info.device_id(), relative_path,
                                   MediaGalleryPrefInfo::kScanResult,
                                   ASCIIToUTF16("volume label"),
                                   ASCIIToUTF16("vendor name"),
                                   ASCIIToUTF16("model name"),
                                   1000000ULL, now, 1, 2, 3);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddScanResultExpectation(id, base::string16(), info.device_id(),
                           relative_path, 1, 2, 3);
  Verify();

  // Update the found media count.
  id = gallery_prefs()->AddGallery(info.device_id(), relative_path,
                                   MediaGalleryPrefInfo::kScanResult,
                                   ASCIIToUTF16("volume label"),
                                   ASCIIToUTF16("vendor name"),
                                   ASCIIToUTF16("model name"),
                                   1000000ULL, now, 4, 5, 6);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddScanResultExpectation(id, base::string16(), info.device_id(),
                           relative_path, 4, 5, 6);
  Verify();

  // Remove a scan result (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(id);
  expected_galleries_[id].type = MediaGalleryPrefInfo::kRemovedScan;
  expected_galleries_[id].audio_count = 0;
  expected_galleries_[id].image_count = 0;
  expected_galleries_[id].video_count = 0;
  Verify();

  // Try adding the gallery again as a scan result it should be a no-op.
  id = gallery_prefs()->AddGallery(info.device_id(), relative_path,
                                   MediaGalleryPrefInfo::kScanResult,
                                   ASCIIToUTF16("volume label"),
                                   ASCIIToUTF16("vendor name"),
                                   ASCIIToUTF16("model name"),
                                   1000000ULL, now, 7, 8, 9);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  Verify();

  // Add the gallery again as a user action.
  id = gallery_prefs()->AddGalleryByPath(path,
                                         MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, base::string16(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();
}

TEST(MediaGalleriesPrefInfoTest, NameGeneration) {
  ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());

  MediaGalleryPrefInfo info;
  info.pref_id = 1;
  info.display_name = ASCIIToUTF16("override");
  info.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM, "unique");

  EXPECT_EQ(ASCIIToUTF16("override"), info.GetGalleryDisplayName());

  info.display_name = ASCIIToUTF16("o2");
  EXPECT_EQ(ASCIIToUTF16("o2"), info.GetGalleryDisplayName());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MEDIA_GALLERIES_DIALOG_DEVICE_NOT_ATTACHED),
            info.GetGalleryAdditionalDetails());

  info.last_attach_time = base::Time::Now();
  EXPECT_NE(l10n_util::GetStringUTF16(
                IDS_MEDIA_GALLERIES_DIALOG_DEVICE_NOT_ATTACHED),
            info.GetGalleryAdditionalDetails());
  EXPECT_NE(l10n_util::GetStringUTF16(
                IDS_MEDIA_GALLERIES_DIALOG_DEVICE_ATTACHED),
            info.GetGalleryAdditionalDetails());

  info.volume_label = ASCIIToUTF16("vol");
  info.vendor_name = ASCIIToUTF16("vendor");
  info.model_name = ASCIIToUTF16("model");
  EXPECT_EQ(ASCIIToUTF16("o2"), info.GetGalleryDisplayName());

  info.display_name = base::string16();
  EXPECT_EQ(ASCIIToUTF16("vol"), info.GetGalleryDisplayName());
  info.volume_label = base::string16();
  EXPECT_EQ(ASCIIToUTF16("vendor, model"), info.GetGalleryDisplayName());

  info.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::FIXED_MASS_STORAGE, "unique");
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("unique")).AsUTF8Unsafe(),
            base::UTF16ToUTF8(info.GetGalleryTooltip()));

  TestStorageMonitor::Destroy();
}
