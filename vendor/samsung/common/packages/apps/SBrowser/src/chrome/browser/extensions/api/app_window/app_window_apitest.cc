// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/base/base_window.h"
#include "ui/gfx/rect.h"

#ifdef TOOLKIT_GTK
#include "content/public/test/test_utils.h"
#endif

using apps::AppWindow;

namespace {

class TestAppWindowRegistryObserver : public apps::AppWindowRegistry::Observer {
 public:
  explicit TestAppWindowRegistryObserver(Profile* profile)
      : profile_(profile), icon_updates_(0) {
    apps::AppWindowRegistry::Get(profile_)->AddObserver(this);
  }
  virtual ~TestAppWindowRegistryObserver() {
    apps::AppWindowRegistry::Get(profile_)->RemoveObserver(this);
  }

  // Overridden from AppWindowRegistry::Observer:
  virtual void OnAppWindowAdded(AppWindow* app_window) OVERRIDE {}
  virtual void OnAppWindowIconChanged(AppWindow* app_window) OVERRIDE {
    ++icon_updates_;
  }
  virtual void OnAppWindowRemoved(AppWindow* app_window) OVERRIDE {}

  int icon_updates() { return icon_updates_; }

 private:
  Profile* profile_;
  int icon_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestAppWindowRegistryObserver);
};

}  // namespace

namespace extensions {

// Flaky, http://crbug.com/164735 .
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DISABLED_WindowsApiBounds) {
  ExtensionTestMessageListener background_listener("background_ok", false);
  ExtensionTestMessageListener ready_listener("ready", true /* will_reply */);
  ExtensionTestMessageListener success_listener("success", false);

  LoadAndLaunchPlatformApp("windows_api_bounds");
  ASSERT_TRUE(background_listener.WaitUntilSatisfied());
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  AppWindow* window = GetFirstAppWindow();

  gfx::Rect new_bounds(100, 200, 300, 400);
  new_bounds.Inset(-window->GetBaseWindow()->GetFrameInsets());
  window->GetBaseWindow()->SetBounds(new_bounds);

  // TODO(jeremya/asargent) figure out why in GTK the window doesn't end up
// with exactly the bounds we set. Is it a bug in our app window
// implementation?  crbug.com/160252
#ifdef TOOLKIT_GTK
  int slop = 50;
#else
  int slop = 0;
#endif  // !TOOLKIT_GTK

  ready_listener.Reply(base::IntToString(slop));

#ifdef TOOLKIT_GTK
  // TODO(asargent)- this is here to help track down the root cause of
  // crbug.com/164735.
  {
    gfx::Rect last_bounds;
    while (!success_listener.was_satisfied()) {
      gfx::Rect current_bounds = window->GetBaseWindow()->GetBounds();
      if (current_bounds != last_bounds) {
        LOG(INFO) << "new bounds: " << current_bounds.ToString();
      }
      last_bounds = current_bounds;
      content::RunAllPendingInMessageLoop();
    }
  }
#endif

  ASSERT_TRUE(success_listener.WaitUntilSatisfied());
}

// Tests chrome.app.window.setIcon.
IN_PROC_BROWSER_TEST_F(ExperimentalPlatformAppBrowserTest, WindowsApiSetIcon) {
  scoped_ptr<TestAppWindowRegistryObserver> test_observer(
      new TestAppWindowRegistryObserver(browser()->profile()));
  ExtensionTestMessageListener listener("IconSet", false);
  LoadAndLaunchPlatformApp("windows_api_set_icon");
  EXPECT_EQ(0, test_observer->icon_updates());
  // Wait until the icon load has been requested.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  // Now wait until the WebContent has decoded the icon and chrome has
  // processed it. This needs to be in a loop since the renderer runs in a
  // different process.
  while (test_observer->icon_updates() < 1) {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  AppWindow* app_window = GetFirstAppWindow();
  ASSERT_TRUE(app_window);
  EXPECT_NE(std::string::npos,
            app_window->app_icon_url().spec().find("icon.png"));
  EXPECT_EQ(1, test_observer->icon_updates());
}

// TODO(asargent) - Figure out what to do about the fact that minimize events
// don't work under ubuntu unity.
// (crbug.com/162794 and https://bugs.launchpad.net/unity/+bug/998073).
// TODO(linux_aura) http://crbug.com/163931
// Flaky on Mac, http://crbug.com/232330
#if defined(TOOLKIT_VIEWS) && !(defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiProperties) {
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/windows_api_properties")) << message_;
}

#endif  // defined(TOOLKIT_VIEWS)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlwaysOnTopWithPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/has_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlwaysOnTopNoPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/no_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiGet) {
  EXPECT_TRUE(RunPlatformAppTest("platform_apps/windows_api_get"))
      << message_;
}

}  // namespace extensions
