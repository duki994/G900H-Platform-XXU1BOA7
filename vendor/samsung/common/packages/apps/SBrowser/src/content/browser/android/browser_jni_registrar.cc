// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/child_process_launcher_android.h"
#include "content/browser/android/content_settings.h"
#include "content/browser/android/content_video_view.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/content_view_render_view.h"
#include "content/browser/android/content_view_statics.h"
#include "content/browser/android/date_time_chooser_android.h"
#include "content/browser/android/download_controller_android_impl.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/surface_texture_peer_browser_impl.h"
#include "content/browser/android/tracing_controller_android.h"
#include "content/browser/android/web_contents_observer_android.h"
#include "content/browser/device_orientation/sensor_manager_android.h"
#include "content/browser/frame_host/navigation_controller_android.h"
#include "content/browser/gamepad/gamepad_platform_data_fetcher_android.h"
#include "content/browser/geolocation/location_api_adapter_android.h"
#include "content/browser/media/android/media_drm_credential_manager.h"
#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/browser/power_save_blocker_android.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/input/motion_event_android.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/java/java_bound_object.h"
#include "content/browser/screen_orientation/screen_orientation_provider_android.h"
#include "content/browser/speech/speech_recognizer_impl_android.h"
#include "content/browser/vibration/vibration_provider_android.h"
#include "content/browser/web_contents/web_contents_android.h"
#if defined(S_NATIVE_SUPPORT) && !defined(S_UNITTEST_SUPPORT)
#include "sbrowser/content/native/browser/android/sbr/sbr_android_protocol_handler.h"
#include "sbrowser/content/native/browser/android/sbr/sbr_content_video_view.h"
#include "sbrowser/content/native/browser/android/sbr/sbr_content_view_core_impl.h"
#include "sbrowser/content/native/browser/android/sbr/sbr_content_settings.h"
#include "sbrowser/content/native/browser/android/sbr/sbr_input_stream_impl.h"
#if defined(ENABLE_PUSH_API)
#include "sbrowser/content/native/browser/android/sbr/push_provider_android.h"
#endif
#endif

using content::SurfaceTexturePeerBrowserImpl;

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
    {"AndroidLocationApiAdapter",
     content::AndroidLocationApiAdapter::RegisterGeolocationService},
    {"BrowserAccessibilityManager",
     content::RegisterBrowserAccessibilityManager},
    {"BrowserStartupController", content::RegisterBrowserStartupController},
    {"ChildProcessLauncher", content::RegisterChildProcessLauncher},
    {"ContentSettings", content::ContentSettings::RegisterContentSettings},
    {"ContentViewRenderView",
     content::ContentViewRenderView::RegisterContentViewRenderView},
    {"ContentVideoView", content::ContentVideoView::RegisterContentVideoView},
    {"ContentViewCore", content::RegisterContentViewCore},
    {"DateTimePickerAndroid", content::RegisterDateTimeChooserAndroid},
    {"DownloadControllerAndroidImpl",
     content::DownloadControllerAndroidImpl::RegisterDownloadController},
    {"GamepadAdapter",
     content::GamepadPlatformDataFetcherAndroid::RegisterGamepadAdapter},
    {"InterstitialPageDelegateAndroid",
     content::InterstitialPageDelegateAndroid::
         RegisterInterstitialPageDelegateAndroid},
    {"LoadUrlParams", content::RegisterLoadUrlParams},
    {"MediaDrmCredentialManager",
     content::MediaDrmCredentialManager::RegisterMediaDrmCredentialManager},
    {"MediaResourceGetterImpl",
     content::MediaResourceGetterImpl::RegisterMediaResourceGetter},
    {"MotionEventAndroid",
     content::MotionEventAndroid::RegisterMotionEventAndroid},
    {"NavigationControllerAndroid",
     content::NavigationControllerAndroid::Register},
    {"PowerSaveBlock", content::RegisterPowerSaveBlocker},
    {"RegisterImeAdapter", content::RegisterImeAdapter},
    {"ScreenOrientationProvider",
     content::ScreenOrientationProviderAndroid::Register},
    {"SensorManagerAndroid", content::SensorManagerAndroid::Register},
    {"SpeechRecognizerImplAndroid",
     content::SpeechRecognizerImplAndroid::RegisterSpeechRecognizer},
    {"TouchEventSynthesizer",
     content::SyntheticGestureTargetAndroid::RegisterTouchEventSynthesizer},
    {"TracingControllerAndroid", content::RegisterTracingControllerAndroid},
    {"VibrationProvider", content::VibrationProviderAndroid::Register},
    {"WebContentsAndroid", content::WebContentsAndroid::Register},
    {"WebContentsObserverAndroid", content::RegisterWebContentsObserverAndroid},
    {"WebViewStatics", content::RegisterWebViewStatics},
#if defined(S_NATIVE_SUPPORT) && !defined(S_UNITTEST_SUPPORT)
    {"AndroidProtocolHandler", content::RegisterAndroidProtocolHandler},
    {"InputStream", content::RegisterInputStream},
    { "SbrContentViewCore", content::RegisterSbrContentViewCore},
    { "SbrContentSettings", content::RegisterSbrContentSettings},
    { "SbrContentVideoView", content::SbrContentVideoView::RegisterSbrContentVideoView},
#if defined(ENABLE_PUSH_API)
    {"PushProvider", content::PushProviderAndroid::Register},
#endif
#endif
    };
}  // namespace

namespace content {
namespace android {

bool RegisterBrowserJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
