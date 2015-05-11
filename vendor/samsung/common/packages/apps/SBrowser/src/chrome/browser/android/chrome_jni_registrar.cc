// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/android/banners/app_banner_manager.h"
#include "chrome/browser/android/bookmarks/bookmarks_bridge.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/android/chromium_application.h"
#include "chrome/browser/android/content_view_util.h"
#include "chrome/browser/android/dev_tools_server.h"
#include "chrome/browser/android/favicon_helper.h"
#include "chrome/browser/android/field_trial_helper.h"
#if defined(ENABLE_SYNC)
#include "chrome/browser/android/foreign_session_helper.h"
#endif
#include "chrome/browser/android/intent_helper.h"
#if defined(ENABLE_MOSTVISITED)
#include "chrome/browser/android/most_visited_sites.h"
#endif
#include "chrome/browser/android/new_tab_page_prefs.h"
#if defined(ENABLE_PRERENDER)
#include "chrome/browser/android/omnibox/omnibox_prerender.h"
#endif
#include "chrome/browser/android/password_authentication_manager.h"
#include "chrome/browser/android/password_ui_view_android.h"
#include "chrome/browser/android/profiles/profile_downloader_android.h"
#include "chrome/browser/android/provider/chrome_browser_provider.h"
#include "chrome/browser/android/recently_closed_tabs_bridge.h"
#include "chrome/browser/android/shortcut_helper.h"
#if defined(ENABLE_SIGNIN)
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#include "chrome/browser/android/signin/signin_manager_android.h"
#endif
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/uma_bridge.h"
#include "chrome/browser/android/uma_utils.h"
#include "chrome/browser/android/url_utilities.h"
#include "chrome/browser/android/voice_search_tab_helper.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/history/android/sqlite_cursor.h"
#include "chrome/browser/invalidation/invalidation_controller_android.h"
#include "chrome/browser/lifetime/application_lifetime_android.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/prerender/external_prerender_handler_android.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/template_url_service_android.h"
#if defined(ENABLE_SIGNIN)
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#endif
#include "chrome/browser/speech/tts_android.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_controller_android.h"
#include "chrome/browser/ui/android/autofill/autofill_dialog_result.h"
#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#if defined(ENABLE_SIGNIN)
#include "chrome/browser/ui/android/infobars/auto_login_infobar_delegate_android.h"
#endif
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/infobars/save_password_infobar.h"
#if defined(ENABLE_TRANSLATE)
#include "chrome/browser/ui/android/infobars/translate_infobar.h"
#endif
#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"
#include "chrome/browser/ui/android/navigation_popup.h"
#include "chrome/browser/ui/android/ssl_client_certificate_request.h"
#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"
#include "chrome/browser/ui/android/website_settings_popup_android.h"
#include "components/autofill/core/browser/android/component_jni_registrar.h"
#include "components/navigation_interception/component_jni_registrar.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"

#if defined(ENABLE_PRINTING) && !defined(ENABLE_FULL_PRINTING)
#include "printing/printing_context_android.h"
#endif

bool RegisterCertificateViewer(JNIEnv* env);

