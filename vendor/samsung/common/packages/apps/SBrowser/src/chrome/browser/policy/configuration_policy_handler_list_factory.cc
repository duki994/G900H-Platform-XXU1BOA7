// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/net/proxy_policy_handler.h"
#include "chrome/browser/profiles/incognito_mode_policy_handler.h"
#include "chrome/browser/search_engines/default_search_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/autofill_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#if defined(ENABLE_TRANSLATE)
#include "components/translate/core/common/translate_pref_names.h"
#endif
#include "grit/component_strings.h"
#include "policy/policy_constants.h"

#if !defined(OS_IOS)
#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "chrome/browser/net/disk_cache_dir_policy_handler.h"
#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"
#include "chrome/browser/policy/javascript_policy_handler.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/browser/sync/sync_policy_handler.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/magnifier/magnifier_constants.h"
#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"
#include "chromeos/dbus/power_policy_controller.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
#include "chrome/browser/policy/configuration_policy_handler_android.h"
#endif  // defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
#include "chrome/browser/download/download_dir_policy_handler.h"
#endif

#if !defined(OS_MACOSX) && !defined(OS_IOS)
#include "apps/pref_names.h"
#endif

namespace policy {

namespace {

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { key::kHomepageLocation,
    prefs::kHomePage,
    base::Value::TYPE_STRING },
  { key::kHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage,
    base::Value::TYPE_BOOLEAN },
  { key::kRestoreOnStartupURLs,
    prefs::kURLsToRestoreOnStartup,
    base::Value::TYPE_LIST },
  { key::kAlternateErrorPagesEnabled,
    prefs::kAlternateErrorPagesEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDnsPrefetchingEnabled,
    prefs::kNetworkPredictionEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kBuiltInDnsClientEnabled,
    prefs::kBuiltInDnsClientEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDisableSpdy,
    prefs::kDisableSpdy,
    base::Value::TYPE_BOOLEAN },
  { key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kForceSafeSearch,
    prefs::kForceSafeSearch,
    base::Value::TYPE_BOOLEAN },
  { key::kPasswordManagerEnabled,
    prefs::kPasswordManagerEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kPasswordManagerAllowShowPasswords,
    prefs::kPasswordManagerAllowShowPasswords,
    base::Value::TYPE_BOOLEAN },
  { key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDisablePrintPreview,
    prefs::kPrintPreviewDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kMetricsReportingEnabled,
    prefs::kMetricsReportingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kApplicationLocaleValue,
    prefs::kApplicationLocale,
    base::Value::TYPE_STRING },
  { key::kDisabledPlugins,
    prefs::kPluginsDisabledPlugins,
    base::Value::TYPE_LIST },
  { key::kDisabledPluginsExceptions,
    prefs::kPluginsDisabledPluginsExceptions,
    base::Value::TYPE_LIST },
  { key::kEnabledPlugins,
    prefs::kPluginsEnabledPlugins,
    base::Value::TYPE_LIST },
  { key::kShowHomeButton,
    prefs::kShowHomeButton,
    base::Value::TYPE_BOOLEAN },
  { key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowDeletingBrowserHistory,
    prefs::kAllowDeletingBrowserHistory,
    base::Value::TYPE_BOOLEAN },
  { key::kDeveloperToolsDisabled,
    prefs::kDevToolsDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kBlockThirdPartyCookies,
    prefs::kBlockThirdPartyCookies,
    base::Value::TYPE_BOOLEAN },
  { key::kDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultPluginsSetting,
    prefs::kManagedDefaultPluginsSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    base::Value::TYPE_INTEGER },
  { key::kAutoSelectCertificateForUrls,
    prefs::kManagedAutoSelectCertificateForUrls,
    base::Value::TYPE_LIST },
  { key::kCookiesAllowedForUrls,
    prefs::kManagedCookiesAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kCookiesBlockedForUrls,
    prefs::kManagedCookiesBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kCookiesSessionOnlyForUrls,
    prefs::kManagedCookiesSessionOnlyForUrls,
    base::Value::TYPE_LIST },
  { key::kImagesAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kImagesBlockedForUrls,
    prefs::kManagedImagesBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kPluginsAllowedForUrls,
    prefs::kManagedPluginsAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kPluginsBlockedForUrls,
    prefs::kManagedPluginsBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kNotificationsAllowedForUrls,
    prefs::kManagedNotificationsAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kNotificationsBlockedForUrls,
    prefs::kManagedNotificationsBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kDefaultNotificationsSetting,
    prefs::kManagedDefaultNotificationsSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultGeolocationSetting,
    prefs::kManagedDefaultGeolocationSetting,
    base::Value::TYPE_INTEGER },
  { key::kSigninAllowed,
    prefs::kSigninAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kEnableOriginBoundCerts,
    prefs::kEnableOriginBoundCerts,
    base::Value::TYPE_BOOLEAN },
  { key::kDisableSSLRecordSplitting,
    prefs::kDisableSSLRecordSplitting,
    base::Value::TYPE_BOOLEAN },
  { key::kEnableOnlineRevocationChecks,
    prefs::kCertRevocationCheckingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kRequireOnlineRevocationChecksForLocalAnchors,
    prefs::kCertRevocationCheckingRequiredLocalAnchors,
    base::Value::TYPE_BOOLEAN },
  { key::kAuthSchemes,
    prefs::kAuthSchemes,
    base::Value::TYPE_STRING },
  { key::kDisableAuthNegotiateCnameLookup,
    prefs::kDisableAuthNegotiateCnameLookup,
    base::Value::TYPE_BOOLEAN },
  { key::kEnableAuthNegotiatePort,
    prefs::kEnableAuthNegotiatePort,
    base::Value::TYPE_BOOLEAN },
  { key::kAuthServerWhitelist,
    prefs::kAuthServerWhitelist,
    base::Value::TYPE_STRING },
  { key::kAuthNegotiateDelegateWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist,
    base::Value::TYPE_STRING },
  { key::kGSSAPILibraryName,
    prefs::kGSSAPILibraryName,
    base::Value::TYPE_STRING },
  { key::kAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt,
    base::Value::TYPE_BOOLEAN },
  { key::kDisable3DAPIs,
    prefs::kDisable3DAPIs,
    base::Value::TYPE_BOOLEAN },
  { key::kDisablePluginFinder,
    prefs::kDisablePluginFinder,
    base::Value::TYPE_BOOLEAN },
  { key::kDiskCacheSize,
    prefs::kDiskCacheSize,
    base::Value::TYPE_INTEGER },
  { key::kMediaCacheSize,
    prefs::kMediaCacheSize,
    base::Value::TYPE_INTEGER },
  { key::kPolicyRefreshRate,
    policy_prefs::kUserPolicyRefreshRate,
    base::Value::TYPE_INTEGER },
  { key::kDevicePolicyRefreshRate,
    prefs::kDevicePolicyRefreshRate,
    base::Value::TYPE_INTEGER },
  { key::kDefaultBrowserSettingEnabled,
    prefs::kDefaultBrowserSettingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostFirewallTraversal,
    prefs::kRemoteAccessHostFirewallTraversal,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostRequireTwoFactor,
    prefs::kRemoteAccessHostRequireTwoFactor,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostDomain,
    prefs::kRemoteAccessHostDomain,
    base::Value::TYPE_STRING },
  { key::kRemoteAccessHostTalkGadgetPrefix,
    prefs::kRemoteAccessHostTalkGadgetPrefix,
    base::Value::TYPE_STRING },
  { key::kRemoteAccessHostRequireCurtain,
    prefs::kRemoteAccessHostRequireCurtain,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostAllowClientPairing,
    prefs::kRemoteAccessHostAllowClientPairing,
    base::Value::TYPE_BOOLEAN },
  { key::kCloudPrintProxyEnabled,
    prefs::kCloudPrintProxyEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kCloudPrintSubmitEnabled,
    prefs::kCloudPrintSubmitEnabled,
    base::Value::TYPE_BOOLEAN },
#if defined(ENABLE_TRANSLATE)
  { key::kTranslateEnabled,
    prefs::kEnableTranslate,
    base::Value::TYPE_BOOLEAN },
#endif
  { key::kAllowOutdatedPlugins,
    prefs::kPluginsAllowOutdated,
    base::Value::TYPE_BOOLEAN },
  { key::kAlwaysAuthorizePlugins,
    prefs::kPluginsAlwaysAuthorize,
    base::Value::TYPE_BOOLEAN },
  { key::kBookmarkBarEnabled,
    prefs::kShowBookmarkBar,
    base::Value::TYPE_BOOLEAN },
  { key::kEditBookmarksEnabled,
    prefs::kEditBookmarksEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowFileSelectionDialogs,
    prefs::kAllowFileSelectionDialogs,
    base::Value::TYPE_BOOLEAN },
  { key::kImportBookmarks,
    prefs::kImportBookmarks,
    base::Value::TYPE_BOOLEAN },
  { key::kImportHistory,
    prefs::kImportHistory,
    base::Value::TYPE_BOOLEAN },
  { key::kImportHomepage,
    prefs::kImportHomepage,
    base::Value::TYPE_BOOLEAN },
  { key::kImportSearchEngine,
    prefs::kImportSearchEngine,
    base::Value::TYPE_BOOLEAN },
  { key::kImportSavedPasswords,
    prefs::kImportSavedPasswords,
    base::Value::TYPE_BOOLEAN },
  { key::kMaxConnectionsPerProxy,
    prefs::kMaxConnectionsPerProxy,
    base::Value::TYPE_INTEGER },
  { key::kURLWhitelist,
    policy_prefs::kUrlWhitelist,
    base::Value::TYPE_LIST },
  { key::kEnableMemoryInfo,
    prefs::kEnableMemoryInfo,
    base::Value::TYPE_BOOLEAN },
  { key::kRestrictSigninToPattern,
    prefs::kGoogleServicesUsernamePattern,
    base::Value::TYPE_STRING },
  { key::kDefaultMediaStreamSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    base::Value::TYPE_INTEGER },
  { key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSpellCheckServiceEnabled,
    prefs::kSpellCheckUseSpellingService,
    base::Value::TYPE_BOOLEAN },
  { key::kDisableScreenshots,
    prefs::kDisableScreenshots,
    base::Value::TYPE_BOOLEAN },
  { key::kAudioCaptureAllowed,
    prefs::kAudioCaptureAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kVideoCaptureAllowed,
    prefs::kVideoCaptureAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kAudioCaptureAllowedUrls,
    prefs::kAudioCaptureAllowedUrls,
    base::Value::TYPE_LIST },
  { key::kVideoCaptureAllowedUrls,
    prefs::kVideoCaptureAllowedUrls,
    base::Value::TYPE_LIST },
  { key::kHideWebStoreIcon,
    prefs::kHideWebStoreIcon,
    base::Value::TYPE_BOOLEAN },
  { key::kVariationsRestrictParameter,
    prefs::kVariationsRestrictParameter,
    base::Value::TYPE_STRING },
  { key::kSupervisedUserCreationEnabled,
    prefs::kManagedUserCreationAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kForceEphemeralProfiles,
    prefs::kForceEphemeralProfiles,
    base::Value::TYPE_BOOLEAN },

#if !defined(OS_MACOSX) && !defined(OS_IOS)
  { key::kFullscreenAllowed,
    prefs::kFullscreenAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kFullscreenAllowed,
    apps::prefs::kAppFullscreenAllowed,
    base::Value::TYPE_BOOLEAN },
#endif  // !defined(OS_MACOSX) && !defined(OS_IOS)

#if defined(OS_CHROMEOS)
  { key::kChromeOsLockOnIdleSuspend,
    prefs::kEnableAutoScreenLock,
    base::Value::TYPE_BOOLEAN },
  { key::kChromeOsReleaseChannel,
    prefs::kChromeOsReleaseChannel,
    base::Value::TYPE_STRING },
  { key::kDriveDisabled,
    prefs::kDisableDrive,
    base::Value::TYPE_BOOLEAN },
  { key::kDriveDisabledOverCellular,
    prefs::kDisableDriveOverCellular,
    base::Value::TYPE_BOOLEAN },
  { key::kExternalStorageDisabled,
    prefs::kExternalStorageDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kAudioOutputAllowed,
    prefs::kAudioOutputAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kShowLogoutButtonInTray,
    prefs::kShowLogoutButtonInTray,
    base::Value::TYPE_BOOLEAN },
  { key::kShelfAutoHideBehavior,
    prefs::kShelfAutoHideBehaviorLocal,
    base::Value::TYPE_STRING },
  { key::kSessionLengthLimit,
    prefs::kSessionLengthLimit,
    base::Value::TYPE_INTEGER },
  { key::kWaitForInitialUserActivity,
    prefs::kSessionWaitForInitialUserActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kPowerManagementUsesAudioActivity,
    prefs::kPowerUseAudioActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kPowerManagementUsesVideoActivity,
    prefs::kPowerUseVideoActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowScreenWakeLocks,
    prefs::kPowerAllowScreenWakeLocks,
    base::Value::TYPE_BOOLEAN },
  { key::kWaitForInitialUserActivity,
    prefs::kPowerWaitForInitialUserActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kTermsOfServiceURL,
    prefs::kTermsOfServiceURL,
    base::Value::TYPE_STRING },
  { key::kShowAccessibilityOptionsInSystemTrayMenu,
    prefs::kShouldAlwaysShowAccessibilityMenu,
    base::Value::TYPE_BOOLEAN },
  { key::kLargeCursorEnabled,
    prefs::kLargeCursorEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSpokenFeedbackEnabled,
    prefs::kSpokenFeedbackEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kHighContrastEnabled,
    prefs::kHighContrastEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kVirtualKeyboardEnabled,
    prefs::kVirtualKeyboardEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultLargeCursorEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultHighContrastEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kRebootAfterUpdate,
    prefs::kRebootAfterUpdate,
    base::Value::TYPE_BOOLEAN },
  { key::kAttestationEnabledForUser,
    prefs::kAttestationEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kChromeOsMultiProfileUserBehavior,
    prefs::kMultiProfileUserBehavior,
    base::Value::TYPE_STRING },
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    base::Value::TYPE_BOOLEAN },
#endif  // !defined(OS_MACOSX) && !defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  { key::kDataCompressionProxyEnabled,
    prefs::kSpdyProxyAuthEnabled,
    base::Value::TYPE_BOOLEAN },
