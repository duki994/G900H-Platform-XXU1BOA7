// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/shell.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/login/fake_user_manager.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace chromeos {

namespace {

// This is a simple test app that creates an app window and immediately closes
// it again. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggbflgnkafappblpkiflbgpmkfdpnhhe
const char kTestKioskApp[] = "ggbflgnkafappblpkiflbgpmkfdpnhhe";

// This app creates a window and declares usage of the identity API in its
// manifest, so we can test device robot token minting via the identity API.
// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ibjkkfdnfcaoapcpheeijckmpcfkifob
const char kTestEnterpriseKioskApp[] = "ibjkkfdnfcaoapcpheeijckmpcfkifob";

// An offline enable test app. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ajoggoflpgplnnjkjamcmbepjdjdnpdp
// An app profile with version 1.0.0 installed is in
//   chrome/test/data/chromeos/app_mode/offline_enabled_app_profile
// The version 2.0.0 crx is in
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
const char kTestOfflineEnabledKioskApp[] = "ajoggoflpgplnnjkjamcmbepjdjdnpdp";

// Timeout while waiting for network connectivity during tests.
const int kTestNetworkTimeoutSeconds = 1;

// Email of owner account for test.
const char kTestOwnerEmail[] = "owner@example.com";

const char kTestEnterpriseAccountId[] = "enterprise-kiosk-app@localhost";
const char kTestEnterpriseServiceAccountId[] = "service_account@example.com";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestUserinfoToken[] = "fake-userinfo-token";
const char kTestLoginToken[] = "fake-login-token";
const char kTestAccessToken[] = "fake-access-token";
const char kTestClientId[] = "fake-client-id";
const char kTestAppScope[] =
    "https://www.googleapis.com/auth/userinfo.profile";

// Test JS API.
const char kLaunchAppForTestNewAPI[] =
    "login.AccountPickerScreen.runAppForTesting";
const char kLaunchAppForTestOldAPI[] =
    "login.AppsMenuButton.runAppForTesting";
const char kCheckDiagnosticModeNewAPI[] =
    "$('oobe').confirmDiagnosticMode_";
const char kCheckDiagnosticModeOldAPI[] =
    "$('show-apps-button').confirmDiagnosticMode_";

// Helper function for GetConsumerKioskAutoLaunchStatusCallback.
void ConsumerKioskAutoLaunchStatusCheck(
    KioskAppManager::ConsumerKioskAutoLaunchStatus* out_status,
    const base::Closure& runner_quit_task,
    KioskAppManager::ConsumerKioskAutoLaunchStatus in_status) {
  LOG(INFO) << "KioskAppManager::ConsumerKioskModeStatus = " << in_status;
  *out_status = in_status;
  runner_quit_task.Run();
}

// Helper KioskAppManager::EnableKioskModeCallback implementation.
void ConsumerKioskModeAutoStartLockCheck(
    bool* out_locked,
    const base::Closure& runner_quit_task,
    bool in_locked) {
  LOG(INFO) << "kiosk locked  = " << in_locked;
  *out_locked = in_locked;
  runner_quit_task.Run();
}

// Helper function for WaitForNetworkTimeOut.
void OnNetworkWaitTimedOut(const base::Closure& runner_quit_task) {
  runner_quit_task.Run();
}

// Helper function for DeviceOAuth2TokenServiceFactory::Get().
void CopyTokenService(DeviceOAuth2TokenService** out_token_service,
                      DeviceOAuth2TokenService* in_token_service) {
  *out_token_service = in_token_service;
}

// Helper functions for CanConfigureNetwork mock.
class ScopedCanConfigureNetwork {
 public:
  ScopedCanConfigureNetwork(bool can_configure, bool needs_owner_auth)
      : can_configure_(can_configure),
        needs_owner_auth_(needs_owner_auth),
        can_configure_network_callback_(
            base::Bind(&ScopedCanConfigureNetwork::CanConfigureNetwork,
                       base::Unretained(this))),
        needs_owner_auth_callback_(base::Bind(
            &ScopedCanConfigureNetwork::NeedsOwnerAuthToConfigureNetwork,
            base::Unretained(this))) {
    AppLaunchController::SetCanConfigureNetworkCallbackForTesting(
        &can_configure_network_callback_);
    AppLaunchController::SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
        &needs_owner_auth_callback_);
  }
  ~ScopedCanConfigureNetwork() {
    AppLaunchController::SetCanConfigureNetworkCallbackForTesting(NULL);
    AppLaunchController::SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
        NULL);
  }

  bool CanConfigureNetwork() {
    return can_configure_;
  }

  bool NeedsOwnerAuthToConfigureNetwork() {
    return needs_owner_auth_;
  }

 private:
  bool can_configure_;
  bool needs_owner_auth_;
  AppLaunchController::ReturnBoolCallback can_configure_network_callback_;
  AppLaunchController::ReturnBoolCallback needs_owner_auth_callback_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCanConfigureNetwork);
};

