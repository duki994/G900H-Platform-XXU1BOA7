// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/common/chrome_switches.h"

const char kDefaultProfileName[] = "default";

class AvatarButtonControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kNewProfileManagement);
    DCHECK(profiles::IsMultipleProfilesEnabled());

    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_.reset(
        [[AvatarButtonController alloc] initWithBrowser:browser()]);
  }

  virtual void TearDown() OVERRIDE {
    browser()->window()->Close();
    CocoaProfileTest::TearDown();
  }

  NSButton* button() { return [controller_ buttonView]; }

  NSView* view() { return [controller_ view]; }

  AvatarButtonController* controller() { return controller_.get(); }

 private:
  base::scoped_nsobject<AvatarButtonController> controller_;
};

TEST_F(AvatarButtonControllerTest, ButtonShown) {
  EXPECT_FALSE([view() isHidden]);
  EXPECT_EQ(kDefaultProfileName, base::SysNSStringToUTF8([button() title]));
}

TEST_F(AvatarButtonControllerTest, DoubleOpen) {
  EXPECT_FALSE([controller() menuController]);

  [button() performClick:button()];

  BaseBubbleController* menu = [controller() menuController];
  EXPECT_TRUE(menu);
  EXPECT_TRUE([menu isKindOfClass:[ProfileChooserController class]]);

  [button() performClick:button()];
  EXPECT_EQ(menu, [controller() menuController]);

  // Do not animate out because that is hard to test around.
  static_cast<InfoBubbleWindow*>(menu.window).allowedAnimations =
      info_bubble::kAnimateNone;
  [menu close];
  EXPECT_FALSE([controller() menuController]);
}