namespace chrome {
namespace android {

static base::android::RegistrationMethod kChromeRegisteredMethods[] = {
  // Register JNI for components we depend on.
  { "NavigationInterception",
    navigation_interception::RegisterNavigationInterceptionJni },
  { "WebContentsDelegateAndroid",
    web_contents_delegate_android::RegisterWebContentsDelegateAndroidJni },
  { "RegisterAuxiliaryProfileLoader", autofill::RegisterAutofillAndroidJni },
  // Register JNI for chrome classes.
#if defined(ENABLE_SIGNIN)
  { "AccountManagementScreenHelper", AccountManagementScreenHelper::Register },
  { "AndroidProfileOAuth2TokenService",
    AndroidProfileOAuth2TokenService::Register },
#endif
  { "AppBannerManager", RegisterAppBannerManager },
  { "ApplicationLifetime", RegisterApplicationLifetimeAndroid },
#if defined(ENABLE_AUTOFILL)
  { "AutofillDialogControllerAndroid",
    autofill::AutofillDialogControllerAndroid::
        RegisterAutofillDialogControllerAndroid },
  { "AutofillDialogResult",
    autofill::AutofillDialogResult::RegisterAutofillDialogResult },
  { "AutofillPopup",
    autofill::AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid },
#endif
#if defined(ENABLE_SIGNIN)
  { "AutoLoginDelegate", AutoLoginInfoBarDelegateAndroid::Register },
#endif
  { "BookmarksBridge", BookmarksBridge::RegisterBookmarksBridge },
  { "CertificateViewer", RegisterCertificateViewer },
  { "ChromeBrowserProvider",
    ChromeBrowserProvider::RegisterChromeBrowserProvider },
  { "ChromeHttpAuthHandler",
    ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler },
  { "ChromeWebContentsDelegateAndroid",
    RegisterChromeWebContentsDelegateAndroid },
  { "ChromiumApplication",
    ChromiumApplication::RegisterBindings },
  { "ConfirmInfoBarDelegate", RegisterConfirmInfoBarDelegate },
  { "ContentViewUtil", RegisterContentViewUtil },
  { "ContextMenuHelper", RegisterContextMenuHelper },
  { "DataReductionProxySettings", DataReductionProxySettingsAndroid::Register },
#if defined(ENABLE_DEV_TOOLS)
  { "DevToolsServer", RegisterDevToolsServer },
#endif
  { "InvalidationController", invalidation::RegisterInvalidationController },
#if defined(ENABLE_PRERENDER)
  { "ExternalPrerenderRequestHandler",
      prerender::ExternalPrerenderHandlerAndroid::
      RegisterExternalPrerenderHandlerAndroid },
#endif
  { "FaviconHelper", FaviconHelper::RegisterFaviconHelper },
  { "FieldTrialHelper", RegisterFieldTrialHelper },
#if defined(ENABLE_SYNC)
  { "ForeignSessionHelper",
    ForeignSessionHelper::RegisterForeignSessionHelper },
#endif
  { "InfoBarContainer", RegisterInfoBarContainer },
  { "ShortcutHelper", ShortcutHelper::RegisterShortcutHelper },
  { "IntentHelper", RegisterIntentHelper },
  { "JavascriptAppModalDialog",
    JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog },
#if defined(ENABLE_MOSTVISITED)
  { "MostVisitedSites", MostVisitedSites::Register },
#endif
  { "NativeInfoBar", RegisterNativeInfoBar },
  { "NavigationPopup", NavigationPopup::RegisterNavigationPopup },
  { "NewTabPagePrefs",
    NewTabPagePrefs::RegisterNewTabPagePrefs },
#if defined(ENABLE_PRERENDER)
  { "OmniboxPrerender", RegisterOmniboxPrerender },
#endif
  { "PasswordAuthenticationManager",
    PasswordAuthenticationManager::RegisterPasswordAuthenticationManager },
  { "PasswordUIViewAndroid",
    PasswordUIViewAndroid::RegisterPasswordUIViewAndroid },
#if defined(ENABLE_AUTOFILL)
  { "PersonalDataManagerAndroid",
    autofill::PersonalDataManagerAndroid::Register },
#endif
  { "ProfileAndroid", ProfileAndroid::RegisterProfileAndroid },
  { "ProfileDownloaderAndroid", ProfileDownloaderAndroid::Register },
#if defined(ENABLE_SYNC)
  { "ProfileSyncService", ProfileSyncServiceAndroid::Register },
#endif
  { "RecentlyClosedBridge", RecentlyClosedTabsBridge::Register },
  { "SavePasswordInfoBar", RegisterSavePasswordInfoBar },
#if defined(ENABLE_SIGNIN)
  { "SigninManager", SigninManagerAndroid::Register },
#endif
  { "SqliteCursor", SQLiteCursor::RegisterSqliteCursor },
  { "SSLClientCertificateRequest", RegisterSSLClientCertificateRequestAndroid },
  { "StartupMetricUtils", RegisterStartupMetricUtils },
  { "TabAndroid", TabAndroid::RegisterTabAndroid },
  { "TemplateUrlServiceAndroid", TemplateUrlServiceAndroid::Register },
  { "ToolbarModelAndroid", ToolbarModelAndroid::RegisterToolbarModelAndroid },
#if defined(ENABLE_TRANSLATE)
  { "TranslateInfoBarDelegate", RegisterTranslateInfoBarDelegate },
#endif  
  { "TtsPlatformImpl", TtsPlatformImplAndroid::Register },
  { "UmaBridge", RegisterUmaBridge },
  { "UrlUtilities", RegisterUrlUtilities },
  { "VoiceSearchTabHelper", RegisterVoiceSearchTabHelper },
  { "WebsiteSettingsPopupAndroid",
    WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid },
#if defined(ENABLE_PRINTING) && !defined(ENABLE_FULL_PRINTING)
  { "PrintingContext",
      printing::PrintingContextAndroid::RegisterPrintingContext},
#endif
};

bool RegisterJni(JNIEnv* env) {
  TRACE_EVENT0("startup", "chrome_android::RegisterJni");
  return RegisterNativeMethods(env, kChromeRegisteredMethods,
                               arraysize(kChromeRegisteredMethods));
}

}  // namespace android
}  // namespace chrome
