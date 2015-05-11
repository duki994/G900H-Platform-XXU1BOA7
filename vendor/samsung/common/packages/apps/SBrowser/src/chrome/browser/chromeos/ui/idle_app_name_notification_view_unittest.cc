// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/idle_app_name_notification_view.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/common/manifest_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kTestAppName[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
}  // namespace

class IdleAppNameNotificationViewTest : public BrowserWithTestWindowTest {
 public:
  IdleAppNameNotificationViewTest()
      : BrowserWithTestWindowTest(
            Browser::TYPE_TABBED,
            chrome::HOST_DESKTOP_TYPE_ASH,
            false) {
  }

  virtual ~IdleAppNameNotificationViewTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Add the application switch.
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(::switches::kAppId,
                                                        kTestAppName);

    BrowserWithTestWindowTest::SetUp();

    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName, "Test");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetString(extensions::manifest_keys::kDescription, "Test app");
    manifest.SetString("author", "Someone");

    std::string error;
    correct_extension_ =
        extensions::Extension::Create(base::FilePath(),
                                      extensions::Manifest::UNPACKED,
                                      manifest,
                                      extensions::Extension::NO_FLAGS,
                                      kTestAppName,
                                      &error);
    base::DictionaryValue manifest2;
    manifest2.SetString(extensions::manifest_keys::kName, "Test");
    manifest2.SetString(extensions::manifest_keys::kVersion, "1");
    manifest2.SetString(extensions::manifest_keys::kDescription, "Test app");

    incorrect_extension_ =
        extensions::Extension::Create(base::FilePath(),
                                      extensions::Manifest::UNPACKED,
                                      manifest2,
                                      extensions::Extension::NO_FLAGS,
                                      kTestAppName,
                                      &error);
  }

  virtual void TearDown() OVERRIDE {
    // The destruction of the widget might be a delayed task.
    base::MessageLoop::current()->RunUntilIdle();
    BrowserWithTestWindowTest::TearDown();
  }

  extensions::Extension* correct_extension() { return correct_extension_; }
  extensions::Extension* incorrect_extension() { return incorrect_extension_; }

 private:
  // Extensions to test with.
  scoped_refptr<extensions::Extension> correct_extension_;
  scoped_refptr<extensions::Extension> incorrect_extension_;

  DISALLOW_COPY_AND_ASSIGN(IdleAppNameNotificationViewTest);
};

// Check that creating and immediate destroying does not crash (and closes the
// message).
TEST_F(IdleAppNameNotificationViewTest, CheckTooEarlyDestruction) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  scoped_ptr<chromeos::IdleAppNameNotificationView> message(
      new chromeos::IdleAppNameNotificationView(10, 5, correct_extension()));
}

// Check that the message gets created and it destroys itself after time.
TEST_F(IdleAppNameNotificationViewTest, CheckSelfDestruction) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  scoped_ptr<chromeos::IdleAppNameNotificationView> message(
      new chromeos::IdleAppNameNotificationView(10, 5, correct_extension()));
  EXPECT_TRUE(message->IsVisible());

  // Wait now for some time and see that it closes itself again.
  for (int i = 0; i < 50 && message->IsVisible(); i++) {
    sleep(1);
    base::MessageLoop::current()->RunUntilIdle();
  }
  EXPECT_FALSE(message->IsVisible());
}

// Check that the shown text for a correct application is correct.
TEST_F(IdleAppNameNotificationViewTest, CheckCorrectApp) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  scoped_ptr<chromeos::IdleAppNameNotificationView> message(
      new chromeos::IdleAppNameNotificationView(10, 5, correct_extension()));
  base::string16 text = message->GetShownTextForTest();
  // Check that the string starts with the application name followed by a space.
  base::string16 name = base::ASCIIToUTF16("Test ");
  EXPECT_EQ(name, text.substr(0, name.length()));
  // Check that the string ends with a space + author's name.
  base::string16 author = base::ASCIIToUTF16(" Someone");
  EXPECT_EQ(author,
            text.substr(text.length() - author.length(), author.length()));
}

// Check that an invalid author gets shown accordingly.
TEST_F(IdleAppNameNotificationViewTest, CheckInvalidAuthor) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  scoped_ptr<chromeos::IdleAppNameNotificationView> message(
      new chromeos::IdleAppNameNotificationView(10, 5, incorrect_extension()));
  base::string16 text = message->GetShownTextForTest();
  // Check that the string starts with the application name followed by a space.
  base::string16 name = base::ASCIIToUTF16("Test ");
  EXPECT_EQ(name, text.substr(0, name.length()));
  // Check that it ends in an invalid author notification.
  base::string16 author = l10n_util::GetStringUTF16(
            IDS_IDLE_APP_NAME_INVALID_AUTHOR_NOTIFICATION);
  EXPECT_EQ(author,
            text.substr(text.length() - author.length(), author.length()));
}

// Check that an invalid app gets shown accordingly.
TEST_F(IdleAppNameNotificationViewTest, CheckInvalidApp) {
  // Create a message which is visible for 10ms and fades in/out for 5ms.
  scoped_ptr<chromeos::IdleAppNameNotificationView> message(
      new chromeos::IdleAppNameNotificationView(10, 5, NULL));
  base::string16 text = message->GetShownTextForTest();
  base::string16 error = l10n_util::GetStringUTF16(
      IDS_IDLE_APP_NAME_UNKNOWN_APPLICATION_NOTIFICATION);
  EXPECT_EQ(error, text);
}