// Helper class to wait until a js condition becomes true.
class JsConditionWaiter {
 public:
  JsConditionWaiter(content::WebContents* web_contents,
                    const std::string& js)
      : web_contents_(web_contents),
        js_(js) {
  }

  void Wait() {
    if (CheckJs())
      return;

    base::RepeatingTimer<JsConditionWaiter> check_timer;
    check_timer.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(10),
        this,
        &JsConditionWaiter::OnTimer);

    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

 private:
  bool CheckJs() {
    bool result;
    CHECK(content::ExecuteScriptAndExtractBool(
        web_contents_,
        "window.domAutomationController.send(!!(" + js_ + "));",
        &result));
    return result;
  }

  void OnTimer() {
    DCHECK(runner_);
    if (CheckJs())
      runner_->Quit();
  }

  content::WebContents* web_contents_;
  const std::string js_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(JsConditionWaiter);
};

}  // namespace

// Helper class that monitors app windows to wait for a window to appear.
class AppWindowObserver : public apps::AppWindowRegistry::Observer {
 public:
  AppWindowObserver(apps::AppWindowRegistry* registry,
                    const std::string& app_id)
      : registry_(registry), app_id_(app_id), window_(NULL), running_(false) {
    registry_->AddObserver(this);
  }
  virtual ~AppWindowObserver() { registry_->RemoveObserver(this); }

  apps::AppWindow* Wait() {
    running_ = true;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(window_);
    return window_;
  }

  // AppWindowRegistry::Observer
  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE {
    if (!running_)
      return;

    if (app_window->extension_id() == app_id_) {
      window_ = app_window;
      message_loop_runner_->Quit();
      running_ = false;
    }
  }
  virtual void OnAppWindowIconChanged(apps::AppWindow* app_window) OVERRIDE {}
  virtual void OnAppWindowRemoved(apps::AppWindow* app_window) OVERRIDE {}

 private:
  apps::AppWindowRegistry* registry_;
  std::string app_id_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  apps::AppWindow* window_;
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowObserver);
};

class KioskTest : public OobeBaseTest {
 public:
  KioskTest() {
    set_exit_when_last_browser_closes(false);
  }

  virtual ~KioskTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    test_app_id_ = kTestKioskApp;
    mock_user_manager_.reset(new MockUserManager);
    AppLaunchController::SkipSplashWaitForTesting();
    AppLaunchController::SetNetworkWaitForTesting(kTestNetworkTimeoutSeconds);

    OobeBaseTest::SetUp();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    AppLaunchController::SetNetworkTimeoutCallbackForTesting(NULL);
    AppLaunchSigninScreen::SetUserManagerForTesting(NULL);

    OobeBaseTest::CleanUpOnMainThread();

    // Clean up while main thread still runs.
    // See http://crbug.com/176659.
    KioskAppManager::Get()->CleanUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    OobeBaseTest::SetUpCommandLine(command_line);