#endif  // defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  { key::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    base::Value::TYPE_BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
};

#if !defined(OS_IOS)
// Mapping from extension type names to Manifest::Type.
#if defined(ENABLE_EXTENSIONS_ALL)
StringToIntEnumListPolicyHandler::MappingEntry kExtensionAllowedTypesMap[] = {
  { "extension", extensions::Manifest::TYPE_EXTENSION },
  { "theme", extensions::Manifest::TYPE_THEME },
  { "user_script", extensions::Manifest::TYPE_USER_SCRIPT },
  { "hosted_app", extensions::Manifest::TYPE_HOSTED_APP },
  { "legacy_packaged_app", extensions::Manifest::TYPE_LEGACY_PACKAGED_APP },
  { "platform_app", extensions::Manifest::TYPE_PLATFORM_APP },
};
#endif
#endif  // !defined(OS_IOS)

}  // namespace

scoped_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  scoped_ptr<ConfigurationPolicyHandlerList> handlers(
      new ConfigurationPolicyHandlerList(base::Bind(&GetChromePolicyDetails)));
  for (size_t i = 0; i < arraysize(kSimplePolicyMap); ++i) {
    handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
        new SimplePolicyHandler(kSimplePolicyMap[i].policy_name,
                                kSimplePolicyMap[i].preference_path,
                                kSimplePolicyMap[i].value_type)));
  }

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new AutofillPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new DefaultSearchPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IncognitoModePolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ProxyPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new URLBlacklistPolicyHandler()));

