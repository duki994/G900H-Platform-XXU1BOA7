// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

std::string GalleryName(const MediaGalleryPrefInfo& gallery) {
  base::string16 name = gallery.GetGalleryDisplayName();
  return UTF16ToASCII(name);
}

class MockMediaGalleriesDialog
    : public MediaGalleriesDialog {
 public:
  typedef base::Callback<void(int update_count)> DialogDestroyedCallback;

  explicit MockMediaGalleriesDialog(const DialogDestroyedCallback& callback)
      : update_count_(0),
        dialog_destroyed_callback_(callback) {
  }

  virtual ~MockMediaGalleriesDialog() {
    dialog_destroyed_callback_.Run(update_count_);
  }

  // MockMediaGalleriesDialog implementation.
  virtual void UpdateGalleries() OVERRIDE {
    update_count_++;
  }

  // Number of times UpdateResults has been called.
  int update_count() {
    return update_count_;
  }

 private:
  int update_count_;

  DialogDestroyedCallback dialog_destroyed_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaGalleriesDialog);
};

}  // namespace

class MediaGalleriesDialogControllerTest : public ::testing::Test {
 public:
  MediaGalleriesDialogControllerTest()
      : dialog_(NULL),
        dialog_update_count_at_destruction_(0),
        controller_(NULL),
        profile_(new TestingProfile()),
        weak_factory_(this) {
  }

  virtual ~MediaGalleriesDialogControllerTest() {
    EXPECT_FALSE(controller_);
    EXPECT_FALSE(dialog_);
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

    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        extensions::MediaGalleriesPermission::kReadPermission);
    extension_ = AddMediaGalleriesApp("read", read_permissions, profile_.get());
  }

  virtual void TearDown() OVERRIDE {
    TestStorageMonitor::Destroy();
  }

  void StartDialog() {
    ASSERT_FALSE(controller_);
    controller_ = new MediaGalleriesDialogController(
        *extension_.get(),
        gallery_prefs_.get(),
        base::Bind(&MediaGalleriesDialogControllerTest::CreateMockDialog,
                   base::Unretained(this)),
        base::Bind(
            &MediaGalleriesDialogControllerTest::OnControllerDone,
            base::Unretained(this)));
  }

  MediaGalleriesDialogController* controller() {
    return controller_;
  }

  MockMediaGalleriesDialog* dialog() {
    return dialog_;
  }

  int dialog_update_count_at_destruction() {
    EXPECT_FALSE(dialog_);
    return dialog_update_count_at_destruction_;
  }

  extensions::Extension* extension() {
    return extension_.get();
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_.get();
  }

  void TestForgottenType(MediaGalleryPrefInfo::Type type);

 protected:
  EnsureMediaDirectoriesExists mock_gallery_locations_;

 private:
  MediaGalleriesDialog* CreateMockDialog(
      MediaGalleriesDialogController* controller) {
    EXPECT_FALSE(dialog_);
    dialog_update_count_at_destruction_ = 0;
    dialog_ = new MockMediaGalleriesDialog(base::Bind(
        &MediaGalleriesDialogControllerTest::OnDialogDestroyed,
        weak_factory_.GetWeakPtr()));
    return dialog_;
  }

  void OnDialogDestroyed(int update_count) {
    EXPECT_TRUE(dialog_);
    dialog_update_count_at_destruction_ = update_count;
    dialog_ = NULL;
  }

  void OnControllerDone() {
    controller_ = NULL;
  }

  // Needed for extension service & friends to work.
  content::TestBrowserThreadBundle thread_bundle_;

  // The dialog is owned by the controller, but this pointer should only be
  // valid while the dialog is live within the controller.
  MockMediaGalleriesDialog* dialog_;
  int dialog_update_count_at_destruction_;

  // The controller owns itself.
  MediaGalleriesDialogController* controller_;

  scoped_refptr<extensions::Extension> extension_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  TestStorageMonitor monitor_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<MediaGalleriesPreferences> gallery_prefs_;

  base::WeakPtrFactory<MediaGalleriesDialogControllerTest>
      weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogControllerTest);
};