    // Create gaia and webstore URL from test server url but using different
    // host names. This is to avoid gaia response being tagged as from
    // webstore in chrome_resource_dispatcher_host_delegate.cc.
    GURL webstore_url = GetTestWebstoreUrl();
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryURL,
        webstore_url.Resolve("/chromeos/app_mode/webstore").spec());
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryDownloadURL,
        webstore_url.Resolve(
            "/chromeos/app_mode/webstore/downloads/%s.crx").spec());
  }

  GURL GetTestWebstoreUrl() {
    const GURL& server_url = embedded_test_server()->base_url();
    std::string webstore_host("webstore");
    GURL::Replacements replace_webstore_host;
    replace_webstore_host.SetHostStr(webstore_host);
    return server_url.ReplaceComponents(replace_webstore_host);
  }

  void LaunchApp(const std::string& app_id, bool diagnostic_mode) {
    bool new_kiosk_ui = !CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kDisableNewKioskUI);
    GetLoginUI()->CallJavascriptFunction(new_kiosk_ui ?
        kLaunchAppForTestNewAPI : kLaunchAppForTestOldAPI,
        base::StringValue(app_id),
        base::FundamentalValue(diagnostic_mode));
  }

  void ReloadKioskApps() {
    // Remove then add to ensure NOTIFICATION_KIOSK_APPS_LOADED fires.
    KioskAppManager::Get()->RemoveApp(test_app_id_);
    KioskAppManager::Get()->AddApp(test_app_id_);
  }

  void ReloadAutolaunchKioskApps() {
    KioskAppManager::Get()->AddApp(test_app_id_);
    KioskAppManager::Get()->SetAutoLaunchApp(test_app_id_);
  }

  void PrepareAppLaunch() {
    EnableConsumerKioskMode();

    // Start UI
    content::WindowedNotificationObserver login_signal(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
    if (wizard_controller) {
      wizard_controller->SkipToLoginForTesting(LoginScreenContext());
      login_signal.Wait();
    } else {
      // No wizard and running with an existing profile and it should land
      // on account picker.
      OobeScreenWaiter(OobeDisplay::SCREEN_ACCOUNT_PICKER).Wait();
    }

    // Wait for the Kiosk App configuration to reload.
    content::WindowedNotificationObserver apps_loaded_signal(
        chrome::NOTIFICATION_KIOSK_APPS_LOADED,
        content::NotificationService::AllSources());
    ReloadKioskApps();
    apps_loaded_signal.Wait();
  }

  void StartAppLaunchFromLoginScreen(const base::Closure& network_setup_cb) {
    PrepareAppLaunch();

    if (!network_setup_cb.is_null())
      network_setup_cb.Run();

    LaunchApp(test_app_id(), false);
  }

  const extensions::Extension* GetInstalledApp() {
    Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
    return extensions::ExtensionSystem::Get(app_profile)->
        extension_service()->GetInstalledExtension(test_app_id_);
  }

  const Version& GetInstalledAppVersion() {
    return *GetInstalledApp()->version();
  }

  void WaitForAppLaunchSuccess() {
    ExtensionTestMessageListener
        launch_data_check_listener("launchData.isKioskSession = true", false);

    // Wait for the Kiosk App to launch.
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
        content::NotificationService::AllSources()).Wait();

    // Default profile switches to app profile after app is launched.
    Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
    ASSERT_TRUE(app_profile);

    // Check installer status.
    EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
              chromeos::KioskAppLaunchError::Get());

    // Check if the kiosk webapp is really installed for the default profile.
    const extensions::Extension* app =
        extensions::ExtensionSystem::Get(app_profile)->
        extension_service()->GetInstalledExtension(test_app_id_);
    EXPECT_TRUE(app);

    // App should appear with its window.
    apps::AppWindowRegistry* app_window_registry =
        apps::AppWindowRegistry::Get(app_profile);
    apps::AppWindow* window =
        AppWindowObserver(app_window_registry, test_app_id_).Wait();
    EXPECT_TRUE(window);

    // Login screen should be gone or fading out.
    chromeos::LoginDisplayHost* login_display_host =
        chromeos::LoginDisplayHostImpl::default_host();
    EXPECT_TRUE(
        login_display_host == NULL ||
        login_display_host->GetNativeWindow()->layer()->GetTargetOpacity() ==
            0.0f);

    // Wait until the app terminates if it is still running.
    if (!app_window_registry->GetAppWindowsForApp(test_app_id_).empty())
      content::RunMessageLoop();

    // Check that the app had been informed that it is running in a kiosk
    // session.
    EXPECT_TRUE(launch_data_check_listener.was_satisfied());
  }

  void WaitForAppLaunchNetworkTimeout() {
    if (GetAppLaunchController()->network_wait_timedout())
      return;

    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;

    base::Closure callback = base::Bind(
        &OnNetworkWaitTimedOut, runner->QuitClosure());
    AppLaunchController::SetNetworkTimeoutCallbackForTesting(&callback);

    runner->Run();

    CHECK(GetAppLaunchController()->network_wait_timedout());
    AppLaunchController::SetNetworkTimeoutCallbackForTesting(NULL);
  }

  void EnableConsumerKioskMode() {
    scoped_ptr<bool> locked(new bool(false));
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    KioskAppManager::Get()->EnableConsumerKioskAutoLaunch(
        base::Bind(&ConsumerKioskModeAutoStartLockCheck,
                   locked.get(),
                   runner->QuitClosure()));
    runner->Run();
    EXPECT_TRUE(*locked.get());
  }

  KioskAppManager::ConsumerKioskAutoLaunchStatus
  GetConsumerKioskModeStatus() {
    KioskAppManager::ConsumerKioskAutoLaunchStatus status =
        static_cast<KioskAppManager::ConsumerKioskAutoLaunchStatus>(-1);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
        base::Bind(&ConsumerKioskAutoLaunchStatusCheck,
                   &status,
                   runner->QuitClosure()));
    runner->Run();
    CHECK_NE(status,
             static_cast<KioskAppManager::ConsumerKioskAutoLaunchStatus>(-1));
    return status;
  }

  // Copies the app profile from |relative_app_profile_dir| from test directory
  // to the app profile directory (assuming "user") under testing profile. This
  // is for that needs to have a kiosk app already installed from a previous
  // run. Note this must be called before app profile is loaded.
  void SetupAppProfile(const std::string& relative_app_profile_dir) {
    base::FilePath app_profile_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &app_profile_dir));
    app_profile_dir = app_profile_dir.AppendASCII("user");
    ASSERT_TRUE(base::CreateDirectory(app_profile_dir));

    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_data_dir = test_data_dir.AppendASCII(relative_app_profile_dir);
    ASSERT_TRUE(base::CopyFile(test_data_dir.AppendASCII("Preferences"),
                               app_profile_dir.AppendASCII("Preferences")));
    ASSERT_TRUE(
        base::CopyDirectory(test_data_dir.AppendASCII("Extensions"),
                            app_profile_dir,
                            true));
  }

  void RunAppLaunchNetworkDownTest() {
    // Mock network could be configured with owner's password.
    ScopedCanConfigureNetwork can_configure_network(true, true);

    // Start app launch and wait for network connectivity timeout.
    StartAppLaunchFromLoginScreen(SimulateNetworkOfflineClosure());
    OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
    splash_waiter.Wait();
    WaitForAppLaunchNetworkTimeout();

    // Configure network link should be visible.
    JsExpect("$('splash-config-network').hidden == false");

    // Set up fake user manager with an owner for the test.
    mock_user_manager()->SetActiveUser(kTestOwnerEmail);
    AppLaunchSigninScreen::SetUserManagerForTesting(mock_user_manager());
    static_cast<LoginDisplayHostImpl*>(LoginDisplayHostImpl::default_host())
        ->GetOobeUI()->ShowOobeUI(false);

    // Configure network should bring up lock screen for owner.
    OobeScreenWaiter lock_screen_waiter(OobeDisplay::SCREEN_ACCOUNT_PICKER);
    static_cast<AppLaunchSplashScreenActor::Delegate*>(GetAppLaunchController())
        ->OnConfigureNetwork();
    lock_screen_waiter.Wait();

    // There should be only one owner pod on this screen.
    JsExpect("$('pod-row').isSinglePod");

    // A network error screen should be shown after authenticating.
    OobeScreenWaiter error_screen_waiter(OobeDisplay::SCREEN_ERROR_MESSAGE);
    static_cast<AppLaunchSigninScreen::Delegate*>(GetAppLaunchController())
        ->OnOwnerSigninSuccess();
    error_screen_waiter.Wait();

    ASSERT_TRUE(GetAppLaunchController()->showing_network_dialog());

    SimulateNetworkOnline();
    WaitForAppLaunchSuccess();
  }

  AppLaunchController* GetAppLaunchController() {
    return chromeos::LoginDisplayHostImpl::default_host()
        ->GetAppLaunchController();
  }

  MockUserManager* mock_user_manager() { return mock_user_manager_.get(); }

  void set_test_app_id(const std::string& test_app_id) {
    test_app_id_ = test_app_id;
  }
  const std::string& test_app_id() const { return test_app_id_; }

 private:
  std::string test_app_id_;
  scoped_ptr<MockUserManager> mock_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(KioskTest);
};