#if !defined(OS_IOS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new FileSelectionDialogsPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new JavascriptPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new RestoreOnStartupPolicyHandler()));
#if defined(ENABLE_SYNC)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new browser_sync::SyncPolicyHandler()));
#endif
#if defined(ENABLE_EXTENSIONS_ALL)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionListPolicyHandler(
          key::kExtensionInstallWhitelist,
          extensions::pref_names::kInstallAllowList,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionListPolicyHandler(
          key::kExtensionInstallBlacklist,
          extensions::pref_names::kInstallDenyList,
          true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionInstallForcelistPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionURLPatternListPolicyHandler(
          key::kExtensionInstallSources,
          extensions::pref_names::kAllowedInstallSites)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new StringToIntEnumListPolicyHandler(
          key::kExtensionAllowedTypes,
          extensions::pref_names::kAllowedTypes,
          kExtensionAllowedTypesMap,
          kExtensionAllowedTypesMap + arraysize(kExtensionAllowedTypesMap))));
#endif
#endif  // !defined(OS_IOS)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new DiskCacheDirPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new DownloadDirPolicyHandler));

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::NativeMessagingHostListPolicyHandler(
          key::kNativeMessagingWhitelist,
          extensions::pref_names::kNativeMessagingWhitelist,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::NativeMessagingHostListPolicyHandler(
          key::kNativeMessagingBlacklist,
          extensions::pref_names::kNativeMessagingBlacklist,
          true)));
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(OS_CHROMEOS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionListPolicyHandler(
          key::kAttestationExtensionWhitelist,
          prefs::kAttestationExtensionWhitelist,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      NetworkConfigurationPolicyHandler::CreateForDevicePolicy()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new PinnedLauncherAppsPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ScreenMagnifierPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new LoginScreenPowerManagementPolicyHandler));

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kScreenDimDelayAC,
                                prefs::kPowerAcScreenDimDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kScreenOffDelayAC,
                                prefs::kPowerAcScreenOffDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kScreenLockDelayAC,
                                prefs::kPowerAcScreenLockDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kIdleWarningDelayAC,
                                prefs::kPowerAcIdleWarningDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kIdleDelayAC, prefs::kPowerAcIdleDelayMs, 0, INT_MAX, true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kScreenDimDelayBattery,
                                prefs::kPowerBatteryScreenDimDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kScreenOffDelayBattery,
                                prefs::kPowerBatteryScreenOffDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kScreenLockDelayBattery,
                                prefs::kPowerBatteryScreenLockDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kIdleWarningDelayBattery,
                                prefs::kPowerBatteryIdleWarningDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kIdleDelayBattery,
                                prefs::kPowerBatteryIdleDelayMs,
                                0,
                                INT_MAX,
                                true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kSAMLOfflineSigninTimeLimit,
                                prefs::kSAMLOfflineSigninTimeLimit,
                                -1,
                                INT_MAX,
                                true)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kIdleActionAC,
          prefs::kPowerAcIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kIdleActionBattery,
          prefs::kPowerBatteryIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new DeprecatedIdleActionHandler()));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kLidCloseAction,
          prefs::kPowerLidClosedAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntPercentageToDoublePolicyHandler(
          key::kPresentationScreenDimDelayScale,
          prefs::kPowerPresentationScreenDimDelayFactor,
          100,
          INT_MAX,
          true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntPercentageToDoublePolicyHandler(
          key::kUserActivityScreenDimDelayScale,
          prefs::kPowerUserActivityScreenDimDelayFactor,
          100,
          INT_MAX,
          true)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kUptimeLimit, prefs::kUptimeLimit, 3600, INT_MAX, true)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kDeviceLoginScreenDefaultScreenMagnifierType,
          NULL,
          0,
          ash::MAGNIFIER_FULL,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ExternalDataPolicyHandler(key::kUserAvatarImage)));
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ManagedBookmarksPolicyHandler()));
#endif
  return handlers.Pass();
}

}  // namespace policy