void MediaGalleriesDialogControllerTest::TestForgottenType(
    MediaGalleryPrefInfo::Type type) {
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  MediaGalleryPrefId forgotten1 = gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("forgotten1"), type);
  MediaGalleryPrefId forgotten2 = gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("forgotten2"), type);
  // Show dialog and accept to verify 2 entries
  StartDialog();
  EXPECT_EQ(mock_gallery_locations_.num_galleries() + 2U,
            controller()->AttachedPermissions().size());
  EXPECT_EQ(0U, controller()->UnattachedPermissions().size());
  controller()->DidToggleGalleryId(forgotten1, true);
  controller()->DidToggleGalleryId(forgotten2, true);
  controller()->DialogFinished(true);
  EXPECT_EQ(2U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Forget one and cancel to see that it's still there.
  StartDialog();
  controller()->DidForgetGallery(forgotten1);
  EXPECT_EQ(mock_gallery_locations_.num_galleries() + 1U,
            controller()->AttachedPermissions().size());
  controller()->DialogFinished(false);
  EXPECT_EQ(2U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Forget one and confirm to see that it's gone.
  StartDialog();
  controller()->DidForgetGallery(forgotten1);
  EXPECT_EQ(mock_gallery_locations_.num_galleries() + 1U,
            controller()->AttachedPermissions().size());
  controller()->DialogFinished(true);
  EXPECT_EQ(1U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Add a new one and forget it & see that it's gone.
  MediaGalleryPrefId forgotten3 = gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("forgotten3"), type);
  StartDialog();
  EXPECT_EQ(mock_gallery_locations_.num_galleries() + 2U,
            controller()->AttachedPermissions().size());
  EXPECT_EQ(0U, controller()->UnattachedPermissions().size());
  controller()->DidToggleGalleryId(forgotten3, true);
  controller()->DidForgetGallery(forgotten3);
  EXPECT_EQ(mock_gallery_locations_.num_galleries() + 1U,
            controller()->AttachedPermissions().size());
  controller()->DialogFinished(true);
  EXPECT_EQ(1U, gallery_prefs()->GalleriesForExtension(*extension()).size());
}

TEST_F(MediaGalleriesDialogControllerTest, TestForgottenUserAdded) {
  TestForgottenType(MediaGalleryPrefInfo::kUserAdded);
}

TEST_F(MediaGalleriesDialogControllerTest, TestForgottenAutoDetected) {
  TestForgottenType(MediaGalleryPrefInfo::kAutoDetected);
}

TEST_F(MediaGalleriesDialogControllerTest, TestForgottenScanResult) {
  TestForgottenType(MediaGalleryPrefInfo::kScanResult);
}

TEST_F(MediaGalleriesDialogControllerTest, TestNameGeneration) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = 1;
  gallery.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::FIXED_MASS_STORAGE, "/path/to/gallery");
  gallery.type = MediaGalleryPrefInfo::kAutoDetected;
  std::string galleryName("/path/to/gallery");
#if defined(OS_CHROMEOS)
  galleryName = "gallery";
#endif
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.display_name = base::ASCIIToUTF16("override");
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.display_name = base::string16();
  gallery.volume_label = base::ASCIIToUTF16("label");
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.path = base::FilePath(FILE_PATH_LITERAL("sub/gallery2"));
  galleryName = "/path/to/gallery/sub/gallery2";
#if defined(OS_CHROMEOS)
  galleryName = "gallery2";
#endif
#if defined(OS_WIN)
  galleryName = base::FilePath(FILE_PATH_LITERAL("/path/to/gallery"))
                    .Append(gallery.path).MaybeAsASCII();
#endif
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.path = base::FilePath();
  gallery.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      "/path/to/dcim");
  gallery.display_name = base::ASCIIToUTF16("override");
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.volume_label = base::ASCIIToUTF16("volume");
  gallery.vendor_name = base::ASCIIToUTF16("vendor");
  gallery.model_name = base::ASCIIToUTF16("model");
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.display_name = base::string16();
  EXPECT_EQ("volume", GalleryName(gallery));

  gallery.volume_label = base::string16();
  EXPECT_EQ("vendor, model", GalleryName(gallery));

  gallery.total_size_in_bytes = 1000000;
  EXPECT_EQ("977 KB vendor, model", GalleryName(gallery));

  gallery.path = base::FilePath(FILE_PATH_LITERAL("sub/path"));
  EXPECT_EQ("path - 977 KB vendor, model", GalleryName(gallery));
}