IN_PROC_BROWSER_TEST_F(KioskTest, InstallAndLaunchApp) {
  StartAppLaunchFromLoginScreen(SimulateNetworkOnlineClosure());
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, PRE_LaunchAppNetworkDown) {
  // Tests the network down case for the initial app download and launch.
  RunAppLaunchNetworkDownTest();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppNetworkDown) {
  // Tests the network down case for launching an existing app that is
  // installed in PRE_LaunchAppNetworkDown.
  RunAppLaunchNetworkDownTest();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppNetworkDownConfigureNotAllowed) {
  // Mock network could not be configured.
  ScopedCanConfigureNetwork can_configure_network(false, true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(SimulateNetworkOfflineClosure());
  OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
  splash_waiter.Wait();
  WaitForAppLaunchNetworkTimeout();

  // Configure network link should not be visible.
  JsExpect("$('splash-config-network').hidden == true");

  // Network becomes online and app launch is resumed.
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppNetworkPortal) {
  // Mock network could be configured without the owner password.
  ScopedCanConfigureNetwork can_configure_network(true, false);

  // Start app launch with network portal state.
  StartAppLaunchFromLoginScreen(SimulateNetworkPortalClosure());
  OobeScreenWaiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH)
      .WaitNoAssertCurrentScreen();
  WaitForAppLaunchNetworkTimeout();

  // Network error should show up automatically since this test does not
  // require owner auth to configure network.
  OobeScreenWaiter(OobeDisplay::SCREEN_ERROR_MESSAGE).Wait();

  ASSERT_TRUE(GetAppLaunchController()->showing_network_dialog());
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchAppUserCancel) {
  StartAppLaunchFromLoginScreen(SimulateNetworkOfflineClosure());
  OobeScreenWaiter splash_waiter(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH);
  splash_waiter.Wait();

  CrosSettings::Get()->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled, true);
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("app_launch_bailout"));
  signal.Wait();
  EXPECT_EQ(chromeos::KioskAppLaunchError::USER_CANCEL,
            chromeos::KioskAppLaunchError::Get());
}

