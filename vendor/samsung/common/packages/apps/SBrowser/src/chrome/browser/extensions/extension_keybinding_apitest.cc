// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"

using content::WebContents;

namespace extensions {

class CommandsApiTest : public ExtensionApiTest {
 public:
  CommandsApiTest() {}
  virtual ~CommandsApiTest() {}

 protected:
  BrowserActionTestUtil GetBrowserActionsBar() {
    return BrowserActionTestUtil(browser());
  }

  bool IsGrantedForTab(const Extension* extension,
                       const content::WebContents* web_contents) {
    return PermissionsData::HasAPIPermissionForTab(
        extension,
        SessionID::IdForTab(web_contents),
        APIPermission::kTab);
  }
};

// Test the basic functionality of the Keybinding API:
// - That pressing the shortcut keys should perform actions (activate the
//   browser action or send an event).
// - Note: Page action keybindings are tested in PageAction test below.
// - The shortcut keys taken by one extension are not overwritten by the last
//   installed extension.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Load this extension, which uses the same keybindings but sets the page
  // to different colors. This is so we can see that it doesn't interfere. We
  // don't test this extension in any other way (it should otherwise be
  // immaterial to this test).
  ASSERT_TRUE(RunExtensionTest("keybinding/conflicting")) << message_;

  // Test that there are two browser actions in the toolbar.
  ASSERT_EQ(2, GetBrowserActionsBar().NumberOfBrowserActions());

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  // activeTab shouldn't have been granted yet.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  EXPECT_FALSE(IsGrantedForTab(extension, tab));

  // Activate the shortcut (Ctrl+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));

  // activeTab should now be granted.
  EXPECT_TRUE(IsGrantedForTab(extension, tab));

  // Verify the command worked.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function(){"
      "  if(document.body.bgColor == 'red'){"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Activate the shortcut (Ctrl+Shift+Y).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_Y, true, true, false, false));

  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function(){"
      "  if(document.body.bgColor == 'blue'){"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

// Flaky on linux and chromeos, http://crbug.com/165825
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_PageAction PageAction
#else
#define MAYBE_PageAction DISABLED_PageAction
#endif
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_PageAction) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  {
    // Load a page, the extension will detect the navigation and request to show
    // the page action icon.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        test_server()->GetURL("files/extensions/test_file.txt"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Make sure it appears and is the right one.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  int tab_id = SessionTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())->session_id().id();
  ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())->
      GetPageAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_EQ("Make this page red", action->GetTitle(tab_id));

  // Activate the shortcut (Alt+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, true, true, false));

  // Verify the command worked (the page action turns the page red).
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function(){"
      "  if(document.body.bgColor == 'red'){"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_SynthesizedCommand DISABLED_SynthesizedCommand
#else
#define MAYBE_SynthesizedCommand SynthesizedCommand
#endif

// This test validates that the getAll query API function returns registered
// commands as well as synthesized ones and that inactive commands (like the
// synthesized ones are in nature) have no shortcuts.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_SynthesizedCommand) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/synthesized")) << message_;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_DontOverwriteSystemShortcuts DISABLED_DontOverwriteSystemShortcuts
#else
#define MAYBE_DontOverwriteSystemShortcuts DontOverwriteSystemShortcuts
#endif

// This test validates that an extension cannot request a shortcut that is
// already in use by Chrome.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_DontOverwriteSystemShortcuts) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(RunExtensionTest("keybinding/dont_overwrite_system")) << message_;

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Activate the shortcut (Alt+Shift+F) to make the page blue.
  {
    ResultCatcher catcher;
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_F, false, true, true, false));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'blue') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Activate the bookmark shortcut (Ctrl+D) to make the page green (should not
  // work without requesting via chrome_settings_overrides).
#if defined(OS_MACOSX)
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, false, false, false, true));
#else
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, true, false, false, false));
#endif

  // The page should still be blue.
  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'blue') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);

  // Activate the shortcut (Ctrl+F) to make the page red (should not work).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, false, false, false));

  // The page should still be blue.
  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'blue') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

// This test validates that an extension can override the Chrome bookmark
// shortcut if it has requested to do so.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, OverwriteBookmarkShortcut) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui",
      "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/overwrite_bookmark_shortcut"))
      << message_;

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/extensions/test_file.txt"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Activate the shortcut (Ctrl+D) to make the page green.
  {
    ResultCatcher catcher;
#if defined(OS_MACOSX)
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, false, false, false, true));
#else
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_D, true, false, false, false));
#endif
    ASSERT_TRUE(catcher.GetNextResult());
  }

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "setInterval(function() {"
      "  if (document.body.bgColor == 'green') {"
      "    window.domAutomationController.send(true)}}, 100)",
      &result));
  ASSERT_TRUE(result);
}

#if defined(OS_WIN)
// Currently this feature is implemented on Windows only.
#define MAYBE_AllowDuplicatedMediaKeys AllowDuplicatedMediaKeys
#else
#define MAYBE_AllowDuplicatedMediaKeys DISABLED_AllowDuplicatedMediaKeys
#endif

// Test that media keys go to all extensions that register for them.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_AllowDuplicatedMediaKeys) {
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/non_global_media_keys_0"))
      << message_;
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(RunExtensionTest("keybinding/non_global_media_keys_1"))
      << message_;
  ASSERT_TRUE(catcher.GetNextResult());

  // Activate the Media Stop key.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_MEDIA_STOP, false, false, false, false));

  // We should get two success result.
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace extensions