IN_PROC_BROWSER_TEST_F(KioskTest, LaunchInDiagnosticMode) {
  PrepareAppLaunch();
  SimulateNetworkOnline();

  LaunchApp(kTestKioskApp, true);

  content::WebContents* login_contents = GetLoginUI()->GetWebContents();

  bool new_kiosk_ui = !CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewKioskUI);
  JsConditionWaiter(login_contents, new_kiosk_ui ?
      kCheckDiagnosticModeNewAPI : kCheckDiagnosticModeOldAPI).Wait();

  std::string diagnosticMode(new_kiosk_ui ?
      kCheckDiagnosticModeNewAPI : kCheckDiagnosticModeOldAPI);
  ASSERT_TRUE(content::ExecuteScript(
      login_contents,
      "(function() {"
         "var e = new Event('click');" +
         diagnosticMode + "."
             "okButton_.dispatchEvent(e);"
      "})();"));

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, AutolaunchWarningCancel) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  ReloadAutolaunchKioskApps();
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.AutolaunchScreen.confirmAutoLaunchForTesting",
      base::FundamentalValue(false));

  // Wait for the auto launch warning to go away.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());
}

IN_PROC_BROWSER_TEST_F(KioskTest, AutolaunchWarningConfirm) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.AutolaunchScreen.confirmAutoLaunchForTesting",
      base::FundamentalValue(true));

  // Wait for the auto launch warning to go away.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_TRUE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableCancel) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // Wait for the kiosk_enable screen to show and cancel the screen.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.KioskEnableScreen.enableKioskForTesting",
      base::FundamentalValue(false));

  // Wait for the kiosk_enable screen to disappear.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  // Check that the status still says configurable.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableConfirmed) {
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // Wait for the kiosk_enable screen to show and cancel the screen.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.KioskEnableScreen.enableKioskForTesting",
      base::FundamentalValue(true));

  // Wait for the signal that indicates Kiosk Mode is enabled.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLED,
      content::NotificationService::AllSources()).Wait();
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableAbortedWithAutoEnrollment) {
  // Fake an auto enrollment is going to be enforced.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnrollmentInitialModulus, "1");
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnrollmentModulusLimit, "2");
  g_browser_process->local_state()->SetBoolean(prefs::kShouldAutoEnroll, true);
  g_browser_process->local_state()->SetInteger(
      prefs::kAutoEnrollmentPowerLimit, 3);

  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // The flow should be aborted due to auto enrollment enforcement.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  GetSigninScreenHandler()->set_kiosk_enable_flow_aborted_callback_for_test(
      runner->QuitClosure());
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(KioskTest, KioskEnableAfter2ndSigninScreen) {
  // Fake an auto enrollment is not going to be enforced.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnrollmentInitialModulus, "1");
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnrollmentModulusLimit, "2");
  g_browser_process->local_state()->SetBoolean(prefs::kShouldAutoEnroll, false);
  g_browser_process->local_state()->SetInteger(
      prefs::kAutoEnrollmentPowerLimit, -1);

  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // Wait for the kiosk_enable screen to show and cancel the screen.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.KioskEnableScreen.enableKioskForTesting",
      base::FundamentalValue(false));

  // Wait for the kiosk_enable screen to disappear.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  // Show signin screen again.
  chromeos::LoginDisplayHostImpl::default_host()->StartSignInScreen(
      LoginScreenContext());
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();

  // Show kiosk enable screen again.
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // And it should show up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
}

class KioskUpdateTest : public KioskTest {
 public:
  KioskUpdateTest() {}
  virtual ~KioskUpdateTest() {}

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Needs background networking so that ExtensionDownloader works.
    needs_background_networking_ = true;

    KioskTest::SetUpCommandLine(command_line);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    KioskTest::SetUpOnMainThread();

    GURL webstore_url = GetTestWebstoreUrl();
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kAppsGalleryUpdateURL,
        webstore_url.Resolve("/update_check.xml").spec());

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&KioskUpdateTest::HandleRequest,
                   base::Unretained(this)));
  }

  void SetUpdateCheckContent(const std::string& update_check_file,
                             const std::string& app_id,
                             const GURL& crx_download_url,
                             const std::string& crx_fp,
                             const std::string& crx_size,
                             const std::string& version) {
    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    base::FilePath update_file =
        test_data_dir.AppendASCII(update_check_file.c_str());
    ASSERT_TRUE(base::ReadFileToString(update_file, &update_check_content_));

    ReplaceSubstringsAfterOffset(&update_check_content_, 0, "$AppId", app_id);
    ReplaceSubstringsAfterOffset(
        &update_check_content_, 0, "$CrxDownloadUrl", crx_download_url.spec());
    ReplaceSubstringsAfterOffset(&update_check_content_, 0, "$FP", crx_fp);
    ReplaceSubstringsAfterOffset(&update_check_content_, 0, "$Size", crx_size);
    ReplaceSubstringsAfterOffset(
        &update_check_content_, 0, "$Version", version);
  }

 private:
  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
    std::string request_path = request_url.path();
    if (!update_check_content_.empty() &&
        request_path == "/update_check.xml") {
      scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/xml");
      http_response->set_content(update_check_content_);
      return http_response.PassAs<HttpResponse>();
    }

    return scoped_ptr<HttpResponse>();
  }

  std::string update_check_content_;

  DISALLOW_COPY_AND_ASSIGN(KioskUpdateTest);
};

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppNoNetwork) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  SetupAppProfile("chromeos/app_mode/offline_enabled_app_profile");

  PrepareAppLaunch();
  SimulateNetworkOffline();

  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppNoUpdate) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  SetupAppProfile("chromeos/app_mode/offline_enabled_app_profile");

  SetUpdateCheckContent(
      "chromeos/app_mode/webstore/update_check/no_update.xml",
      kTestOfflineEnabledKioskApp,
      GURL(),
      "",
      "",
      "");

  PrepareAppLaunch();
  SimulateNetworkOnline();

  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  EXPECT_EQ("1.0.0", GetInstalledAppVersion().GetString());
}

IN_PROC_BROWSER_TEST_F(KioskUpdateTest, LaunchOfflineEnabledAppHasUpdate) {
  set_test_app_id(kTestOfflineEnabledKioskApp);
  SetupAppProfile("chromeos/app_mode/offline_enabled_app_profile");

  GURL webstore_url = GetTestWebstoreUrl();
  GURL crx_download_url = webstore_url.Resolve(
      "/chromeos/app_mode/webstore/downloads/"
      "ajoggoflpgplnnjkjamcmbepjdjdnpdp.crx");

  SetUpdateCheckContent(
      "chromeos/app_mode/webstore/update_check/has_update.xml",
      kTestOfflineEnabledKioskApp,
      crx_download_url,
      "ca08d1d120429f49a2b5b1d4db67ce4234390f0758b580e25fba5226a0526209",
      "2294",
      "2.0.0");

  PrepareAppLaunch();
  SimulateNetworkOnline();

  LaunchApp(test_app_id(), false);
  WaitForAppLaunchSuccess();

  EXPECT_EQ("2.0.0", GetInstalledAppVersion().GetString());
}

class KioskEnterpriseTest : public KioskTest {
 protected:
  KioskEnterpriseTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    device_policy_test_helper_.MarkAsEnterpriseOwned();
    device_policy_test_helper_.InstallOwnerKey();

    KioskTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    KioskTest::SetUpOnMainThread();
    // Configure kTestEnterpriseKioskApp in device policy.
    em::DeviceLocalAccountsProto* accounts =
        device_policy_test_helper_.device_policy()->payload()
            .mutable_device_local_accounts();
    em::DeviceLocalAccountInfoProto* account = accounts->add_account();
    account->set_account_id(kTestEnterpriseAccountId);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(kTestEnterpriseKioskApp);
    accounts->set_auto_login_id(kTestEnterpriseAccountId);
    em::PolicyData& policy_data =
        device_policy_test_helper_.device_policy()->policy_data();
    policy_data.set_service_account_identity(kTestEnterpriseServiceAccountId);
    device_policy_test_helper_.device_policy()->Build();
    DBusThreadManager::Get()->GetSessionManagerClient()->StoreDevicePolicy(
        device_policy_test_helper_.device_policy()->GetBlob(),
        base::Bind(&KioskEnterpriseTest::StorePolicyCallback));

    DeviceSettingsService::Get()->Load();

    // Configure OAuth authentication.
    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

    // This token satisfies the userinfo.email request from
    // DeviceOAuth2TokenService used in token validation.
    FakeGaia::AccessTokenInfo userinfo_token_info;
    userinfo_token_info.token = kTestUserinfoToken;
    userinfo_token_info.scopes.insert(
        "https://www.googleapis.com/auth/userinfo.email");
    userinfo_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_token_info.email = kTestEnterpriseServiceAccountId;
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, userinfo_token_info);

    // The any-api access token for accessing the token minting endpoint.
    FakeGaia::AccessTokenInfo login_token_info;
    login_token_info.token = kTestLoginToken;
    login_token_info.scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
    login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, login_token_info);

    // This is the access token requested by the app via the identity API.
    FakeGaia::AccessTokenInfo access_token_info;
    access_token_info.token = kTestAccessToken;
    access_token_info.scopes.insert(kTestAppScope);
    access_token_info.audience = kTestClientId;
    access_token_info.email = kTestEnterpriseServiceAccountId;
    fake_gaia_->IssueOAuthToken(kTestLoginToken, access_token_info);

    DeviceOAuth2TokenService* token_service = NULL;
    DeviceOAuth2TokenServiceFactory::Get(
        base::Bind(&CopyTokenService, &token_service));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(token_service);
    token_service->SetAndSaveRefreshToken(kTestRefreshToken);
  }

  static void StorePolicyCallback(bool result) {
    ASSERT_TRUE(result);
  }

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskEnterpriseTest);
};

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest, EnterpriseKioskApp) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  // Wait for the Kiosk App configuration to reload, then launch the app.
  KioskAppManager::App app;
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APPS_LOADED,
      base::Bind(&KioskAppManager::GetApp,
                 base::Unretained(KioskAppManager::Get()),
                 kTestEnterpriseKioskApp, &app)).Wait();

  LaunchApp(kTestEnterpriseKioskApp, false);

  // Wait for the Kiosk App to launch.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources()).Wait();

  // Check installer status.
  EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
            chromeos::KioskAppLaunchError::Get());

  // Wait for the window to appear.
  apps::AppWindow* window =
      AppWindowObserver(
          apps::AppWindowRegistry::Get(ProfileManager::GetPrimaryUserProfile()),
          kTestEnterpriseKioskApp).Wait();
  ASSERT_TRUE(window);

  // Check whether the app can retrieve an OAuth2 access token.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      window->web_contents(),
      "chrome.identity.getAuthToken({ 'interactive': false }, function(token) {"
      "    window.domAutomationController.setAutomationId(0);"
      "    window.domAutomationController.send(token);"
      "});",
      &result));
  EXPECT_EQ(kTestAccessToken, result);

  // Terminate the app.
  window->GetBaseWindow()->Close();
  content::RunAllPendingInMessageLoop();
}

// Specialized test fixture for testing kiosk mode on the
// hidden WebUI initialization flow for slow hardware.
class KioskHiddenWebUITest : public KioskTest,
                             public ash::DesktopBackgroundControllerObserver {
 public:
  KioskHiddenWebUITest() : wallpaper_loaded_(false) {}

  // KioskTest overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    KioskTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kDeviceRegistered, "1");
    command_line->AppendSwitch(switches::kDisableBootAnimation);
    command_line->AppendSwitch(switches::kDisableOobeAnimation);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    KioskTest::SetUpOnMainThread();
    ash::Shell::GetInstance()->desktop_background_controller()
        ->AddObserver(this);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ash::Shell::GetInstance()->desktop_background_controller()
        ->RemoveObserver(this);
    KioskTest::TearDownOnMainThread();
  }

  void WaitForWallpaper() {
    if (!wallpaper_loaded_) {
      runner_ = new content::MessageLoopRunner;
      runner_->Run();
    }
  }

  bool wallpaper_loaded() const { return wallpaper_loaded_; }

  // ash::DesktopBackgroundControllerObserver overrides:
  virtual void OnWallpaperDataChanged() OVERRIDE {
    wallpaper_loaded_ = true;
    if (runner_.get())
      runner_->Quit();
  }

  bool wallpaper_loaded_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(KioskHiddenWebUITest);
};

IN_PROC_BROWSER_TEST_F(KioskHiddenWebUITest, AutolaunchWarning) {
  // Add a device owner.
  FakeUserManager* user_manager = new FakeUserManager();
  user_manager->AddUser(kTestOwnerEmail);
  ScopedUserManagerEnabler enabler(user_manager);

  // Set kiosk app to autolaunch.
  EnableConsumerKioskMode();
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  ReloadAutolaunchKioskApps();
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();

  // Wait for the wallpaper to load.
  WaitForWallpaper();
  EXPECT_TRUE(wallpaper_loaded());
}

}  // namespace chromeos
