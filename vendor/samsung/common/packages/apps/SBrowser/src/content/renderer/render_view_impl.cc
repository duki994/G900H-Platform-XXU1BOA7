// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <time.h>
#include <algorithm>
#include <cmath>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/trace_event.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "content/child/child_thread.h"
#include "content/child/npapi/webplugin_delegate_impl.h"
#include "content/child/request_extra_data.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/clipboard_messages.h"
#include "content/common/database_messages.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/input_messages.h"
#include "content/common/java_bridge_messages.h"
#include "content/common/pepper_messages.h"
#include "content/common/socket_stream_handle_data.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/history_item_serialization.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/web_preferences.h"
#include "content/renderer/accessibility/renderer_accessibility.h"
#include "content/renderer/accessibility/renderer_accessibility_complete.h"
#include "content/renderer/accessibility/renderer_accessibility_focus_only.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/disambiguation_popup_helper.h"
#include "content/renderer/dom_automation_controller.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/geolocation_dispatcher.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/idle_user_detector.h"
#include "content/renderer/image_loading_helper.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/input_tag_speech_dispatcher.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/java/java_bridge_dispatcher.h"
#include "content/renderer/load_progress_tracker.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/midi_dispatcher.h"
#include "content/renderer/media/render_media_log.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webmediaplayer_impl.h"
#include "content/renderer/media/webmediaplayer_ms.h"
#include "content/renderer/media/webmediaplayer_params.h"
#include "content/renderer/memory_benchmarking_extension.h"
#include "content/renderer/mhtml_generator.h"
#include "content/renderer/notification_provider.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl_params.h"
#include "content/renderer/render_view_mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/renderer_webcolorchooser_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "content/renderer/speech_recognition_dispatcher.h"
#include "content/renderer/stats_collection_controller.h"
#include "content/renderer/stats_collection_observer.h"
#include "content/renderer/text_input_client_observer.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/web_ui_extension.h"
#include "content/renderer/web_ui_extension_data.h"
#include "content/renderer/websharedworker_proxy.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/filter_collection.h"
#include "media/base/media_switches.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebSocketStreamHandle.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebColorName.h"
#include "third_party/WebKit/public/web/WebColorSuggestion.h"
#include "third_party/WebKit/public/web/WebDOMEvent.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFileChooserParams.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebGlyphCache.h"
#include "third_party/WebKit/public/web/WebHelperPlugin.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebPageSerializer.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginDocument.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSearchableFormData.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "third_party/WebKit/public/web/default/WebRenderTheme.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "v8/include/v8.h"
#include "webkit/child/weburlresponse_extradata_impl.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "content/common/android/device_telephony_info.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/renderer/android/address_detector.h"
#include "content/renderer/android/content_detector.h"
#include "content/renderer/android/email_detector.h"

#if !defined (DISABLE_LIBPHONENUMBER)
#include "content/renderer/android/phone_number_detector.h"
#endif

#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/stream_texture_factory_android_impl.h"
#include "content/renderer/media/android/webmediaplayer_android.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "ui/gfx/rect_f.h"

#elif defined(OS_WIN)
// TODO(port): these files are currently Windows only because they concern:
//   * theming
#include "ui/native_theme/native_theme_win.h"
#elif defined(USE_X11)
#include "ui/native_theme/native_theme.h"
#elif defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/renderer/npapi/webplugin_delegate_proxy.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/rtc_peer_connection_handler.h"
#endif

#if defined(ENABLE_PUSH_API)
#include "content/renderer/push_registration/push_registration_dispatcher.h"
#endif

#include "skia/ext/bitmap_platform_device_skia.h"

#if (defined(ENABLE_FIT_TO_MAJOR)||defined(S_FIT_TO_SCREEN)) && defined(OS_ANDROID)
#include "ui/gfx/android/device_display_info.h"
#endif

using blink::WebAXObject;
using blink::WebApplicationCacheHost;
using blink::WebApplicationCacheHostClient;
using blink::WebCString;
using blink::WebColor;
using blink::WebColorName;
using blink::WebConsoleMessage;
using blink::WebData;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebDragData;
using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebElement;
using blink::WebExternalPopupMenu;
using blink::WebExternalPopupMenuClient;
using blink::WebFileChooserCompletion;
using blink::WebFindOptions;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebGestureEvent;
using blink::WebHistoryItem;
using blink::WebHTTPBody;
using blink::WebIconURL;
using blink::WebImage;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebMediaPlayer;
using blink::WebMediaPlayerAction;
using blink::WebMediaPlayerClient;
using blink::WebMouseEvent;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPageSerializer;
using blink::WebPageSerializerClient;
using blink::WebPeerConnection00Handler;
using blink::WebPeerConnection00HandlerClient;
using blink::WebPeerConnectionHandler;
using blink::WebPeerConnectionHandlerClient;
using blink::WebPluginAction;
using blink::WebPluginContainer;
using blink::WebPluginDocument;
using blink::WebPoint;
using blink::WebPopupMenuInfo;
using blink::WebRange;
using blink::WebRect;
using blink::WebReferrerPolicy;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSerializedScriptValue;
using blink::WebSettings;
using blink::WebSize;
using blink::WebSocketStreamHandle;
using blink::WebStorageNamespace;
using blink::WebStorageQuotaCallbacks;
using blink::WebStorageQuotaError;
using blink::WebStorageQuotaType;
using blink::WebString;
using blink::WebTextAffinity;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebUserGestureIndicator;
using blink::WebVector;
using blink::WebView;
using blink::WebWidget;
using blink::WebWindowFeatures;
using base::Time;
using base::TimeDelta;
using webkit_glue::WebURLResponseExtraDataImpl;

#if defined(OS_ANDROID)
using blink::WebContentDetectionResult;
using blink::WebFloatPoint;
using blink::WebFloatRect;
using blink::WebHitTestResult;
#endif

namespace content {

//-----------------------------------------------------------------------------

typedef std::map<blink::WebView*, RenderViewImpl*> ViewMap;
static base::LazyInstance<ViewMap> g_view_map = LAZY_INSTANCE_INITIALIZER;
typedef std::map<int32, RenderViewImpl*> RoutingIDViewMap;
static base::LazyInstance<RoutingIDViewMap> g_routing_id_view_map =
    LAZY_INSTANCE_INITIALIZER;

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
const int kDelaySecondsForContentStateSyncHidden = 5;
const int kDelaySecondsForContentStateSync = 1;

const size_t kExtraCharsBeforeAndAfterSelection = 100;

const float kScalingIncrementForGesture = 0.01f;

// Samsung: Reader debugging flag used for performance checks. Once the
// performance is verfied, this flag should be removed as shipping the code with
// debug flag is highly deprecated.
const bool kSamsungReaderDebugFlag = true;

#if defined(OS_ANDROID)
// Delay between tapping in content and launching the associated android intent.
// Used to allow users see what has been recognized as content.
const size_t kContentIntentDelayMilliseconds = 700;
#endif

static RenderViewImpl* (*g_create_render_view_impl)(RenderViewImplParams*) =
    NULL;

// static
bool RenderViewImpl::IsReload(const FrameMsg_Navigate_Params& params) {
  return
      params.navigation_type == FrameMsg_Navigate_Type::RELOAD ||
      params.navigation_type == FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE ||
      params.navigation_type ==
          FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
}

// static
Referrer RenderViewImpl::GetReferrerFromRequest(
    WebFrame* frame,
    const WebURLRequest& request) {
  return Referrer(GURL(request.httpHeaderField(WebString::fromUTF8("Referer"))),
                  request.referrerPolicy());
}

// Returns false unless this is a top-level navigation.
static bool IsTopLevelNavigation(WebFrame* frame) {
  return frame->parent() == NULL;
}

// Returns false unless this is a top-level navigation that crosses origins.
static bool IsNonLocalTopLevelNavigation(const GURL& url,
                                         WebFrame* frame,
                                         WebNavigationType type,
                                         bool is_form_post) {
  if (!IsTopLevelNavigation(frame))
    return false;

  // Navigations initiated within Webkit are not sent out to the external host
  // in the following cases.
  // 1. The url scheme is not http/https
  // 2. The origin of the url and the opener is the same in which case the
  //    opener relationship is maintained.
  // 3. Reloads/form submits/back forward navigations
  if (!url.SchemeIs(kHttpScheme) && !url.SchemeIs(kHttpsScheme))
    return false;

  if (type != blink::WebNavigationTypeReload &&
      type != blink::WebNavigationTypeBackForward && !is_form_post) {
    // The opener relationship between the new window and the parent allows the
    // new window to script the parent and vice versa. This is not allowed if
    // the origins of the two domains are different. This can be treated as a
    // top level navigation and routed back to the host.
    blink::WebFrame* opener = frame->opener();
    if (!opener)
      return true;

    if (url.GetOrigin() != GURL(opener->document().url()).GetOrigin())
      return true;
  }
  return false;
}

// static
void RenderViewImpl::NotifyTimezoneChange(blink::WebFrame* frame) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Context::Scope context_scope(frame->mainWorldScriptContext());
  v8::Date::DateTimeConfigurationChangeNotification(v8::Isolate::GetCurrent());
  blink::WebFrame* child = frame->firstChild();
  for (; child; child = child->nextSibling())
    NotifyTimezoneChange(child);
}

static WindowOpenDisposition NavigationPolicyToDisposition(
    WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyIgnore:
      return IGNORE_ACTION;
    case blink::WebNavigationPolicyDownload:
      return SAVE_TO_DISK;
    case blink::WebNavigationPolicyCurrentTab:
      return CURRENT_TAB;
    case blink::WebNavigationPolicyNewBackgroundTab:
      return NEW_BACKGROUND_TAB;
    case blink::WebNavigationPolicyNewForegroundTab:
      return NEW_FOREGROUND_TAB;
    case blink::WebNavigationPolicyNewWindow:
      return NEW_WINDOW;
    case blink::WebNavigationPolicyNewPopup:
      return NEW_POPUP;
  default:
    NOTREACHED() << "Unexpected WebNavigationPolicy";
    return IGNORE_ACTION;
  }
}

// Returns true if the device scale is high enough that losing subpixel
// antialiasing won't have a noticeable effect on text quality.
static bool DeviceScaleEnsuresTextQuality(float device_scale_factor) {
#if defined(OS_ANDROID)
  // On Android, we never have subpixel antialiasing.
  return true;
#else
  return device_scale_factor > 1.5f;
#endif

}

static bool ShouldUseFixedPositionCompositing(float device_scale_factor) {
  // Compositing for fixed-position elements is dependent on
  // device_scale_factor if no flag is set. http://crbug.com/172738
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableCompositingForFixedPosition))
    return false;

  if (command_line.HasSwitch(switches::kEnableCompositingForFixedPosition))
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseAcceleratedCompositingForOverflowScroll(
    float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableAcceleratedOverflowScroll))
    return false;

  if (command_line.HasSwitch(switches::kEnableAcceleratedOverflowScroll))
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseAcceleratedCompositingForScrollableFrames(
    float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableAcceleratedScrollableFrames))
    return false;

  if (command_line.HasSwitch(switches::kEnableAcceleratedScrollableFrames))
    return true;

  if (!cc::switches::IsLCDTextEnabled())
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseCompositedScrollingForFrames(
    float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableCompositedScrollingForFrames))
    return false;

  if (command_line.HasSwitch(switches::kEnableCompositedScrollingForFrames))
    return true;

  if (!cc::switches::IsLCDTextEnabled())
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseUniversalAcceleratedCompositingForOverflowScroll() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(
          switches::kDisableUniversalAcceleratedOverflowScroll))
    return false;

  if (command_line.HasSwitch(
          switches::kEnableUniversalAcceleratedOverflowScroll))
    return true;

  return false;
}

static bool ShouldUseTransitionCompositing(float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableCompositingForTransition))
    return false;

  if (command_line.HasSwitch(switches::kEnableCompositingForTransition))
    return true;

  // TODO(ajuma): Re-enable this by default for high-DPI once the problem
  // of excessive layer promotion caused by overlap has been addressed.
  // http://crbug.com/178119.
  return false;
}

static bool ShouldUseAcceleratedFixedRootBackground(float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableAcceleratedFixedRootBackground))
    return false;

  if (command_line.HasSwitch(switches::kEnableAcceleratedFixedRootBackground))
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static FaviconURL::IconType ToFaviconType(blink::WebIconURL::Type type) {
  switch (type) {
    case blink::WebIconURL::TypeFavicon:
      return FaviconURL::FAVICON;
    case blink::WebIconURL::TypeTouch:
      return FaviconURL::TOUCH_ICON;
    case blink::WebIconURL::TypeTouchPrecomposed:
      return FaviconURL::TOUCH_PRECOMPOSED_ICON;
    case blink::WebIconURL::TypeInvalid:
      return FaviconURL::INVALID_ICON;
  }
  return FaviconURL::INVALID_ICON;
}

///////////////////////////////////////////////////////////////////////////////

struct RenderViewImpl::PendingFileChooser {
  PendingFileChooser(const FileChooserParams& p, WebFileChooserCompletion* c)
      : params(p),
        completion(c) {
  }
  FileChooserParams params;
  WebFileChooserCompletion* completion;  // MAY BE NULL to skip callback.
};

namespace {

class WebWidgetLockTarget : public MouseLockDispatcher::LockTarget {
 public:
  explicit WebWidgetLockTarget(blink::WebWidget* webwidget)
      : webwidget_(webwidget) {}

  virtual void OnLockMouseACK(bool succeeded) OVERRIDE {
    if (succeeded)
      webwidget_->didAcquirePointerLock();
    else
      webwidget_->didNotAcquirePointerLock();
  }

  virtual void OnMouseLockLost() OVERRIDE {
    webwidget_->didLosePointerLock();
  }

  virtual bool HandleMouseLockedInputEvent(
      const blink::WebMouseEvent &event) OVERRIDE {
    // The WebWidget handles mouse lock in WebKit's handleInputEvent().
    return false;
  }

 private:
  blink::WebWidget* webwidget_;
};

bool TouchEnabled() {
// Based on the definition of chrome::kEnableTouchIcon.
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

WebDragData DropDataToWebDragData(const DropData& drop_data) {
  std::vector<WebDragData::Item> item_list;

  // These fields are currently unused when dragging into WebKit.
  DCHECK(drop_data.download_metadata.empty());
  DCHECK(drop_data.file_contents.empty());
  DCHECK(drop_data.file_description_filename.empty());

  if (!drop_data.text.is_null()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeText);
    item.stringData = drop_data.text.string();
    item_list.push_back(item);
  }

  // TODO(dcheng): Do we need to distinguish between null and empty URLs? Is it
  // meaningful to write an empty URL to the clipboard?
  if (!drop_data.url.is_empty()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeURIList);
    item.stringData = WebString::fromUTF8(drop_data.url.spec());
    item.title = drop_data.url_title;
    item_list.push_back(item);
  }

  if (!drop_data.html.is_null()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeHTML);
    item.stringData = drop_data.html.string();
    item.baseURL = drop_data.html_base_url;
    item_list.push_back(item);
  }

  for (std::vector<DropData::FileInfo>::const_iterator it =
           drop_data.filenames.begin();
       it != drop_data.filenames.end();
       ++it) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeFilename;
    item.filenameData = it->path;
    item.displayNameData = it->display_name;
    item_list.push_back(item);
  }

  for (std::map<base::string16, base::string16>::const_iterator it =
           drop_data.custom_data.begin();
       it != drop_data.custom_data.end();
       ++it) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = it->first;
    item.stringData = it->second;
    item_list.push_back(item);
  }

  WebDragData result;
  result.initialize();
  result.setItems(item_list);
  result.setFilesystemId(drop_data.filesystem_id);
  return result;
}

}  // namespace

RenderViewImpl::RenderViewImpl(RenderViewImplParams* params)
    : RenderWidget(blink::WebPopupTypeNone,
                   params->screen_info,
                   params->swapped_out,
                   params->hidden),
      webkit_preferences_(params->webkit_prefs),
      send_content_state_immediately_(false),
      enabled_bindings_(0),
      send_preferred_size_changes_(false),
      is_loading_(false),
      navigation_gesture_(NavigationGestureUnknown),
      opened_by_user_gesture_(true),
      opener_suppressed_(false),
      suppress_dialogs_until_swap_out_(false),
      is_draw_suspended_(false),
      page_id_(-1),
      last_page_id_sent_to_browser_(-1),
      next_page_id_(params->next_page_id),
      history_list_offset_(-1),
      history_list_length_(0),
      target_url_status_(TARGET_NONE),
      selection_text_offset_(0),
      selection_range_(gfx::Range::InvalidRange()),
#if defined(OS_ANDROID)
      top_controls_constraints_(cc::BOTH),
#endif
      cached_is_main_frame_pinned_to_left_(false),
      cached_is_main_frame_pinned_to_right_(false),
      cached_has_main_frame_horizontal_scrollbar_(false),
      cached_has_main_frame_vertical_scrollbar_(false),
      notification_provider_(NULL),
#if defined(ENABLE_PUSH_API)
      push_registration_dispatcher_(NULL),
#endif
      geolocation_dispatcher_(NULL),
      input_tag_speech_dispatcher_(NULL),
      speech_recognition_dispatcher_(NULL),
      media_stream_dispatcher_(NULL),
      browser_plugin_manager_(NULL),
      media_stream_client_(NULL),
      web_user_media_client_(NULL),
      midi_dispatcher_(NULL),
      devtools_agent_(NULL),
      accessibility_mode_(AccessibilityModeOff),
      renderer_accessibility_(NULL),
      mouse_lock_dispatcher_(NULL),
#if defined(OS_ANDROID)
      body_background_color_(SK_ColorWHITE),
      expected_content_intent_id_(0),
      media_player_manager_(NULL),
#endif
#if defined(OS_WIN)
      focused_plugin_id_(-1),
#endif
#if defined(ENABLE_PLUGINS)
      focused_pepper_plugin_(NULL),
      pepper_last_mouse_event_target_(NULL),
#endif
      enumeration_completion_id_(0),
      load_progress_tracker_(new LoadProgressTracker(this)),
      session_storage_namespace_id_(params->session_storage_namespace_id),
      handling_select_range_(false),
      next_snapshot_id_(0),
      allow_partial_swap_(params->allow_partial_swap),
      advanced_ime_options_(0)
{
}

void RenderViewImpl::Initialize(RenderViewImplParams* params) {
  routing_id_ = params->routing_id;
  surface_id_ = params->surface_id;
  if (params->opener_id != MSG_ROUTING_NONE && params->is_renderer_created)
    opener_id_ = params->opener_id;

  // Ensure we start with a valid next_page_id_ from the browser.
  DCHECK_GE(next_page_id_, 0);

#if defined(ENABLE_NOTIFICATIONS)
  notification_provider_ = new NotificationProvider(this);
#else
  notification_provider_ = NULL;
#endif

  webwidget_ = WebView::create(this);
  webwidget_mouse_lock_target_.reset(new WebWidgetLockTarget(webwidget_));

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kStatsCollectionController))
    stats_collection_observer_.reset(new StatsCollectionObserver(this));

#if defined(OS_ANDROID)
  content::DeviceTelephonyInfo device_info;

  const std::string region_code =
      command_line.HasSwitch(switches::kNetworkCountryIso)
          ? command_line.GetSwitchValueASCII(switches::kNetworkCountryIso)
          : device_info.GetNetworkCountryIso();
  content_detectors_.push_back(linked_ptr<ContentDetector>(
      new AddressDetector()));
#if !defined (DISABLE_LIBPHONENUMBER)
  content_detectors_.push_back(linked_ptr<ContentDetector>(
      new PhoneNumberDetector(region_code)));
#endif
  content_detectors_.push_back(linked_ptr<ContentDetector>(
      new EmailDetector()));
#endif

#if defined(ENABLE_FIT_TO_MAJOR) || defined(S_FIT_TO_SCREEN)
  const float maxScreenInchToFit = 12;
  const float minScreenInchToFit = 8;
  gfx::DeviceDisplayInfo displayInfo;
#endif

#if defined(ENABLE_FIT_TO_MAJOR)
  // 'fit to major' should be applied if logicalScreenWidth >= 1280px and physicalScreenInch < 12inch
  const int minScreenWidthToFit = 1280;
  int screenWidth =
    std::max(params->screen_info.rect.width, params->screen_info.rect.height);
  if (screenWidth >= minScreenWidthToFit
    && displayInfo.GetPhysicalDisplayInch() < maxScreenInchToFit
    && !command_line.HasSwitch(switches::kDisableFitToMajor))
      webview()->settings()->setFitToMajorScreenWidth(screenWidth);
#endif

#if defined(S_FIT_TO_SCREEN)
  // 'fit to screen' should be applied if physicalScreenInch < 12inch
  // and physicalScreenInch > 8inch
  if (displayInfo.GetPhysicalDisplayInch() < maxScreenInchToFit
    && displayInfo.GetPhysicalDisplayInch() > minScreenInchToFit)
    webview()->settings()->setFitToScreenEnabled(true);
#endif

  RenderThread::Get()->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_ClosePage.
  AddRef();
  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->WidgetCreated();
    if (is_hidden_)
      RenderThreadImpl::current()->WidgetHidden();
  }

  // If this is a popup, we must wait for the CreatingNew_ACK message before
  // completing initialization.  Otherwise, we can finish it now.
  if (opener_id_ == MSG_ROUTING_NONE) {
    did_show_ = true;
    CompleteInit();
  }

  g_view_map.Get().insert(std::make_pair(webview(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(routing_id_, this));
  webview()->setDeviceScaleFactor(device_scale_factor_);
  webview()->settings()->setAcceleratedCompositingForFixedPositionEnabled(
      ShouldUseFixedPositionCompositing(device_scale_factor_));
  webview()->settings()->setAcceleratedCompositingForOverflowScrollEnabled(
      ShouldUseAcceleratedCompositingForOverflowScroll(device_scale_factor_));
  webview()->settings()->setCompositorDrivenAcceleratedScrollingEnabled(
      ShouldUseUniversalAcceleratedCompositingForOverflowScroll());
  webview()->settings()->setAcceleratedCompositingForTransitionEnabled(
      ShouldUseTransitionCompositing(device_scale_factor_));
  webview()->settings()->setAcceleratedCompositingForFixedRootBackgroundEnabled(
      ShouldUseAcceleratedFixedRootBackground(device_scale_factor_));
  webview()->settings()->setAcceleratedCompositingForScrollableFramesEnabled(
      ShouldUseAcceleratedCompositingForScrollableFrames(device_scale_factor_));
  webview()->settings()->setCompositedScrollingForFramesEnabled(
      ShouldUseCompositedScrollingForFrames(device_scale_factor_));

  ApplyWebPreferences(webkit_preferences_, webview());

  main_render_frame_.reset(
      RenderFrameImpl::Create(this, params->main_frame_routing_id));
  // The main frame WebFrame object is closed by
  // RenderFrameImpl::frameDetached().
  webview()->setMainFrame(WebFrame::create(main_render_frame_.get()));
  main_render_frame_->SetWebFrame(webview()->mainFrame());

  if (switches::IsTouchDragDropEnabled())
    webview()->settings()->setTouchDragDropEnabled(true);

  if (switches::IsTouchEditingEnabled())
    webview()->settings()->setTouchEditingEnabled(true);

  if (!params->frame_name.empty())
    webview()->mainFrame()->setName(params->frame_name);

  OnSetRendererPrefs(params->renderer_prefs);

#if defined(ENABLE_WEBRTC)
  if (!media_stream_dispatcher_)
    media_stream_dispatcher_ = new MediaStreamDispatcher(this);
#endif

  new MHTMLGenerator(this);
#if defined(OS_MACOSX)
  new TextInputClientObserver(this);
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
  media_player_manager_ = new RendererMediaPlayerManager(this);
  new JavaBridgeDispatcher(this);
#endif

#ifdef ENABLE_DEV_TOOLS
  // The next group of objects all implement RenderViewObserver, so are deleted
  // along with the RenderView automatically.
  devtools_agent_ = new DevToolsAgent(this);
  if (RenderWidgetCompositor* rwc = compositor()) {
    webview()->devToolsAgent()->setLayerTreeId(rwc->GetLayerTreeId());
  }
#endif
  mouse_lock_dispatcher_ = new RenderViewMouseLockDispatcher(this);
  new ImageLoadingHelper(this);

  // Create renderer_accessibility_ if needed.
  OnSetAccessibilityMode(params->accessibility_mode);

  new IdleUserDetector(this);

  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BINDINGS_POLICY_DOM_AUTOMATION;
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    enabled_bindings_ |= BINDINGS_POLICY_STATS_COLLECTION;

  ProcessViewLayoutFlags(command_line);

  GetContentClient()->renderer()->RenderViewCreated(this);

  // If we have an opener_id but we weren't created by a renderer, then
  // it's the browser asking us to set our opener to another RenderView.
  if (params->opener_id != MSG_ROUTING_NONE && !params->is_renderer_created) {
    RenderViewImpl* opener_view = FromRoutingID(params->opener_id);
    if (opener_view)
      webview()->mainFrame()->setOpener(opener_view->webview()->mainFrame());
  }

  // If we are initially swapped out, navigate to kSwappedOutURL.
  // This ensures we are in a unique origin that others cannot script.
  if (is_swapped_out_)
    NavigateToSwappedOutURL(webview()->mainFrame());

  // Determine page load render draw time
  unsigned int suspend_time_seconds = 0;
  if (command_line.HasSwitch(switches::kPageLoadSuspendDrawTime)) {
    base::StringToUint(command_line.GetSwitchValueNative(
      switches::kPageLoadSuspendDrawTime), &suspend_time_seconds);
  }
  page_load_draw_suspend_time_ = TimeDelta::FromSeconds(suspend_time_seconds);
}

RenderViewImpl::~RenderViewImpl() {
  history_page_ids_.clear();

  base::debug::TraceLog::GetInstance()->RemoveProcessLabel(routing_id_);

  // If file chooser is still waiting for answer, dispatch empty answer.
  while (!file_chooser_completions_.empty()) {
    if (file_chooser_completions_.front()->completion) {
      file_chooser_completions_.front()->completion->didChooseFile(
          WebVector<WebString>());
    }
    file_chooser_completions_.pop_front();
  }

#if defined(OS_ANDROID)
  // The date/time picker client is both a scoped_ptr member of this class and
  // a RenderViewObserver. Reset it to prevent double deletion.
  date_time_picker_client_.reset();
#endif

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap or RoutingIDViewMap.
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
  RoutingIDViewMap* routing_id_views = g_routing_id_view_map.Pointer();
  for (RoutingIDViewMap::iterator it = routing_id_views->begin();
       it != routing_id_views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, RenderViewGone());
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, OnDestruct());
}

/*static*/
RenderViewImpl* RenderViewImpl::FromWebView(WebView* webview) {
  ViewMap* views = g_view_map.Pointer();
  ViewMap::iterator it = views->find(webview);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromWebView(blink::WebView* webview) {
  return RenderViewImpl::FromWebView(webview);
}

/*static*/
RenderViewImpl* RenderViewImpl::FromRoutingID(int32 routing_id) {
  RoutingIDViewMap* views = g_routing_id_view_map.Pointer();
  RoutingIDViewMap::iterator it = views->find(routing_id);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromRoutingID(int routing_id) {
  return RenderViewImpl::FromRoutingID(routing_id);
}

/*static*/
void RenderView::ForEach(RenderViewVisitor* visitor) {
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

/*static*/
RenderViewImpl* RenderViewImpl::Create(
    int32 opener_id,
    const RendererPreferences& renderer_prefs,
    const WebPreferences& webkit_prefs,
    int32 routing_id,
    int32 main_frame_routing_id,
    int32 surface_id,
    int64 session_storage_namespace_id,
    const base::string16& frame_name,
    bool is_renderer_created,
    bool swapped_out,
    bool hidden,
    int32 next_page_id,
    const blink::WebScreenInfo& screen_info,
    unsigned int accessibility_mode,
    bool allow_partial_swap) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  RenderViewImplParams params(
      opener_id,
      renderer_prefs,
      webkit_prefs,
      routing_id,
      main_frame_routing_id,
      surface_id,
      session_storage_namespace_id,
      frame_name,
      is_renderer_created,
      swapped_out,
      hidden,
      next_page_id,
      screen_info,
      accessibility_mode,
      allow_partial_swap);
  RenderViewImpl* render_view = NULL;
  if (g_create_render_view_impl)
    render_view = g_create_render_view_impl(&params);
  else
    render_view = new RenderViewImpl(&params);
  render_view->Initialize(&params);
  return render_view;
}

// static
void RenderViewImpl::InstallCreateHook(
    RenderViewImpl* (*create_render_view_impl)(RenderViewImplParams*)) {
  CHECK(!g_create_render_view_impl);
  g_create_render_view_impl = create_render_view_impl;
}

void RenderViewImpl::AddObserver(RenderViewObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderViewImpl::RemoveObserver(RenderViewObserver* observer) {
  observer->RenderViewGone();
  observers_.RemoveObserver(observer);
}

blink::WebView* RenderViewImpl::webview() const {
  return static_cast<blink::WebView*>(webwidget());
}

#if defined(ENABLE_PLUGINS)
void RenderViewImpl::PepperInstanceCreated(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.insert(instance);
}

void RenderViewImpl::PepperInstanceDeleted(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.erase(instance);

  if (pepper_last_mouse_event_target_ == instance)
    pepper_last_mouse_event_target_ = NULL;
  if (focused_pepper_plugin_ == instance)
    PepperFocusChanged(instance, false);
}

void RenderViewImpl::PepperFocusChanged(PepperPluginInstanceImpl* instance,
                                        bool focused) {
  if (focused)
    focused_pepper_plugin_ = instance;
  else if (focused_pepper_plugin_ == instance)
    focused_pepper_plugin_ = NULL;

  UpdateTextInputType();
  UpdateSelectionBounds();
}

void RenderViewImpl::RegisterPluginDelegate(WebPluginDelegateProxy* delegate) {
  plugin_delegates_.insert(delegate);
  // If the renderer is visible, set initial visibility and focus state.
  if (!is_hidden()) {
#if defined(OS_MACOSX)
    delegate->SetContainerVisibility(true);
    if (webview() && webview()->isActive())
      delegate->SetWindowFocus(true);
#endif
  }
  // Plugins start assuming the content has focus (so that they work in
  // environments where RenderView isn't hosting them), so we always have to
  // set the initial state. See webplugin_delegate_impl.h for details.
  delegate->SetContentAreaFocus(has_focus());
}

void RenderViewImpl::UnregisterPluginDelegate(
    WebPluginDelegateProxy* delegate) {
  plugin_delegates_.erase(delegate);
}

#if defined(OS_WIN)
void RenderViewImpl::PluginFocusChanged(bool focused, int plugin_id) {
  if (focused)
    focused_plugin_id_ = plugin_id;
  else
    focused_plugin_id_ = -1;
}
#endif

#if defined(OS_MACOSX)
void RenderViewImpl::PluginFocusChanged(bool focused, int plugin_id) {
  Send(new ViewHostMsg_PluginFocusChanged(routing_id(), focused, plugin_id));
}

void RenderViewImpl::StartPluginIme() {
  IPC::Message* msg = new ViewHostMsg_StartPluginIme(routing_id());
  // This message can be sent during event-handling, and needs to be delivered
  // within that context.
  msg->set_unblock(true);
  Send(msg);
}
#endif  // defined(OS_MACOSX)

#endif  // ENABLE_PLUGINS

void RenderViewImpl::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  if (webview())
    webview()->transferActiveWheelFlingAnimation(params);
}

bool RenderViewImpl::HasIMETextFocus() {
  return GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
}

bool RenderViewImpl::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (main_frame)
    GetContentClient()->SetActiveURL(main_frame->document().url());

  ObserverListBase<RenderViewObserver>::Iterator it(observers_);
  RenderViewObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    if (observer->OnMessageReceived(message))
      return true;

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderViewImpl, message, msg_is_ok)
    IPC_MESSAGE_HANDLER(InputMsg_Copy, OnCopy)
    IPC_MESSAGE_HANDLER(InputMsg_Cut, OnCut)
    IPC_MESSAGE_HANDLER(InputMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(InputMsg_ExecuteEditCommand, OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(InputMsg_MoveCaret, OnMoveCaret)
    IPC_MESSAGE_HANDLER(InputMsg_Paste, OnPaste)
    IPC_MESSAGE_HANDLER(InputMsg_PasteAndMatchStyle, OnPasteAndMatchStyle)
    IPC_MESSAGE_HANDLER(InputMsg_DirectPaste, OnDirectPaste)
    IPC_MESSAGE_HANDLER(InputMsg_Redo, OnRedo)
    IPC_MESSAGE_HANDLER(InputMsg_Replace, OnReplace)
    IPC_MESSAGE_HANDLER(InputMsg_ReplaceMisspelling, OnReplaceMisspelling)
    IPC_MESSAGE_HANDLER(InputMsg_ScrollFocusedEditableNodeIntoRect,
                        OnScrollFocusedEditableNodeIntoRect)
    IPC_MESSAGE_HANDLER(InputMsg_SelectAll, OnSelectAll)
    IPC_MESSAGE_HANDLER(InputMsg_SelectRange, OnSelectRange)
    IPC_MESSAGE_HANDLER(InputMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(InputMsg_Undo, OnUndo)
    IPC_MESSAGE_HANDLER(InputMsg_Unselect, OnUnselect)
    IPC_MESSAGE_HANDLER(FrameMsg_Navigate, OnNavigate)
    IPC_MESSAGE_HANDLER(ViewMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(ViewMsg_ReloadFrame, OnReloadFrame)
    IPC_MESSAGE_HANDLER(ViewMsg_SetName, OnSetName)
    IPC_MESSAGE_HANDLER(ViewMsg_SetEditableSelectionOffsets,
                        OnSetEditableSelectionOffsets)
    IPC_MESSAGE_HANDLER(ViewMsg_SetCompositionFromExistingText,
                        OnSetCompositionFromExistingText)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtendSelectionAndDelete,
                        OnExtendSelectionAndDelete)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyImageAt, OnCopyImageAt)
    IPC_MESSAGE_HANDLER(ViewMsg_Find, OnFind)
    IPC_MESSAGE_HANDLER(ViewMsg_StopFinding, OnStopFinding)
    IPC_MESSAGE_HANDLER(ViewMsg_Zoom, OnZoom)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevel, OnSetZoomLevel)
    IPC_MESSAGE_HANDLER(ViewMsg_ZoomFactor, OnZoomFactor)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForLoadingURL,
                        OnSetZoomLevelForLoadingURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageEncoding, OnSetPageEncoding)
    IPC_MESSAGE_HANDLER(ViewMsg_ResetPageEncodingToDefault,
                        OnResetPageEncodingToDefault)
    IPC_MESSAGE_HANDLER(ViewMsg_ScriptEvalRequest, OnScriptEvalRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_PostMessageEvent, OnPostMessageEvent)
    IPC_MESSAGE_HANDLER(ViewMsg_CSSInsertRequest, OnCSSInsertRequest)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(DragMsg_SourceEndedOrMoved, OnDragSourceEndedOrMoved)
    IPC_MESSAGE_HANDLER(DragMsg_SourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowBindings, OnAllowBindings)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_TimezoneChange, OnUpdateTimezone)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextZoomFactor, OnSetTextZoomFactor)
    IPC_MESSAGE_HANDLER(ViewMsg_EnumerateDirectoryResponse,
                        OnEnumerateDirectoryResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_RunFileChooserResponse, OnFileChooserResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_ShouldClose, OnShouldClose)
    IPC_MESSAGE_HANDLER(ViewMsg_SuppressDialogsUntilSwapOut,
                        OnSuppressDialogsUntilSwapOut)
    IPC_MESSAGE_HANDLER(ViewMsg_SwapOut, OnSwapOut)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_ThemeChanged, OnThemeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearFocusedNode, OnClearFocusedNode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBackground, OnSetBackground)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableAutoResize, OnEnableAutoResize)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableAutoResize, OnDisableAutoResize)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableScrollbarsForSmallWindows,
                        OnDisableScrollbarsForSmallWindows)
    IPC_MESSAGE_HANDLER(ViewMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlayerActionAt, OnMediaPlayerActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_OrientationChangeEvent,
                        OnOrientationChangeEvent)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginActionAt, OnPluginActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_SetActive, OnSetActive)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                        OnGetAllSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(
        ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
        OnGetSerializedHtmlDataForCurrentPageWithLocalLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowContextMenu, OnShowContextMenu)
    // TODO(viettrungluu): Move to a separate message filter.
    IPC_MESSAGE_HANDLER(ViewMsg_SetHistoryLengthAndPrune,
                        OnSetHistoryLengthAndPrune)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableViewSourceMode, OnEnableViewSourceMode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAccessibilityMode, OnSetAccessibilityMode)
    IPC_MESSAGE_HANDLER(ViewMsg_DisownOpener, OnDisownOpener)
    IPC_MESSAGE_HANDLER(ViewMsg_ReleaseDisambiguationPopupDIB,
                        OnReleaseDisambiguationPopupDIB)
	#if defined(S_TRANSPORT_DIB_FOR_SOFT_BITMAP)					
    IPC_MESSAGE_HANDLER(ViewMsg_ReleaseSnapshotDIB,
                        OnReleaseSnapshotDIB) 
	#endif //S_TRANSPORT_DIB_FOR_SOFT_BITMAP					                      
    IPC_MESSAGE_HANDLER(ViewMsg_WindowSnapshotCompleted,
                        OnWindowSnapshotCompleted)

    IPC_MESSAGE_HANDLER(ViewMsg_HandleSelectionDrop, OnHandleSelectionDrop)
    IPC_MESSAGE_HANDLER(ViewMsg_HandleMouseClickWithCtrlkey,OnHandleMouseClickWithCtrlkey)
    IPC_MESSAGE_HANDLER(ViewMsg_HandleSelectionDropOnFocusedInput, OnHandleSelectionDropOnFocusedInput)
    IPC_MESSAGE_HANDLER(ViewMsg_GetFocusedInputInfo, OnGetFocusedInputInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_GetSelectionVisibilityStatus, OnGetSelectionVisibilityStatus)
    IPC_MESSAGE_HANDLER(ViewMsg_CheckBelongToSelection, OnCheckBelongToSelection)
    IPC_MESSAGE_HANDLER(ViewMsg_GetSelectionBitmap, OnGetSelectionBitmap)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectClosestWord, OnSelectClosestWord)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearTextSelection, OnClearTextSelection)
    IPC_MESSAGE_HANDLER(ViewMsg_GetSelectionMarkup, OnGetSelectionMarkup)
#if defined(SBROWSER_MULTI_SELECTION)
    IPC_MESSAGE_HANDLER(ViewMsg_GetSelectionMarkupWithBounds, OnGetSelectionMarkupWithBounds)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_LoadDataWithBaseUrl, OnLoadDataWithBaseUrl)
    IPC_MESSAGE_HANDLER(ViewMsg_LongPressOnFocused, OnLongPressOnFocused)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectLinkText, OnSelectLinkText);
    IPC_MESSAGE_HANDLER(ViewMsg_RecognizeArticle, OnRecognizeArticle)

    IPC_MESSAGE_HANDLER(ViewMsg_GetBitmapFromCachedResource,
                        OnGetBitmapFromCachedResource)

#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(InputMsg_ActivateNearestFindResult,
                        OnActivateNearestFindResult)
    IPC_MESSAGE_HANDLER(ViewMsg_FindMatchRects, OnFindMatchRects)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectPopupMenuItems, OnSelectPopupMenuItems)
    IPC_MESSAGE_HANDLER(ViewMsg_UndoScrollFocusedEditableNodeIntoView,
                        OnUndoScrollFocusedEditableNodeIntoRect)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTopControlsState,
                        OnUpdateTopControlsState)
    IPC_MESSAGE_HANDLER(ViewMsg_PauseVideo, OnPauseVideo)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtractSmartClipData, OnExtractSmartClipData)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(InputMsg_CopyToFindPboard, OnCopyToFindPboard)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginImeCompositionCompleted,
                        OnPluginImeCompositionCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectPopupMenuItem, OnSelectPopupMenuItem)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInLiveResize, OnSetInLiveResize)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWindowVisibility, OnSetWindowVisibility)
    IPC_MESSAGE_HANDLER(ViewMsg_WindowFrameChanged, OnWindowFrameChanged)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_CaptureRendererContentSnapShot,
                        OnCaptureRendererContentSnapShot)
    // Adding a new message? Add platform independent ones first, then put the
    // platform specific ones at the end.
    IPC_MESSAGE_HANDLER(ViewMsg_HoverHighlight, OnHoverHighlight)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveToPreviousTextOrSelectElement,
                        OnMovePreviousTextOrSelectElement)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveToNextTextOrSelectElement,
                        OnMoveNextTextOrSelectElement)
    IPC_MESSAGE_HANDLER(ViewMsg_GetTouchedFixedElementHeight,OnGetTouchedFixedElementHeight)

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()

  if (!msg_is_ok) {
    // The message had a handler, but its deserialization failed.
    // Kill the renderer to avoid potential spoofing attacks.
    CHECK(false) << "Unable to deserialize message in RenderViewImpl.";
  }

  return handled;
}

void RenderViewImpl::OnNavigate(const FrameMsg_Navigate_Params& params) {
  LOG(INFO) << "[APP_LAUNCH][SBRCHECK_LU] RenderViewImpl::OnNavigate: Received IPC [LoadURL] [RenderProcess] [RoutingID=" << GetRoutingID() << "]";
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, Navigate(params.url));
}

bool RenderViewImpl::IsBackForwardToStaleEntry(
    const FrameMsg_Navigate_Params& params,
    bool is_reload) {
  // Make sure this isn't a back/forward to an entry we have already cropped
  // or replaced from our history, before the browser knew about it.  If so,
  // a new navigation has committed in the mean time, and we can ignore this.
  bool is_back_forward = !is_reload && params.page_state.IsValid();

  // Note: if the history_list_length_ is 0 for a back/forward, we must be
  // restoring from a previous session.  We'll update our state in OnNavigate.
  if (!is_back_forward || history_list_length_ <= 0)
    return false;

  DCHECK_EQ(static_cast<int>(history_page_ids_.size()), history_list_length_);

  // Check for whether the forward history has been cropped due to a recent
  // navigation the browser didn't know about.
  if (params.pending_history_list_offset >= history_list_length_)
    return true;

  // Check for whether this entry has been replaced with a new one.
  int expected_page_id =
      history_page_ids_[params.pending_history_list_offset];
  if (expected_page_id > 0 && params.page_id != expected_page_id) {
    if (params.page_id < expected_page_id)
      return true;

    // Otherwise we've removed an earlier entry and should have shifted all
    // entries left.  For now, it's ok to lazily update the list.
    // TODO(creis): Notify all live renderers when we remove entries from
    // the front of the list, so that we don't hit this case.
    history_page_ids_[params.pending_history_list_offset] = params.page_id;
  }

  return false;
}

// Stop loading the current page.
void RenderViewImpl::OnStop() {
  if (webview())
    webview()->mainFrame()->stopLoading();
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, OnStop());
  main_render_frame_->OnStop();
}

// Reload current focused frame.
// E.g. called by right-clicking on the frame and picking "reload this frame".
void RenderViewImpl::OnReloadFrame() {
  if (webview() && webview()->focusedFrame()) {
    // We always obey the cache (ignore_cache=false) here.
    // TODO(evanm): perhaps we could allow shift-clicking the menu item to do
    // a cache-ignoring reload of the frame.
    webview()->focusedFrame()->reload(false);
  }
}

void RenderViewImpl::OnCopyImageAt(int x, int y) {
  webview()->copyImageAt(WebPoint(x, y));
}

void RenderViewImpl::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING) {
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_,
                                         pending_target_url_));
  }

  target_url_status_ = TARGET_NONE;
}

void RenderViewImpl::OnCopy() {
  if (!webview())
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  WebNode current_node = context_menu_node_.isNull() ?
      GetFocusedNode() : context_menu_node_;
  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Copy"),
                                            current_node);
}

void RenderViewImpl::OnCut() {
  if (!webview())
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Cut"),
                                            GetFocusedNode());
}

void RenderViewImpl::OnDelete() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Delete"),
                                            GetFocusedNode());
}

void RenderViewImpl::OnExecuteEditCommand(const std::string& name,
    const std::string& value) {
  if (!webview() || !webview()->focusedFrame())
    return;
#if defined(S_PLM_P140723_07592) 
  if(name=="Unselect")
  {
      OnUnselect();	
  }
#endif
  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8(name), WebString::fromUTF8(value));
}

void RenderViewImpl::OnSelectLinkText(const gfx::Point& point) {
  if (!webview() || !webview()->focusedFrame())
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  webview()->focusedFrame()->selectLinkText(point);
}

void RenderViewImpl::OnGetBitmapFromCachedResource(
    const std::string& image_url) {
  if (!webview())
    return;

  SkBitmap bitmap = webview()->bitmapFromCachedResource(
      WebString::fromUTF8(image_url)).getSkBitmap();
  LOG(INFO)<<"SCRAP :: OnGetBitmapFromCachedResource ::  bitmap.getSize() ::"<< bitmap.getSize()<<":bitmap.isNull():"<<bitmap.isNull();
  Send(new ViewHostMsg_OnGetBitmapFromCachedResource(routing_id_,bitmap));
}

void RenderViewImpl::OnMoveCaret(const gfx::Point& point) {
  if (!webview())
    return;

  Send(new ViewHostMsg_MoveCaret_ACK(routing_id_));

  webview()->focusedFrame()->moveCaretSelection(point);
}

void RenderViewImpl::OnPaste() {
  if (!webview())
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Paste"),
                                            GetFocusedNode());
}

void RenderViewImpl::OnPasteAndMatchStyle() {
  if (!webview())
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("PasteAndMatchStyle"), GetFocusedNode());
}

void RenderViewImpl::OnDirectPaste(const base::string16& text) {
  if (!webview())
    return;

  blink::WebNode node = GetFocusedNode();
  if (node.isNull() || !IsEditableNode(node))
    return;

  webview()->focusedFrame()->replaceSelection(text);
}

void RenderViewImpl::OnRedo() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Redo"),
                                            GetFocusedNode());
}

void RenderViewImpl::OnReplace(const base::string16& text) {
  if (!webview())
    return;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame->hasSelection())
    frame->selectWordAroundCaret();

  frame->replaceSelection(text);
}

void RenderViewImpl::OnReplaceMisspelling(const base::string16& text) {
  if (!webview())
    return;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame->hasSelection())
    return;

  frame->replaceMisspelledRange(text);
}

void RenderViewImpl::OnScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  blink::WebNode node = GetFocusedNode();
  if (!node.isNull()) {
    if (IsEditableNode(node)) {
      webview()->saveScrollAndScaleState();
      webview()->scrollFocusedNodeIntoRect(rect);
    } else {
      // Samsung specific. For form navigation feature we use this ipc to
      // move other nodes as well.
      // FIXME: This else phrase doesn't look great. However, creating an IPC
      // message for one liner implementation is expensive. Please improve here.
      webview()->scrollFocusedNodeIntoView();
    }
  }
}

void RenderViewImpl::OnSelectAll() {
  if (!webview())
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("SelectAll"), GetFocusedNode());
}

void RenderViewImpl::OnSelectRange(const gfx::Point& start,
                                   const gfx::Point& end
                                   #if defined(S_MULTISELECTION_BOUNDS)
								   ,bool isLastTouchPoint,bool isFirstTouchPoint
								   #endif
								   ) 
{
  if (!webview())
    return;

  Send(new ViewHostMsg_SelectRange_ACK(routing_id_));
	LOG(INFO)<<"PERF: RenderViewImpl::OnSelectRange  = "<<start.x()<<"  "<<start.y();
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
 #if defined(S_MULTISELECTION_BOUNDS)
  webview()->focusedFrame()->selectRange(start, end,isLastTouchPoint,isFirstTouchPoint);
 #else
   webview()->focusedFrame()->selectRange(start, end);
 #endif
}

void RenderViewImpl::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderViewImpl::OnUndo() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Undo"),
                                            GetFocusedNode());
}

void RenderViewImpl::OnUnselect() {
  if (!webview())
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Unselect"),
                                            GetFocusedNode());
}

#if defined(OS_MACOSX)
void RenderViewImpl::OnCopyToFindPboard() {
  if (!webview())
    return;

  // Since the find pasteboard supports only plain text, this can be simpler
  // than the |OnCopy()| case.
  WebFrame* frame = webview()->focusedFrame();
  if (frame->hasSelection()) {
    base::string16 selection = frame->selectionAsText();
    RenderThread::Get()->Send(
        new ClipboardHostMsg_FindPboardWriteStringAsync(selection));
  }
}
#endif

void RenderViewImpl::OnSetName(const std::string& name) {
  if (!webview())
    return;

  webview()->mainFrame()->setName(WebString::fromUTF8(name));
}

void RenderViewImpl::OnSetEditableSelectionOffsets(int start, int end) {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  webview()->setEditableSelectionOffsets(start, end);
}

void RenderViewImpl::OnSetCompositionFromExistingText(
    int start, int end,
    const std::vector<blink::WebCompositionUnderline>& underlines) {
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  webview()->setCompositionFromExistingText(start, end, underlines);
}

void RenderViewImpl::OnExtendSelectionAndDelete(int before, int after) {
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  webview()->extendSelectionAndDelete(before, after);
}

void RenderViewImpl::OnSetHistoryLengthAndPrune(int history_length,
                                                int32 minimum_page_id) {
  DCHECK_GE(history_length, 0);
  DCHECK(history_list_offset_ == history_list_length_ - 1);
  DCHECK_GE(minimum_page_id, -1);

  // Generate the new list.
  std::vector<int32> new_history_page_ids(history_length, -1);
  for (size_t i = 0; i < history_page_ids_.size(); ++i) {
    if (minimum_page_id >= 0 && history_page_ids_[i] < minimum_page_id)
      continue;
    new_history_page_ids.push_back(history_page_ids_[i]);
  }
  new_history_page_ids.swap(history_page_ids_);

  // Update indexes.
  history_list_length_ = history_page_ids_.size();
  history_list_offset_ = history_list_length_ - 1;
}


void RenderViewImpl::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->setInitialFocus(reverse);
}

#if defined(OS_MACOSX)
void RenderViewImpl::OnSetInLiveResize(bool in_live_resize) {
  if (!webview())
    return;
  if (in_live_resize)
    webview()->willStartLiveResize();
  else
    webview()->willEndLiveResize();
}
#endif

#if defined(OS_ANDROID)
void RenderViewImpl::OnUndoScrollFocusedEditableNodeIntoRect() {
  const WebNode node = GetFocusedNode();
  if (!node.isNull() && IsEditableNode(node))
    webview()->restoreScrollAndScaleState();
}

void RenderViewImpl::OnPauseVideo() {
  // Inform RendererMediaPlayerManager to release all video player resources.
  // If something is in progress the resource will not be freed, it will
  // only be freed once the tab is destroyed or if the user navigates away
  // via WebMediaPlayerAndroid::Destroy.
  media_player_manager_->ReleaseVideoResources();
}
#endif

///////////////////////////////////////////////////////////////////////////////

// Tell the embedding application that the title of the active page has changed
void RenderViewImpl::UpdateTitle(WebFrame* frame,
                                 const base::string16& title,
                                 WebTextDirection title_direction) {
  // Ignore all but top level navigations.
  if (frame->parent())
    return;

  base::debug::TraceLog::GetInstance()->UpdateProcessLabel(
      routing_id_, base::UTF16ToUTF8(title));

  base::string16 shortened_title = title.substr(0, kMaxTitleChars);
  Send(new ViewHostMsg_UpdateTitle(routing_id_, page_id_, shortened_title,
                                   title_direction));
}

void RenderViewImpl::UpdateEncoding(WebFrame* frame,
                                    const std::string& encoding_name) {
  // Only update main frame's encoding_name.
  if (webview()->mainFrame() == frame &&
      last_encoding_name_ != encoding_name) {
    // Save the encoding name for later comparing.
    last_encoding_name_ = encoding_name;

    Send(new ViewHostMsg_UpdateEncoding(routing_id_, last_encoding_name_));
  }
}

// Sends the last committed session history state to the browser so it will be
// saved before we navigate to a new page. This must be called *before* the
// page ID has been updated so we know what it was.
void RenderViewImpl::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  const WebHistoryItem& item =
      webview()->mainFrame()->previousHistoryItem();
  SendUpdateState(item);
}

void RenderViewImpl::SendUpdateState(const WebHistoryItem& item) {
  if (item.isNull())
    return;

  // Don't send state updates for kSwappedOutURL.
  if (item.urlString() == WebString::fromUTF8(kSwappedOutURL))
    return;

  Send(new ViewHostMsg_UpdateState(
      routing_id_, page_id_, HistoryItemToPageState(item)));
}

void RenderViewImpl::OpenURL(WebFrame* frame,
                             const GURL& url,
                             const Referrer& referrer,
                             WebNavigationPolicy policy) {
  ViewHostMsg_OpenURL_Params params;
  params.url = url;
  params.referrer = referrer;
  params.disposition = NavigationPolicyToDisposition(policy);
  params.frame_id = frame->identifier();
  WebDataSource* ds = frame->provisionalDataSource();
  if (ds) {
    DocumentState* document_state = DocumentState::FromDataSource(ds);
    NavigationState* navigation_state = document_state->navigation_state();
    if (navigation_state->is_content_initiated()) {
      params.should_replace_current_entry = ds->replacesCurrentHistoryItem();
    } else {
      // This is necessary to preserve the should_replace_current_entry value on
      // cross-process redirects, in the event it was set by a previous process.
      //
      // TODO(davidben): Avoid this awkward duplication of state. See comment on
      // NavigationState::should_replace_current_entry().
      params.should_replace_current_entry =
          navigation_state->should_replace_current_entry();
    }
  } else {
    params.should_replace_current_entry = false;
  }
  params.user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  if (GetContentClient()->renderer()->AllowPopup())
    params.user_gesture = true;

  if (policy == blink::WebNavigationPolicyNewBackgroundTab ||
      policy == blink::WebNavigationPolicyNewForegroundTab ||
      policy == blink::WebNavigationPolicyNewWindow ||
      policy == blink::WebNavigationPolicyNewPopup) {
    WebUserGestureIndicator::consumeUserGesture();
  }

  Send(new ViewHostMsg_OpenURL(routing_id_, params));
}

// WebViewDelegate ------------------------------------------------------------

void RenderViewImpl::LoadNavigationErrorPage(
    WebFrame* frame,
    const WebURLRequest& failed_request,
    const WebURLError& error,
    bool replace) {
  std::string error_html;
  GetContentClient()->renderer()->GetNavigationErrorStrings(
      this, frame, failed_request, error, &error_html, NULL);

  frame->loadHTMLString(error_html,
                        GURL(kUnreachableWebDataURL),
                        error.unreachableURL,
                        replace);
}

bool RenderViewImpl::RunJavaScriptMessage(JavaScriptMessageType type,
                                          const base::string16& message,
                                          const base::string16& default_value,
                                          const GURL& frame_url,
                                          base::string16* result) {
  // Don't allow further dialogs if we are waiting to swap out, since the
  // PageGroupLoadDeferrer in our stack prevents it.
  if (suppress_dialogs_until_swap_out_)
    return false;

  bool success = false;
  base::string16 result_temp;
  if (!result)
    result = &result_temp;

  SendAndRunNestedMessageLoop(new ViewHostMsg_RunJavaScriptMessage(
      routing_id_, message, default_value, frame_url, type, &success, result));
  return success;
}

bool RenderViewImpl::SendAndRunNestedMessageLoop(IPC::SyncMessage* message) {
  // Before WebKit asks us to show an alert (etc.), it takes care of doing the
  // equivalent of WebView::willEnterModalLoop.  In the case of showModalDialog
  // it is particularly important that we do not call willEnterModalLoop as
  // that would defer resource loads for the dialog itself.
  if (RenderThreadImpl::current())  // Will be NULL during unit tests.
    RenderThreadImpl::current()->DoNotNotifyWebKitOfModalLoop();

  message->EnableMessagePumping();  // Runs a nested message loop.
  return Send(message);
}

void RenderViewImpl::GetWindowSnapshot(const WindowSnapshotCallback& callback) {
  int id = next_snapshot_id_++;
  pending_snapshots_.insert(std::make_pair(id, callback));
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT,
                                0,
                                id);
  scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  if (RenderWidgetCompositor* rwc = compositor()) {
    latency_info_swap_promise_monitor =
        rwc->CreateLatencyInfoSwapPromiseMonitor(&latency_info).Pass();
  } else {
    latency_info_.push_back(latency_info);
  }
  ScheduleCompositeWithForcedRedraw();
}

void RenderViewImpl::OnWindowSnapshotCompleted(const int snapshot_id,
    const gfx::Size& size, const std::vector<unsigned char>& png) {

  // Any pending snapshots with a lower ID than the one received are considered
  // to be implicitly complete, and returned the same snapshot data.
  PendingSnapshotMap::iterator it = pending_snapshots_.begin();
  while(it != pending_snapshots_.end()) {
      if (it->first <= snapshot_id) {
        it->second.Run(size, png);
        pending_snapshots_.erase(it++);
      } else {
        ++it;
      }
  }
}

// blink::WebViewClient ------------------------------------------------------

WebView* RenderViewImpl::createView(
    WebFrame* creator,
    const WebURLRequest& request,
    const WebWindowFeatures& features,
    const WebString& frame_name,
    WebNavigationPolicy policy,
    bool suppress_opener) {
  ViewHostMsg_CreateWindow_Params params;
  params.opener_id = routing_id_;
  params.user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  if (GetContentClient()->renderer()->AllowPopup())
    params.user_gesture = true;
  params.window_container_type = WindowFeaturesToContainerType(features);
  params.session_storage_namespace_id = session_storage_namespace_id_;
  if (frame_name != "_blank")
    params.frame_name = frame_name;
  params.opener_frame_id = creator->identifier();
  params.opener_url = creator->document().url();
  params.opener_top_level_frame_url = creator->top()->document().url();
  GURL security_url(creator->document().securityOrigin().toString().utf8());
  if (!security_url.is_valid())
    security_url = GURL();
  params.opener_security_origin = security_url;
  params.opener_suppressed = suppress_opener;
  params.disposition = NavigationPolicyToDisposition(policy);
  if (!request.isNull()) {
    params.target_url = request.url();
    params.referrer = GetReferrerFromRequest(creator, request);
  }
  params.features = features;

  for (size_t i = 0; i < features.additionalFeatures.size(); ++i)
    params.additional_features.push_back(features.additionalFeatures[i]);

  int32 routing_id = MSG_ROUTING_NONE;
  int32 main_frame_routing_id = MSG_ROUTING_NONE;
  int32 surface_id = 0;
  int64 cloned_session_storage_namespace_id;

  RenderThread::Get()->Send(
      new ViewHostMsg_CreateWindow(params,
                                   &routing_id,
                                   &main_frame_routing_id,
                                   &surface_id,
                                   &cloned_session_storage_namespace_id));
  if (routing_id == MSG_ROUTING_NONE)
    return NULL;

  WebUserGestureIndicator::consumeUserGesture();

  WebPreferences transferred_preferences = webkit_preferences_;

  // Unless accelerated compositing has been explicitly disabled from the
  // command line (e.g. via the blacklist or about:flags) re-enable it for
  // new views that get spawned by this view. This gets around the issue that
  // background extension pages disable accelerated compositing via web prefs
  // but can themselves spawn a visible render view which should be allowed
  // use gpu acceleration.
  if (!webkit_preferences_.accelerated_compositing_enabled) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kDisableAcceleratedCompositing))
      transferred_preferences.accelerated_compositing_enabled = true;
  }

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  RenderViewImpl* view = RenderViewImpl::Create(
      routing_id_,
      renderer_preferences_,
      transferred_preferences,
      routing_id,
      main_frame_routing_id,
      surface_id,
      cloned_session_storage_namespace_id,
      base::string16(),  // WebCore will take care of setting the correct name.
      true,  // is_renderer_created
      false, // swapped_out
      params.disposition == NEW_BACKGROUND_TAB, // hidden
      1,     // next_page_id
      screen_info_,
      accessibility_mode_,
      allow_partial_swap_);
  view->opened_by_user_gesture_ = params.user_gesture;

  // Record whether the creator frame is trying to suppress the opener field.
  view->opener_suppressed_ = params.opener_suppressed;

  return view->webview();
}

void RenderViewImpl::performMouseClick() {
#if defined(OS_ANDROID)
  UpdateTextInputState(true, true);
#endif
}

void RenderViewImpl::messageToClosePopup() {
  SendClosePopup();
}

WebWidget* RenderViewImpl::createPopupMenu(blink::WebPopupType popup_type) {
  RenderWidget* widget =
      RenderWidget::Create(routing_id_, popup_type, screen_info_);
  if (screen_metrics_emulator_) {
    widget->SetPopupOriginAdjustmentsForEmulation(
        screen_metrics_emulator_.get());
  }
  return widget->webwidget();
}

WebExternalPopupMenu* RenderViewImpl::createExternalPopupMenu(
    const WebPopupMenuInfo& popup_menu_info,
    WebExternalPopupMenuClient* popup_menu_client) {
  // An IPC message is sent to the browser to build and display the actual
  // popup.  The user could have time to click a different select by the time
  // the popup is shown.  In that case external_popup_menu_ is non NULL.
  // By returning NULL in that case, we instruct WebKit to cancel that new
  // popup.  So from the user perspective, only the first one will show, and
  // will have to close the first one before another one can be shown.
  if (external_popup_menu_)
    return NULL;
  external_popup_menu_.reset(
      new ExternalPopupMenu(this, popup_menu_info, popup_menu_client));
  if (screen_metrics_emulator_) {
    SetExternalPopupOriginAdjustmentsForEmulation(
        external_popup_menu_.get(), screen_metrics_emulator_.get());
  }
  return external_popup_menu_.get();
}

void RenderViewImpl::destroyExternalPopupMenu(
  WebExternalPopupMenuClient* popup_menu_client) {
  if (!external_popup_menu_.get())
    return;

  if (external_popup_menu_->IsSamePopupMenuClient(popup_menu_client)) {
    LOG(INFO) << __FUNCTION__<<" Destroying popup_menu_client , hence close and reset external_popup_menu_!!";
    external_popup_menu_->close();
    external_popup_menu_.reset();
  }
}

WebStorageNamespace* RenderViewImpl::createSessionStorageNamespace() {
  CHECK(session_storage_namespace_id_ != kInvalidSessionStorageNamespaceId);
  return new WebStorageNamespaceImpl(session_storage_namespace_id_);
}

bool RenderViewImpl::setBingAsCurrentSearchDefault() {
  Send(new ViewHostMsg_SetBingAsCurrentSearchDefault(routing_id_));
  return true;
}

bool RenderViewImpl::isBingCurrentSearchDefault() {
#if defined(S_NATIVE_SUPPORT)
  return webkit_preferences_.is_bing_as_default_search_engine;
#endif
  return false;
}

bool RenderViewImpl::shouldReportDetailedMessageForSource(
    const WebString& source) {
  return GetContentClient()->renderer()->ShouldReportDetailedMessageForSource(
      source);
}

void RenderViewImpl::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line, const WebString& stack_trace) {
  logging::LogSeverity log_severity = logging::LOG_VERBOSE;
  switch (message.level) {
    case WebConsoleMessage::LevelDebug:
      log_severity = logging::LOG_VERBOSE;
      break;
    case WebConsoleMessage::LevelLog:
    case WebConsoleMessage::LevelInfo:
      log_severity = logging::LOG_INFO;
      break;
    case WebConsoleMessage::LevelWarning:
      log_severity = logging::LOG_WARNING;
      break;
    case WebConsoleMessage::LevelError:
      log_severity = logging::LOG_ERROR;
      break;
    default:
      NOTREACHED();
  }
#if defined(S_PLM_P140726_00779)
WebString dummy;
  if (shouldReportDetailedMessageForSource(source_name)) {
    FOR_EACH_OBSERVER(
        RenderViewObserver,
        observers_,
        DetailedConsoleMessageAdded(message.text,
                                    dummy,
                                    stack_trace,
                                    source_line,
                                    static_cast<int32>(log_severity)));
  }

  Send(new ViewHostMsg_AddMessageToConsole(routing_id_,
                                           static_cast<int32>(log_severity),
                                           message.text,
                                           static_cast<int32>(source_line),
                                           dummy));
#else
  if (shouldReportDetailedMessageForSource(source_name)) {
    FOR_EACH_OBSERVER(
        RenderViewObserver,
        observers_,
        DetailedConsoleMessageAdded(message.text,
                                    source_name,
                                    stack_trace,
                                    source_line,
                                    static_cast<int32>(log_severity)));
  }

  Send(new ViewHostMsg_AddMessageToConsole(routing_id_,
                                           static_cast<int32>(log_severity),
                                           message.text,
                                           static_cast<int32>(source_line),
                                           source_name));
  #endif
}

void RenderViewImpl::printPage(WebFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    PrintPage(frame, handling_input_event_));
}

#if defined(S_NOTIFY_ROTATE_STATUS)
void RenderViewImpl::notifyRotateStatus() {
    Send(new ViewHostMsg_NotifyRotateStatus(routing_id_));
}
#endif

blink::WebNotificationPresenter* RenderViewImpl::notificationPresenter() {
  return notification_provider_;
}

bool RenderViewImpl::enumerateChosenDirectory(
    const WebString& path,
    WebFileChooserCompletion* chooser_completion) {
  int id = enumeration_completion_id_++;
  enumeration_completions_[id] = chooser_completion;
  return Send(new ViewHostMsg_EnumerateDirectory(
      routing_id_,
      id,
      base::FilePath::FromUTF16Unsafe(path)));
}

void RenderViewImpl::initializeHelperPluginWebFrame(
    blink::WebHelperPlugin* plugin) {
  plugin->initializeFrame(main_render_frame_.get());
}

void RenderViewImpl::didStartLoading(bool to_different_document) {
  didStartLoading();
}

void RenderViewImpl::enableDrawSuspend() {
  if (page_load_draw_suspend_time_ > TimeDelta::FromSeconds(0)) {
    is_draw_suspended_ = true;
    compositor()->setVisible(false);
    draw_suspend_timer.Start(FROM_HERE, page_load_draw_suspend_time_, this,
                              &RenderViewImpl::disableDrawSuspend);
  }
}

void RenderViewImpl::disableDrawSuspend() {
  if (is_draw_suspended_) {
    is_draw_suspended_ = false;
    draw_suspend_timer.Stop();
    compositor()->setVisible(true);
    OnWasShown(true);
  }
}

void RenderViewImpl::didStartLoading() {
  if (is_loading_) {
    DVLOG(1) << "didStartLoading called while loading";
    return;
  }

  is_loading_ = true;
//To clear the selection while page is loading as handlers are hidden while loading.
#if defined(SBROWSER_PLM_P140701_01971)
if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  main_frame->clearTextSelection();
#endif
  // Send the IPC message through the top-level frame.
  main_render_frame_->didStartLoading();
  enableDrawSuspend();

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidStartLoading());
}

void RenderViewImpl::didStopLoading() {
  if (!is_loading_) {
    DVLOG(1) << "DidStopLoading called while not loading";
    return;
  }

  is_loading_ = false;

  disableDrawSuspend();

  // NOTE: For now we're doing the safest thing, and sending out notification
  // when done loading. This currently isn't an issue as the favicon is only
  // displayed when done loading. Ideally we would send notification when
  // finished parsing the head, but webkit doesn't support that yet.
  // The feed discovery code would also benefit from access to the head.
  // NOTE: Sending of the IPC message happens through the top-level frame.
  main_render_frame_->didStopLoading();

  if (load_progress_tracker_ != NULL)
    load_progress_tracker_->DidStopLoading();

  DidStopLoadingIcons();

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidStopLoading());
}

void RenderViewImpl::didChangeLoadProgress(WebFrame* frame,
                                           double load_progress) {
  if (load_progress_tracker_ != NULL)
    load_progress_tracker_->DidChangeLoadProgress(frame, load_progress);
}

void RenderViewImpl::didCancelCompositionOnSelectionChange() {
  Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
}

void RenderViewImpl::didChangeSelection(bool is_empty_selection
#if defined(S_MULTISELECTION_BOUNDS)
,bool isLastPointBounds
#endif
) {
  if (!handling_input_event_ && !handling_select_range_)
    return;

  if (is_empty_selection)
    selection_text_.clear();

  // UpdateTextInputType should be called before SyncSelectionIfRequired.
  // UpdateTextInputType may send TextInputTypeChanged to notify the focus
  // was changed, and SyncSelectionIfRequired may send SelectionChanged
  // to notify the selection was changed.  Focus change should be notified
  // before selection change.
  UpdateTextInputType();
#if defined(S_MULTISELECTION_BOUNDS)
   SyncSelectionIfRequired(isLastPointBounds);
#else
  SyncSelectionIfRequired();
#endif
#if defined(OS_ANDROID)
  UpdateTextInputState(false, true);
#endif
}

void RenderViewImpl::didExecuteCommand(const WebString& command_name) {
  const std::string& name = base::UTF16ToUTF8(command_name);
  if (StartsWithASCII(name, "Move", true) ||
      StartsWithASCII(name, "Insert", true) ||
      StartsWithASCII(name, "Delete", true))
    return;
  RenderThreadImpl::current()->RecordComputedAction(name);
}

bool RenderViewImpl::handleCurrentKeyboardEvent() {
  if (edit_commands_.empty())
    return false;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame)
    return false;

  EditCommands::iterator it = edit_commands_.begin();
  EditCommands::iterator end = edit_commands_.end();

  bool did_execute_command = false;
  for (; it != end; ++it) {
    // In gtk and cocoa, it's possible to bind multiple edit commands to one
    // key (but it's the exception). Once one edit command is not executed, it
    // seems safest to not execute the rest.
    if (!frame->executeCommand(WebString::fromUTF8(it->name),
                               WebString::fromUTF8(it->value),
                               GetFocusedNode()))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

blink::WebColorChooser* RenderViewImpl::createColorChooser(
    blink::WebColorChooserClient* client,
    const blink::WebColor& initial_color,
    const blink::WebVector<blink::WebColorSuggestion>& suggestions) {
  RendererWebColorChooserImpl* color_chooser =
      new RendererWebColorChooserImpl(this, client);
  std::vector<content::ColorSuggestion> color_suggestions;
  for (size_t i = 0; i < suggestions.size(); i++) {
    color_suggestions.push_back(content::ColorSuggestion(suggestions[i]));
  }
  color_chooser->Open(static_cast<SkColor>(initial_color), color_suggestions);
  return color_chooser;
}

bool RenderViewImpl::runFileChooser(
    const blink::WebFileChooserParams& params,
    WebFileChooserCompletion* chooser_completion) {
  // Do not open the file dialog in a hidden RenderView.
  if (is_hidden())
    return false;
  FileChooserParams ipc_params;
  if (params.directory)
    ipc_params.mode = FileChooserParams::UploadFolder;
  else if (params.multiSelect)
    ipc_params.mode = FileChooserParams::OpenMultiple;
  else if (params.saveAs)
    ipc_params.mode = FileChooserParams::Save;
  else
    ipc_params.mode = FileChooserParams::Open;
  ipc_params.title = params.title;
  ipc_params.default_file_name =
      base::FilePath::FromUTF16Unsafe(params.initialValue);
  ipc_params.accept_types.reserve(params.acceptTypes.size());
  for (size_t i = 0; i < params.acceptTypes.size(); ++i)
    ipc_params.accept_types.push_back(params.acceptTypes[i]);
#if defined(OS_ANDROID)
  ipc_params.capture = params.useMediaCapture;
#endif

  return ScheduleFileChooser(ipc_params, chooser_completion);
}

void RenderViewImpl::runModalAlertDialog(WebFrame* frame,
                                         const WebString& message) {
  RunJavaScriptMessage(JAVASCRIPT_MESSAGE_TYPE_ALERT,
                       message,
                       base::string16(),
                       frame->document().url(),
                       NULL);
}

bool RenderViewImpl::runModalConfirmDialog(WebFrame* frame,
                                           const WebString& message) {
  return RunJavaScriptMessage(JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
                              message,
                              base::string16(),
                              frame->document().url(),
                              NULL);
}

bool RenderViewImpl::runModalPromptDialog(WebFrame* frame,
                                          const WebString& message,
                                          const WebString& default_value,
                                          WebString* actual_value) {
  base::string16 result;
  bool ok = RunJavaScriptMessage(JAVASCRIPT_MESSAGE_TYPE_PROMPT,
                                 message,
                                 default_value,
                                 frame->document().url(),
                                 &result);
  if (ok)
    actual_value->assign(result);
  return ok;
}

bool RenderViewImpl::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  bool is_reload = false;
  WebDataSource* ds = frame->provisionalDataSource();
  if (ds)
    is_reload = (ds->navigationType() == blink::WebNavigationTypeReload);
  return runModalBeforeUnloadDialog(frame, is_reload, message);
}

bool RenderViewImpl::runModalBeforeUnloadDialog(
    WebFrame* frame, bool is_reload, const WebString& message) {
  // If we are swapping out, we have already run the beforeunload handler.
  // TODO(creis): Fix OnSwapOut to clear the frame without running beforeunload
  // at all, to avoid running it twice.
  if (is_swapped_out_)
    return true;

  // Don't allow further dialogs if we are waiting to swap out, since the
  // PageGroupLoadDeferrer in our stack prevents it.
  if (suppress_dialogs_until_swap_out_)
    return false;

  bool success = false;
  // This is an ignored return value, but is included so we can accept the same
  // response as RunJavaScriptMessage.
  base::string16 ignored_result;
  SendAndRunNestedMessageLoop(new ViewHostMsg_RunBeforeUnloadConfirm(
      routing_id_, frame->document().url(), message, is_reload,
      &success, &ignored_result));
  return success;
}

void RenderViewImpl::showValidationMessage(
    const blink::WebRect& anchor_in_root_view,
    const blink::WebString& main_text,
    const blink::WebString& sub_text,
    blink::WebTextDirection hint) {
  base::string16 wrapped_main_text = main_text;
  base::string16 wrapped_sub_text = sub_text;
  if (hint == blink::WebTextDirectionLeftToRight) {
    wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(wrapped_main_text);
    if (!wrapped_sub_text.empty()) {
      wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(wrapped_sub_text);
    }
  } else if (hint == blink::WebTextDirectionRightToLeft
             && !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(&wrapped_main_text);
    if (!wrapped_sub_text.empty()) {
      base::i18n::WrapStringWithRTLFormatting(&wrapped_sub_text);
    }
  }
  Send(new ViewHostMsg_ShowValidationMessage(
    routing_id(), anchor_in_root_view, wrapped_main_text, wrapped_sub_text));
}

void RenderViewImpl::hideValidationMessage() {
  Send(new ViewHostMsg_HideValidationMessage(routing_id()));
}

void RenderViewImpl::moveValidationMessage(
    const blink::WebRect& anchor_in_root_view) {
  Send(new ViewHostMsg_MoveValidationMessage(routing_id(),
                                             anchor_in_root_view));
}

void RenderViewImpl::showContextMenu(
    WebFrame* frame, const blink::WebContextMenuData& data) {
  // TODO(jam): move this method to WebFrameClient.
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(frame);
  render_frame->showContextMenu(data);
}

void RenderViewImpl::clearContextMenu() {
  context_menu_node_.reset();
}

void RenderViewImpl::setStatusText(const WebString& text) {
}

void RenderViewImpl::UpdateTargetURL(const GURL& url,
                                     const GURL& fallback_url) {
  GURL latest_url = url.is_empty() ? fallback_url : url;
  if (latest_url == target_url_)
    return;

  // Tell the browser to display a destination link.
  if (target_url_status_ == TARGET_INFLIGHT ||
      target_url_status_ == TARGET_PENDING) {
    // If we have a request in-flight, save the URL to be sent when we
    // receive an ACK to the in-flight request. We can happily overwrite
    // any existing pending sends.
    pending_target_url_ = latest_url;
    target_url_status_ = TARGET_PENDING;
  } else {
    // URLs larger than |MaxURLChars()| cannot be sent through IPC -
    // see |ParamTraits<GURL>|.
    if (latest_url.possibly_invalid_spec().size() > GetMaxURLChars())
      latest_url = GURL();
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_, latest_url));
    target_url_ = latest_url;
    target_url_status_ = TARGET_INFLIGHT;
  }
}

gfx::RectF RenderViewImpl::ClientRectToPhysicalWindowRect(
    const gfx::RectF& rect) const {
  gfx::RectF window_rect = rect;
  window_rect.Scale(device_scale_factor_ * webview()->pageScaleFactor());
  return window_rect;
}

void RenderViewImpl::StartNavStateSyncTimerIfNecessary() {
  // No need to update state if no page has committed yet.
  if (page_id_ == -1)
    return;

  int delay;
  if (send_content_state_immediately_)
    delay = 0;
  else if (is_hidden())
    delay = kDelaySecondsForContentStateSyncHidden;
  else
    delay = kDelaySecondsForContentStateSync;

  if (nav_state_sync_timer_.IsRunning()) {
    // The timer is already running. If the delay of the timer maches the amount
    // we want to delay by, then return. Otherwise stop the timer so that it
    // gets started with the right delay.
    if (nav_state_sync_timer_.GetCurrentDelay().InSeconds() == delay)
      return;
    nav_state_sync_timer_.Stop();
  }

  nav_state_sync_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(delay), this,
                              &RenderViewImpl::SyncNavigationState);
}

void RenderViewImpl::setMouseOverURL(const WebURL& url) {
  mouse_over_url_ = GURL(url);
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void RenderViewImpl::setKeyboardFocusURL(const WebURL& url) {
  focus_url_ = GURL(url);
  UpdateTargetURL(focus_url_, mouse_over_url_);
}

void RenderViewImpl::startDragging(WebFrame* frame,
                                   const WebDragData& data,
                                   WebDragOperationsMask mask,
                                   const WebImage& image,
                                   const WebPoint& webImageOffset) {
  DropData drop_data(DropDataBuilder::Build(data));
  drop_data.referrer_policy = frame->document().referrerPolicy();
  gfx::Vector2d imageOffset(webImageOffset.x, webImageOffset.y);
  Send(new DragHostMsg_StartDragging(routing_id_,
                                     drop_data,
                                     mask,
                                     image.getSkBitmap(),
                                     imageOffset,
                                     possible_drag_event_info_));
}

bool RenderViewImpl::acceptsLoadDrops() {
  return renderer_preferences_.can_accept_load_drops;
}

void RenderViewImpl::focusNext() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, false));
}

void RenderViewImpl::focusPrevious() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, true));
}

void RenderViewImpl::focusedNodeChanged(const WebNode& node) {
#if defined(OS_ANDROID)
  advanced_ime_options_ = 0;
  if (webview()
      && (IsEditableNode(node) || IsSelectNode(node)))
    advanced_ime_options_ = webview()->getIMEOptions();
#endif
  long node_id = reinterpret_cast<long>(&node);
  Send(new ViewHostMsg_FocusedNodeChanged(routing_id_, IsEditableNode(node),IsSelectNode(node),
       node_id));

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FocusedNodeChanged(node));
}

void RenderViewImpl::numberOfWheelEventHandlersChanged(unsigned num_handlers) {
  Send(new ViewHostMsg_DidChangeNumWheelEvents(routing_id_, num_handlers));
}

void RenderViewImpl::didUpdateLayout() {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidUpdateLayout());

  // We don't always want to set up a timer, only if we've been put in that
  // mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  if (check_preferred_size_timer_.IsRunning())
    return;
  check_preferred_size_timer_.Start(FROM_HERE,
                                    TimeDelta::FromMilliseconds(0), this,
                                    &RenderViewImpl::CheckPreferredSize);
}

void RenderViewImpl::navigateBackForwardSoon(int offset) {
  Send(new ViewHostMsg_GoToEntryAtOffset(routing_id_, offset));
}

int RenderViewImpl::historyBackListCount() {
  return history_list_offset_ < 0 ? 0 : history_list_offset_;
}

int RenderViewImpl::historyForwardListCount() {
  return history_list_length_ - historyBackListCount() - 1;
}

void RenderViewImpl::postAccessibilityEvent(
    const WebAXObject& obj, blink::WebAXEvent event) {
  if (renderer_accessibility_) {
    renderer_accessibility_->HandleWebAccessibilityEvent(obj, event);
  }
}

void RenderViewImpl::didUpdateInspectorSetting(const WebString& key,
                                           const WebString& value) {
  Send(new ViewHostMsg_UpdateInspectorSetting(routing_id_,
                                              key.utf8(),
                                              value.utf8()));
}

// blink::WebWidgetClient ----------------------------------------------------

void RenderViewImpl::didFocus() {
  // TODO(jcivelli): when https://bugs.webkit.org/show_bug.cgi?id=33389 is fixed
  //                 we won't have to test for user gesture anymore and we can
  //                 move that code back to render_widget.cc
  if (WebUserGestureIndicator::isProcessingUserGesture() &&
      !RenderThreadImpl::current()->layout_test_mode()) {
    Send(new ViewHostMsg_Focus(routing_id_));
  }
}

void RenderViewImpl::didBlur() {
  // TODO(jcivelli): see TODO above in didFocus().
  if (WebUserGestureIndicator::isProcessingUserGesture() &&
      !RenderThreadImpl::current()->layout_test_mode()) {
    Send(new ViewHostMsg_Blur(routing_id_));
  }
}

// We are supposed to get a single call to Show for a newly created RenderView
// that was created via RenderViewImpl::CreateWebView.  So, we wait until this
// point to dispatch the ShowView message.
//
// This method provides us with the information about how to display the newly
// created RenderView (i.e., as a blocked popup or as a new tab).
//
void RenderViewImpl::show(WebNavigationPolicy policy) {
  if (did_show_) {
    // When supports_multiple_windows is disabled, popups are reusing
    // the same view. In some scenarios, this makes WebKit to call show() twice.
    if (webkit_preferences_.supports_multiple_windows)
      NOTREACHED() << "received extraneous Show call";
    return;
  }
  did_show_ = true;

  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  // Force new windows to a popup if they were not opened with a user gesture.
  if (!opened_by_user_gesture_) {
    // We exempt background tabs for compat with older versions of Chrome.
    // TODO(darin): This seems bogus.  These should have a user gesture, so
    // we probably don't need this check.
    if (policy != blink::WebNavigationPolicyNewBackgroundTab)
      policy = blink::WebNavigationPolicyNewPopup;
  }

  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if disposition is not NEW_POPUP, or the
  // browser process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowView(opener_id_, routing_id_,
      NavigationPolicyToDisposition(policy), initial_pos_,
      opened_by_user_gesture_));
  SetPendingWindowRect(initial_pos_);
}

void RenderViewImpl::runModal() {
  DCHECK(did_show_) << "should already have shown the view";

  // Don't allow further dialogs if we are waiting to swap out, since the
  // PageGroupLoadDeferrer in our stack prevents it.
  if (suppress_dialogs_until_swap_out_)
    return;

  // We must keep WebKit's shared timer running in this case in order to allow
  // showModalDialog to function properly.
  //
  // TODO(darin): WebKit should really be smarter about suppressing events and
  // timers so that we do not need to manage the shared timer in such a heavy
  // handed manner.
  //
  if (RenderThreadImpl::current())  // Will be NULL during unit tests.
    RenderThreadImpl::current()->DoNotSuspendWebKitSharedTimer();

  SendAndRunNestedMessageLoop(new ViewHostMsg_RunModal(
      routing_id_, opener_id_));
}

bool RenderViewImpl::enterFullScreen() {
  Send(new ViewHostMsg_ToggleFullscreen(routing_id_, true));
  return true;
}

void RenderViewImpl::exitFullScreen() {
  Send(new ViewHostMsg_ToggleFullscreen(routing_id_, false));
}

bool RenderViewImpl::requestPointerLock() {
  return mouse_lock_dispatcher_->LockMouse(webwidget_mouse_lock_target_.get());
}

void RenderViewImpl::requestPointerUnlock() {
  mouse_lock_dispatcher_->UnlockMouse(webwidget_mouse_lock_target_.get());
}

bool RenderViewImpl::isPointerLocked() {
  return mouse_lock_dispatcher_->IsMouseLockedTo(
      webwidget_mouse_lock_target_.get());
}

void RenderViewImpl::didActivateCompositor(int input_handler_identifier) {
#if !defined(OS_MACOSX)  // many events are unhandled - http://crbug.com/138003
  InputHandlerManager* input_handler_manager =
      RenderThreadImpl::current()->input_handler_manager();
  if (input_handler_manager) {
     input_handler_manager->AddInputHandler(
        routing_id_,
        compositor_->GetInputHandler(),
        AsWeakPtr());
  }
#endif

  RenderWidget::didActivateCompositor(input_handler_identifier);
}

void RenderViewImpl::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
  RenderWidget::didHandleGestureEvent(event, event_cancelled);

  if (event.type != blink::WebGestureEvent::GestureTap)
    return;

  blink::WebTextInputType text_input_type =
      GetWebView()->textInputInfo().type;

  Send(new ViewHostMsg_FocusedNodeTouched(
      routing_id(), text_input_type != blink::WebTextInputTypeNone));
}

void RenderViewImpl::initializeLayerTreeView() {
  RenderWidget::initializeLayerTreeView();
  RenderWidgetCompositor* rwc = compositor();
  if (!rwc || !webview() || !webview()->devToolsAgent())
    return;
#ifdef ENABLE_DEV_TOOLS
  webview()->devToolsAgent()->setLayerTreeId(rwc->GetLayerTreeId());
#endif
}

// blink::WebFrameClient -----------------------------------------------------

WebMediaPlayer* RenderViewImpl::createMediaPlayer(
    WebFrame* frame, const blink::WebURL& url, WebMediaPlayerClient* client) {
  NOTREACHED();
  return NULL;
}

blink::WebMediaPlayer* RenderViewImpl::CreateMediaPlayer(
    RenderFrame* render_frame,
    blink::WebFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, WillCreateMediaPlayer(frame, client));

  WebMediaPlayer* player = CreateWebMediaPlayerForMediaStream(frame, url,
                                                              client);
  if (player)
    return player;

#if defined(OS_ANDROID)
  return CreateAndroidWebMediaPlayer(frame, url, client);
#else
  scoped_refptr<media::AudioRendererSink> sink;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableAudio)) {
    sink = RenderThreadImpl::current()->GetAudioRendererMixerManager()->
        CreateInput(routing_id_, render_frame->GetRoutingID());
    DVLOG(1) << "Using AudioRendererMixerManager-provided sink: " << sink.get();
  }

  WebMediaPlayerParams params(
      base::Bind(&ContentRendererClient::DeferMediaLoad,
                 base::Unretained(GetContentClient()->renderer()),
                 static_cast<RenderFrame*>(render_frame)),
      sink);
  return new WebMediaPlayerImpl(frame, client, AsWeakPtr(), params);
#endif  // defined(OS_ANDROID)
}

void RenderViewImpl::didAccessInitialDocument(WebFrame* frame) {
  // Notify the browser process that it is no longer safe to show the pending
  // URL of the main frame, since a URL spoof is now possible.
  if (!frame->parent() && page_id_ == -1)
    Send(new ViewHostMsg_DidAccessInitialDocument(routing_id_));
}

void RenderViewImpl::didDisownOpener(blink::WebFrame* frame) {
  // We only need to notify the browser if the active, top-level frame clears
  // its opener.  We can ignore cases where a swapped out frame clears its
  // opener after hearing about it from the browser, and the browser does not
  // (yet) care about subframe openers.
  if (is_swapped_out_ || frame->parent())
    return;

  // Notify WebContents and all its swapped out RenderViews.
  Send(new ViewHostMsg_DidDisownOpener(routing_id_));
}

void RenderViewImpl::frameDetached(WebFrame* frame) {
  // NOTE: We may get here for either the main frame or for subframes.  The
  // RenderFrameImpl will be deleted immediately after this call for subframes
  // but not for the main frame, which is owned by |main_render_frame_|.

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FrameDetached(frame));
}

void RenderViewImpl::willClose(WebFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FrameWillClose(frame));
}

void RenderViewImpl::didMatchCSS(
    WebFrame* frame,
    const WebVector<WebString>& newly_matching_selectors,
    const WebVector<WebString>& stopped_matching_selectors) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_,
      DidMatchCSS(frame, newly_matching_selectors, stopped_matching_selectors));
}

void RenderViewImpl::Repaint(const gfx::Size& size) {
  OnRepaint(size);
}

void RenderViewImpl::SetEditCommandForNextKeyEvent(const std::string& name,
                                                   const std::string& value) {
  EditCommands edit_commands;
  edit_commands.push_back(EditCommand(name, value));
  OnSetEditCommandsForNextKeyEvent(edit_commands);
}

void RenderViewImpl::ClearEditCommands() {
  edit_commands_.clear();
}

SSLStatus RenderViewImpl::GetSSLStatusOfFrame(blink::WebFrame* frame) const {
  std::string security_info;
  if (frame && frame->dataSource())
    security_info = frame->dataSource()->response().securityInfo();

  SSLStatus ssl_status;
  DeserializeSecurityInfo(security_info,
                          &ssl_status.cert_id,
                          &ssl_status.cert_status,
                          &ssl_status.security_bits,
                          &ssl_status.connection_status,
                          &ssl_status.signed_certificate_timestamp_ids);
  return ssl_status;
}

const std::string& RenderViewImpl::GetAcceptLanguages() const {
  return renderer_preferences_.accept_languages;
}

WebNavigationPolicy RenderViewImpl::DecidePolicyForNavigation(
    RenderFrame* render_frame, WebFrame* frame,
    WebDataSource::ExtraData* extraData, const WebURLRequest& request,
    WebNavigationType type, WebNavigationPolicy default_policy,
    bool is_redirect) {
#ifdef OS_ANDROID
  // The handlenavigation API is deprecated and will be removed once
  // crbug.com/325351 is resolved.
  if (request.url() != GURL(kSwappedOutURL) &&
      GetContentClient()->renderer()->HandleNavigation(
          render_frame,
          static_cast<DocumentState*>(extraData),
          opener_id_,
          frame,
          request,
          type,
          default_policy,
          is_redirect)) {
    return blink::WebNavigationPolicyIgnore;
  }
#endif

  Referrer referrer(GetReferrerFromRequest(frame, request));

  if (is_swapped_out_) {
    if (request.url() != GURL(kSwappedOutURL)) {
      // Targeted links may try to navigate a swapped out frame.  Allow the
      // browser process to navigate the tab instead.  Note that it is also
      // possible for non-targeted navigations (from this view) to arrive
      // here just after we are swapped out.  It's ok to send them to the
      // browser, as long as they're for the top level frame.
      // TODO(creis): Ensure this supports targeted form submissions when
      // fixing http://crbug.com/101395.
      if (frame->parent() == NULL) {
        OpenURL(frame, request.url(), referrer, default_policy);
        return blink::WebNavigationPolicyIgnore;  // Suppress the load here.
      }

      // We should otherwise ignore in-process iframe navigations, if they
      // arrive just after we are swapped out.
      return blink::WebNavigationPolicyIgnore;
    }

    // Allow kSwappedOutURL to complete.
    return default_policy;
  }

  // Webkit is asking whether to navigate to a new URL.
  // This is fine normally, except if we're showing UI from one security
  // context and they're trying to navigate to a different context.
  const GURL& url = request.url();

  // A content initiated navigation may have originated from a link-click,
  // script, drag-n-drop operation, etc.
  bool is_content_initiated = static_cast<DocumentState*>(extraData)->
          navigation_state()->is_content_initiated();

  // Experimental:
  // If --enable-strict-site-isolation or --site-per-process is enabled, send
  // all top-level navigations to the browser to let it swap processes when
  // crossing site boundaries.  This is currently expected to break some script
  // calls and navigations, such as form submissions.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool force_swap_due_to_flag =
      command_line.HasSwitch(switches::kEnableStrictSiteIsolation) ||
      command_line.HasSwitch(switches::kSitePerProcess);
  if (force_swap_due_to_flag &&
      !frame->parent() && (is_content_initiated || is_redirect)) {
    WebString origin_str = frame->document().securityOrigin().toString();
    GURL frame_url(origin_str.utf8().data());
    // TODO(cevans): revisit whether this site check is still necessary once
    // crbug.com/101395 is fixed.
    bool same_domain_or_host =
        net::registry_controlled_domains::SameDomainOrHost(
            frame_url,
            url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (!same_domain_or_host || frame_url.scheme() != url.scheme()) {
      OpenURL(frame, url, referrer, default_policy);
      return blink::WebNavigationPolicyIgnore;
    }
  }

  // If the browser is interested, then give it a chance to look at the request.
  if (is_content_initiated) {
    bool is_form_post = ((type == blink::WebNavigationTypeFormSubmitted) ||
                         (type == blink::WebNavigationTypeFormResubmitted)) &&
                        EqualsASCII(request.httpMethod(), "POST");
    bool browser_handles_request =
        renderer_preferences_.browser_handles_non_local_top_level_requests &&
        IsNonLocalTopLevelNavigation(url, frame, type, is_form_post);
    if (!browser_handles_request) {
      browser_handles_request = IsTopLevelNavigation(frame) &&
          renderer_preferences_.browser_handles_all_top_level_requests;
    }

    if (browser_handles_request) {
      // Reset these counters as the RenderView could be reused for the next
      // navigation.
      page_id_ = -1;
      last_page_id_sent_to_browser_ = -1;
      OpenURL(frame, url, referrer, default_policy);
      return blink::WebNavigationPolicyIgnore;  // Suppress the load here.
    }
  }

  // Use the frame's original request's URL rather than the document's URL for
  // subsequent checks.  For a popup, the document's URL may become the opener
  // window's URL if the opener has called document.write().
  // See http://crbug.com/93517.
  GURL old_url(frame->dataSource()->request().url());

  // Detect when we're crossing a permission-based boundary (e.g. into or out of
  // an extension or app origin, leaving a WebUI page, etc). We only care about
  // top-level navigations (not iframes). But we sometimes navigate to
  // about:blank to clear a tab, and we want to still allow that.
  //
  // Note: this is known to break POST submissions when crossing process
  // boundaries until http://crbug.com/101395 is fixed.  This is better for
  // security than loading a WebUI, extension or app page in the wrong process.
  // POST requests don't work because this mechanism does not preserve form
  // POST data. We will need to send the request's httpBody data up to the
  // browser process, and issue a special POST navigation in WebKit (via
  // FrameLoader::loadFrameRequest). See ResourceDispatcher and WebURLLoaderImpl
  // for examples of how to send the httpBody data.
  if (!frame->parent() && is_content_initiated &&
      !url.SchemeIs(chrome::kAboutScheme)) {
    bool send_referrer = false;

    // All navigations to or from WebUI URLs or within WebUI-enabled
    // RenderProcesses must be handled by the browser process so that the
    // correct bindings and data sources can be registered.
    // Similarly, navigations to view-source URLs or within ViewSource mode
    // must be handled by the browser process (except for reloads - those are
    // safe to leave within the renderer).
    // Lastly, access to file:// URLs from non-file:// URL pages must be
    // handled by the browser so that ordinary renderer processes don't get
    // blessed with file permissions.
    int cumulative_bindings = RenderProcess::current()->GetEnabledBindings();
    bool is_initial_navigation = page_id_ == -1;
    bool should_fork = HasWebUIScheme(url) || HasWebUIScheme(old_url) ||
        (cumulative_bindings & BINDINGS_POLICY_WEB_UI) ||
        url.SchemeIs(kViewSourceScheme) ||
        (frame->isViewSourceModeEnabled() &&
            type != blink::WebNavigationTypeReload);

    if (!should_fork && url.SchemeIs(kFileScheme)) {
      // Fork non-file to file opens.  Check the opener URL if this is the
      // initial navigation in a newly opened window.
      GURL source_url(old_url);
      if (is_initial_navigation && source_url.is_empty() && frame->opener())
        source_url = frame->opener()->top()->document().url();
      DCHECK(!source_url.is_empty());
      should_fork = !source_url.SchemeIs(kFileScheme);
    }

    if (!should_fork) {
      // Give the embedder a chance.
      should_fork = GetContentClient()->renderer()->ShouldFork(
          frame, url, request.httpMethod().utf8(), is_initial_navigation,
          is_redirect, &send_referrer);
    }

    if (should_fork) {
      OpenURL(
          frame, url, send_referrer ? referrer : Referrer(), default_policy);
      return blink::WebNavigationPolicyIgnore;  // Suppress the load here.
    }
  }

  // Detect when a page is "forking" a new tab that can be safely rendered in
  // its own process.  This is done by sites like Gmail that try to open links
  // in new windows without script connections back to the original page.  We
  // treat such cases as browser navigations (in which we will create a new
  // renderer for a cross-site navigation), rather than WebKit navigations.
  //
  // We use the following heuristic to decide whether to fork a new page in its
  // own process:
  // The parent page must open a new tab to about:blank, set the new tab's
  // window.opener to null, and then redirect the tab to a cross-site URL using
  // JavaScript.
  //
  // TODO(creis): Deprecate this logic once we can rely on rel=noreferrer
  // (see below).
  bool is_fork =
      // Must start from a tab showing about:blank, which is later redirected.
      old_url == GURL(kAboutBlankURL) &&
      // Must be the first real navigation of the tab.
      historyBackListCount() < 1 &&
      historyForwardListCount() < 1 &&
      // The parent page must have set the child's window.opener to null before
      // redirecting to the desired URL.
      frame->opener() == NULL &&
      // Must be a top-level frame.
      frame->parent() == NULL &&
      // Must not have issued the request from this page.
      is_content_initiated &&
      // Must be targeted at the current tab.
      default_policy == blink::WebNavigationPolicyCurrentTab &&
      // Must be a JavaScript navigation, which appears as "other".
      type == blink::WebNavigationTypeOther;

  if (is_fork) {
    // Open the URL via the browser, not via WebKit.
    OpenURL(frame, url, Referrer(), default_policy);
    return blink::WebNavigationPolicyIgnore;
  }

  return default_policy;
}

void RenderViewImpl::willSendSubmitEvent(blink::WebFrame* frame,
    const blink::WebFormElement& form) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, WillSendSubmitEvent(frame, form));
}

void RenderViewImpl::willSubmitForm(WebFrame* frame,
                                    const WebFormElement& form) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, WillSubmitForm(frame, form));
}

void RenderViewImpl::didCreateDataSource(WebFrame* frame, WebDataSource* ds) {
  bool content_initiated = !pending_navigation_params_.get();

  // Make sure any previous redirect URLs end up in our new data source.
  if (pending_navigation_params_.get()) {
    for (std::vector<GURL>::const_iterator i =
             pending_navigation_params_->redirects.begin();
         i != pending_navigation_params_->redirects.end(); ++i) {
      ds->appendRedirect(*i);
    }
  }

  DocumentState* document_state = DocumentState::FromDataSource(ds);
  if (!document_state) {
    document_state = new DocumentState;
    ds->setExtraData(document_state);
    if (!content_initiated)
      PopulateDocumentStateFromPending(document_state);
  }

  // Carry over the user agent override flag, if it exists.
  if (content_initiated && webview() && webview()->mainFrame() &&
      webview()->mainFrame()->dataSource()) {
    DocumentState* old_document_state =
        DocumentState::FromDataSource(webview()->mainFrame()->dataSource());
    if (old_document_state) {
      InternalDocumentStateData* internal_data =
          InternalDocumentStateData::FromDocumentState(document_state);
      InternalDocumentStateData* old_internal_data =
          InternalDocumentStateData::FromDocumentState(old_document_state);
      internal_data->set_is_overriding_user_agent(
          old_internal_data->is_overriding_user_agent());
    }
  }

  // The rest of RenderView assumes that a WebDataSource will always have a
  // non-null NavigationState.
  if (content_initiated) {
    document_state->set_navigation_state(
        NavigationState::CreateContentInitiated());
  } else {
    document_state->set_navigation_state(CreateNavigationStateFromPending());
    pending_navigation_params_.reset();
  }

  // DocumentState::referred_by_prefetcher_ is true if we are
  // navigating from a page that used prefetching using a link on that
  // page.  We are early enough in the request process here that we
  // can still see the DocumentState of the previous page and set
  // this value appropriately.
  // TODO(gavinp): catch the important case of navigation in a new
  // renderer process.
  if (webview()) {
    if (WebFrame* old_frame = webview()->mainFrame()) {
      const WebURLRequest& original_request = ds->originalRequest();
      const GURL referrer(
          original_request.httpHeaderField(WebString::fromUTF8("Referer")));
      if (!referrer.is_empty() &&
          DocumentState::FromDataSource(
              old_frame->dataSource())->was_prefetcher()) {
        for (; old_frame; old_frame = old_frame->traverseNext(false)) {
          WebDataSource* old_frame_ds = old_frame->dataSource();
          if (old_frame_ds && referrer == GURL(old_frame_ds->request().url())) {
            document_state->set_was_referred_by_prefetcher(true);
            break;
          }
        }
      }
    }
  }

  if (content_initiated) {
    const WebURLRequest& request = ds->request();
    switch (request.cachePolicy()) {
      case WebURLRequest::UseProtocolCachePolicy:  // normal load.
        document_state->set_load_type(DocumentState::LINK_LOAD_NORMAL);
        break;
      case WebURLRequest::ReloadIgnoringCacheData:  // reload.
        document_state->set_load_type(DocumentState::LINK_LOAD_RELOAD);
        break;
      case WebURLRequest::ReturnCacheDataElseLoad:  // allow stale data.
        document_state->set_load_type(
            DocumentState::LINK_LOAD_CACHE_STALE_OK);
        break;
      case WebURLRequest::ReturnCacheDataDontLoad:  // Don't re-post.
        document_state->set_load_type(DocumentState::LINK_LOAD_CACHE_ONLY);
        break;
    }
  }

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidCreateDataSource(frame, ds));
}

void RenderViewImpl::PopulateDocumentStateFromPending(
    DocumentState* document_state) {
  const FrameMsg_Navigate_Params& params = *pending_navigation_params_.get();
  document_state->set_request_time(params.request_time);

  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);

  if (!params.url.SchemeIs(kJavaScriptScheme) &&
      params.navigation_type == FrameMsg_Navigate_Type::RESTORE) {
    // We're doing a load of a page that was restored from the last session. By
    // default this prefers the cache over loading (LOAD_PREFERRING_CACHE) which
    // can result in stale data for pages that are set to expire. We explicitly
    // override that by setting the policy here so that as necessary we load
    // from the network.
    //
    // TODO(davidben): Remove this in favor of passing a cache policy to the
    // loadHistoryItem call in OnNavigate. That requires not overloading
    // UseProtocolCachePolicy to mean both "normal load" and "determine cache
    // policy based on load type, etc".
    internal_data->set_cache_policy_override(
        WebURLRequest::UseProtocolCachePolicy);
  }

  if (IsReload(params))
    document_state->set_load_type(DocumentState::RELOAD);
  else if (params.page_state.IsValid())
    document_state->set_load_type(DocumentState::HISTORY_LOAD);
  else
    document_state->set_load_type(DocumentState::NORMAL_LOAD);

  internal_data->set_is_overriding_user_agent(params.is_overriding_user_agent);
  internal_data->set_must_reset_scroll_and_scale_state(
      params.navigation_type ==
          FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL);
  document_state->set_can_load_local_resources(params.can_load_local_resources);
}

NavigationState* RenderViewImpl::CreateNavigationStateFromPending() {
  const FrameMsg_Navigate_Params& params = *pending_navigation_params_.get();
  NavigationState* navigation_state = NULL;

  // A navigation resulting from loading a javascript URL should not be treated
  // as a browser initiated event.  Instead, we want it to look as if the page
  // initiated any load resulting from JS execution.
  if (!params.url.SchemeIs(kJavaScriptScheme)) {
    navigation_state = NavigationState::CreateBrowserInitiated(
        params.page_id,
        params.pending_history_list_offset,
        params.should_clear_history_list,
        params.transition);
    navigation_state->set_should_replace_current_entry(
        params.should_replace_current_entry);
    navigation_state->set_transferred_request_child_id(
        params.transferred_request_child_id);
    navigation_state->set_transferred_request_request_id(
        params.transferred_request_request_id);
    navigation_state->set_allow_download(params.allow_download);
    navigation_state->set_extra_headers(params.extra_headers);
  } else {
    navigation_state = NavigationState::CreateContentInitiated();
  }
  return navigation_state;
}

void RenderViewImpl::ProcessViewLayoutFlags(const CommandLine& command_line) {
  bool enable_viewport =
      command_line.HasSwitch(switches::kEnableViewport) ||
      command_line.HasSwitch(switches::kEnableViewportMeta);

  // If viewport tag is enabled, then the WebKit side will take care
  // of setting the fixed layout size and page scale limits.
  if (enable_viewport)
    return;

  // When navigating to a new page, reset the page scale factor to be 1.0.
  webview()->setInitialPageScaleOverride(1.f);

  float maxPageScaleFactor =
      command_line.HasSwitch(switches::kEnablePinch) ? 4.f : 1.f ;
  webview()->setPageScaleFactorLimits(1, maxPageScaleFactor);
}

// TODO(nasko): Remove this method once WebTestProxy in Blink is fixed.
void RenderViewImpl::didStartProvisionalLoad(WebFrame* frame) {
}

void RenderViewImpl::didFailProvisionalLoad(WebFrame* frame,
                                            const WebURLError& error) {
  // Notify the browser that we failed a provisional load with an error.
  //
  // Note: It is important this notification occur before DidStopLoading so the
  //       SSL manager can react to the provisional load failure before being
  //       notified the load stopped.
  //
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidFailProvisionalLoad(frame, error));
}

void RenderViewImpl::didCommitProvisionalLoad(WebFrame* frame,
                                              bool is_new_navigation) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidCommitProvisionalLoad(frame, is_new_navigation));

  // TODO(nasko): Transition this code to RenderFrameImpl, since g_view_map is
  // not accessible from there.
  if (!frame->parent()) {  // Only for top frames.
    RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
    if (render_thread_impl) {  // Can be NULL in tests.
      render_thread_impl->histogram_customizer()->
          RenderViewNavigatedToHost(GURL(GetLoadingUrl(frame)).host(),
                                    g_view_map.Get().size());
    }
  }
}

void RenderViewImpl::didClearWindowObject(WebFrame* frame, int world_id) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidClearWindowObject(frame, world_id));

  // Only install controllers into the main world.
  if (world_id)
    return;

  if (enabled_bindings_& BINDINGS_POLICY_WEB_UI)
    WebUIExtension::Install(frame);

  if (enabled_bindings_ & BINDINGS_POLICY_DOM_AUTOMATION)
    DomAutomationController::Install(this, frame);

  if (enabled_bindings_ & BINDINGS_POLICY_STATS_COLLECTION)
    StatsCollectionController::Install(frame);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kEnableSkiaBenchmarking))
    SkiaBenchmarking::Install(frame);

  if (command_line.HasSwitch(switches::kEnableMemoryBenchmarking))
    MemoryBenchmarkingExtension::Install(frame);
}

void RenderViewImpl::didCreateDocumentElement(WebFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidCreateDocumentElement(frame));
}

void RenderViewImpl::didReceiveTitle(WebFrame* frame, const WebString& title,
                                     WebTextDirection direction) {
  UpdateTitle(frame, title, direction);

  // Also check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderViewImpl::didChangeIcon(WebFrame* frame,
                                   WebIconURL::Type icon_type) {
  if (frame->parent())
    return;

  if (!TouchEnabled() && icon_type != WebIconURL::TypeFavicon)
    return;

  WebVector<WebIconURL> icon_urls = frame->iconURLs(icon_type);
  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    urls.push_back(FaviconURL(icon_urls[i].iconURL(),
                              ToFaviconType(icon_urls[i].iconType())));
  }
  SendUpdateFaviconURL(urls);
}

void RenderViewImpl::didFinishDocumentLoad(WebFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidFinishDocumentLoad(frame));
}

void RenderViewImpl::didHandleOnloadEvents(WebFrame* frame) {
  if (webview()->mainFrame() == frame) {
    Send(new ViewHostMsg_DocumentOnLoadCompletedInMainFrame(routing_id_,
                                                            page_id_));
  }
}

void RenderViewImpl::didFailLoad(WebFrame* frame, const WebURLError& error) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidFailLoad(frame, error));
}

void RenderViewImpl::didFinishLoad(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  DocumentState* document_state = DocumentState::FromDataSource(ds);
  if (document_state->finish_load_time().is_null()) {
    if (!frame->parent()) {
      TRACE_EVENT_INSTANT0("WebCore", "LoadFinished",
                           TRACE_EVENT_SCOPE_PROCESS);
    }
    document_state->set_finish_load_time(Time::Now());
  }

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidFinishLoad(frame));

  // Don't send this message while the subframe is swapped out.
  // TODO(creis): This whole method should move to RenderFrame.
  RenderFrameImpl* rf = RenderFrameImpl::FromWebFrame(frame);
  if (rf && rf->is_swapped_out())
    return;

  Send(new ViewHostMsg_DidFinishLoad(routing_id_,
                                     frame->identifier(),
                                     ds->request().url(),
                                     !frame->parent()));
}

void RenderViewImpl::didNavigateWithinPage(
    WebFrame* frame,
    bool is_new_navigation) {
  // TODO(nasko): Forward calls to the main RenderFrameImpl until all
  // callers of this method on RenderView are removed.
  main_render_frame_->didNavigateWithinPage(frame, is_new_navigation);
}

void RenderViewImpl::didUpdateCurrentHistoryItem(WebFrame* frame) {
  StartNavStateSyncTimerIfNecessary();
}

void RenderViewImpl::willSendRequest(WebFrame* frame,
                                     unsigned identifier,
                                     WebURLRequest& request,
                                     const WebURLResponse& redirect_response) {
  NOTREACHED();
}

void RenderViewImpl::didReceiveResponse(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {
  NOTREACHED();
}

void RenderViewImpl::didFinishResourceLoad(
    WebFrame* frame, unsigned identifier) {
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDataSource(frame->dataSource());
  if (!internal_data->use_error_page())
    return;

#ifdef ENABLE_DEV_TOOLS
  // Do not show error page when DevTools is attached.
  if (devtools_agent_->IsAttached())
    return;
#endif

  // Display error page, if appropriate.
  std::string error_domain = "http";
  int http_status_code = internal_data->http_status_code();
  if (GetContentClient()->renderer()->HasErrorPage(
          http_status_code, &error_domain)) {
    WebURLError error;
    error.unreachableURL = frame->document().url();
    error.domain = WebString::fromUTF8(error_domain);
    error.reason = http_status_code;
    LoadNavigationErrorPage(frame, frame->dataSource()->request(), error, true);
  }
}

void RenderViewImpl::didLoadResourceFromMemoryCache(
    WebFrame* frame, const WebURLRequest& request,
    const WebURLResponse& response) {
  NOTREACHED();
}

void RenderViewImpl::didDisplayInsecureContent(WebFrame* frame) {
  NOTREACHED();
}

void RenderViewImpl::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin, const WebURL& target) {
  NOTREACHED();
}

void RenderViewImpl::didExhaustMemoryAvailableForScript(WebFrame* frame) {
  NOTREACHED();
}

void RenderViewImpl::didCreateScriptContext(WebFrame* frame,
                                            v8::Handle<v8::Context> context,
                                            int extension_group,
                                            int world_id) {
  NOTREACHED();
}

void RenderViewImpl::willReleaseScriptContext(WebFrame* frame,
                                              v8::Handle<v8::Context> context,
                                              int world_id) {
  NOTREACHED();
}

void RenderViewImpl::CheckPreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  gfx::Size size = webview()->contentsPreferredMinimumSize();

  // In the presence of zoom, these sizes are still reported as if unzoomed,
  // so we need to adjust.
  double zoom_factor = ZoomLevelToZoomFactor(webview()->zoomLevel());
  size.set_width(static_cast<int>(size.width() * zoom_factor));
  size.set_height(static_cast<int>(size.height() * zoom_factor));

  if (size == preferred_size_)
    return;

  preferred_size_ = size;
  Send(new ViewHostMsg_DidContentsPreferredSizeChange(routing_id_,
                                                      preferred_size_));
}

BrowserPluginManager* RenderViewImpl::GetBrowserPluginManager() {
  if (!browser_plugin_manager_.get())
    browser_plugin_manager_ = BrowserPluginManager::Create(this);
  return browser_plugin_manager_.get();
}

bool RenderViewImpl::InitializeMediaStreamClient() {
  if (media_stream_client_)
    return true;

  if (!RenderThreadImpl::current())  // Will be NULL during unit tests.
    return false;

#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableWebRTC))
    return false;
#endif

#if defined(ENABLE_WEBRTC)
  if (!media_stream_dispatcher_)
    media_stream_dispatcher_ = new MediaStreamDispatcher(this);

  MediaStreamImpl* media_stream_impl = new MediaStreamImpl(
      this,
      media_stream_dispatcher_,
      RenderThreadImpl::current()->GetMediaStreamDependencyFactory());
  media_stream_client_ = media_stream_impl;
  web_user_media_client_ = media_stream_impl;
  return true;
#else
  return false;
#endif
}

void RenderViewImpl::didChangeContentsSize(WebFrame* frame,
                                           const WebSize& size) {
  if (webview()->mainFrame() != frame)
    return;
  WebView* frameView = frame->view();
  if (!frameView)
    return;

  bool has_horizontal_scrollbar = frame->hasHorizontalScrollbar();
  bool has_vertical_scrollbar = frame->hasVerticalScrollbar();

  if (has_horizontal_scrollbar != cached_has_main_frame_horizontal_scrollbar_ ||
      has_vertical_scrollbar != cached_has_main_frame_vertical_scrollbar_) {
    Send(new ViewHostMsg_DidChangeScrollbarsForMainFrame(
          routing_id_, has_horizontal_scrollbar, has_vertical_scrollbar));

    cached_has_main_frame_horizontal_scrollbar_ = has_horizontal_scrollbar;
    cached_has_main_frame_vertical_scrollbar_ = has_vertical_scrollbar;
  }
}

void RenderViewImpl::UpdateScrollState(WebFrame* frame) {
  WebSize offset = frame->scrollOffset();
  WebSize minimum_offset = frame->minimumScrollOffset();
  WebSize maximum_offset = frame->maximumScrollOffset();

  bool is_pinned_to_left = offset.width <= minimum_offset.width;
  bool is_pinned_to_right = offset.width >= maximum_offset.width;

  if (is_pinned_to_left != cached_is_main_frame_pinned_to_left_ ||
      is_pinned_to_right != cached_is_main_frame_pinned_to_right_) {
    Send(new ViewHostMsg_DidChangeScrollOffsetPinningForMainFrame(
          routing_id_, is_pinned_to_left, is_pinned_to_right));

    cached_is_main_frame_pinned_to_left_ = is_pinned_to_left;
    cached_is_main_frame_pinned_to_right_ = is_pinned_to_right;
  }

  Send(new ViewHostMsg_DidChangeScrollOffset(routing_id_));
}

void RenderViewImpl::didChangeScrollOffset(WebFrame* frame) {
  StartNavStateSyncTimerIfNecessary();

  if (webview()->mainFrame() == frame)
    UpdateScrollState(frame);

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidChangeScrollOffset(frame));
}

void RenderViewImpl::willInsertBody(blink::WebFrame* frame) {
  NOTREACHED();
}

void RenderViewImpl::didFirstVisuallyNonEmptyLayout(WebFrame* frame) {
  if (frame != webview()->mainFrame())
    return;

  InternalDocumentStateData* data =
      InternalDocumentStateData::FromDataSource(frame->dataSource());
  data->set_did_first_visually_non_empty_layout(true);

#if defined(OS_ANDROID)
  // Update body background color if necessary.
  SkColor bg_color = webwidget_->backgroundColor();

  // If not initialized, default to white. Note that 0 is different from black
  // as black still has alpha 0xFF.
  if (!bg_color)
    bg_color = SK_ColorWHITE;

  if (bg_color != body_background_color_) {
    body_background_color_ = bg_color;
    Send(new ViewHostMsg_DidChangeBodyBackgroundColor(
        GetRoutingID(), bg_color));
  }
#endif
}

void RenderViewImpl::SendFindReply(int request_id,
                                   int match_count,
                                   int ordinal,
                                   const WebRect& selection_rect,
                                   bool final_status_update) {
  Send(new ViewHostMsg_Find_Reply(routing_id_,
                                  request_id,
                                  match_count,
                                  selection_rect,
                                  ordinal,
                                  final_status_update));
}

void RenderViewImpl::reportFindInPageMatchCount(int request_id,
                                                int count,
                                                bool final_update) {
  NOTREACHED();
}

void RenderViewImpl::reportFindInPageSelection(int request_id,
                                               int active_match_ordinal,
                                               const WebRect& selection_rect) {
  NOTREACHED();
}

void RenderViewImpl::requestStorageQuota(
    WebFrame* frame,
    WebStorageQuotaType type,
    unsigned long long requested_size,
    blink::WebStorageQuotaCallbacks callbacks) {
  NOTREACHED();
}

bool RenderViewImpl::willCheckAndDispatchMessageEvent(
    blink::WebFrame* sourceFrame,
    blink::WebFrame* targetFrame,
    blink::WebSecurityOrigin target_origin,
    blink::WebDOMMessageEvent event) {
  if (!is_swapped_out_)
    return false;

  ViewMsg_PostMessage_Params params;
  params.data = event.data().toString();
  params.source_origin = event.origin();
  if (!target_origin.isNull())
    params.target_origin = target_origin.toString();

  blink::WebMessagePortChannelArray channels = event.releaseChannels();
  if (!channels.isEmpty()) {
    std::vector<int> message_port_ids(channels.size());
     // Extract the port IDs from the channel array.
     for (size_t i = 0; i < channels.size(); ++i) {
       WebMessagePortChannelImpl* webchannel =
           static_cast<WebMessagePortChannelImpl*>(channels[i]);
       message_port_ids[i] = webchannel->message_port_id();
       webchannel->QueueMessages();
       DCHECK_NE(message_port_ids[i], MSG_ROUTING_NONE);
     }
     params.message_port_ids = message_port_ids;
  }

  // Include the routing ID for the source frame (if one exists), which the
  // browser process will translate into the routing ID for the equivalent
  // frame in the target process.
  params.source_routing_id = MSG_ROUTING_NONE;
  if (sourceFrame) {
    RenderViewImpl* source_view = FromWebView(sourceFrame->view());
    if (source_view)
      params.source_routing_id = source_view->routing_id();
  }

  Send(new ViewHostMsg_RouteMessageEvent(routing_id_, params));
  return true;
}

void RenderViewImpl::willOpenSocketStream(
    WebSocketStreamHandle* handle) {
  NOTREACHED();
}

void RenderViewImpl::willStartUsingPeerConnectionHandler(
    blink::WebFrame* frame, blink::WebRTCPeerConnectionHandler* handler) {
  NOTREACHED();
}

blink::WebString RenderViewImpl::acceptLanguages() {
  return WebString::fromUTF8(renderer_preferences_.accept_languages);
}

blink::WebString RenderViewImpl::userAgentOverride(
    blink::WebFrame* frame,
    const blink::WebURL& url) {
  NOTREACHED();
  return blink::WebString();
}

WebString RenderViewImpl::doNotTrackValue(WebFrame* frame) {
  NOTREACHED();
  return blink::WebString();
}

bool RenderViewImpl::allowWebGL(WebFrame* frame, bool default_value) {
  NOTREACHED();
  return false;
}

void RenderViewImpl::didLoseWebGLContext(
    blink::WebFrame* frame,
    int arb_robustness_status_code) {
  NOTREACHED();
}

// blink::WebPageSerializerClient implementation ------------------------------

void RenderViewImpl::didSerializeDataForFrame(
    const WebURL& frame_url,
    const WebCString& data,
    WebPageSerializerClient::PageSerializationStatus status) {
  Send(new ViewHostMsg_SendSerializedHtmlData(
    routing_id(),
    frame_url,
    data.data(),
    static_cast<int32>(status)));
}

// RenderView implementation ---------------------------------------------------

bool RenderViewImpl::Send(IPC::Message* message) {
  return RenderWidget::Send(message);
}

RenderFrame* RenderViewImpl::GetMainRenderFrame() {
  return main_render_frame_.get();
}

int RenderViewImpl::GetRoutingID() const {
  return routing_id_;
}

int RenderViewImpl::GetPageId() const {
  return page_id_;
}

gfx::Size RenderViewImpl::GetSize() const {
  return size();
}

WebPreferences& RenderViewImpl::GetWebkitPreferences() {
  return webkit_preferences_;
}

void RenderViewImpl::SetWebkitPreferences(const WebPreferences& preferences) {
  OnUpdateWebPreferences(preferences);
}

blink::WebView* RenderViewImpl::GetWebView() {
  return webview();
}

blink::WebNode RenderViewImpl::GetFocusedNode() const {
  if (!webview())
    return WebNode();
  WebFrame* focused_frame = webview()->focusedFrame();
  if (focused_frame) {
    WebDocument doc = focused_frame->document();
    if (!doc.isNull())
      return doc.focusedNode();
  }

  return WebNode();
}

blink::WebNode RenderViewImpl::GetContextMenuNode() const {
  return context_menu_node_;
}

bool RenderViewImpl::IsEditableNode(const WebNode& node) const {
  if (node.isNull())
    return false;

  if (node.isContentEditable())
    return true;

  if (node.isElementNode()) {
    const WebElement& element = node.toConst<WebElement>();
    if (element.isTextFormControlElement())
      return true;

    // Also return true if it has an ARIA role of 'textbox'.
    for (unsigned i = 0; i < element.attributeCount(); ++i) {
      if (LowerCaseEqualsASCII(element.attributeLocalName(i), "role")) {
        if (LowerCaseEqualsASCII(element.attributeValue(i), "textbox"))
          return true;
        break;
      }
    }
  }

  return false;
}

bool RenderViewImpl::IsSelectNode(const WebNode& node) const {
  if (node.isNull())
    return false;

  if (node.isElementNode()) {
    if (node.toConst<WebElement>().hasTagName("select"))
      return true;
  }

  return false;
}

void RenderViewImpl::EvaluateScript(const base::string16& frame_xpath,
                                    const base::string16& jscript,
                                    int id,
                                    bool notify_result) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> result;
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (web_frame)
    result = web_frame->executeScriptAndReturnValue(WebScriptSource(jscript));
  if (notify_result) {
    base::ListValue list;
    if (!result.IsEmpty() && web_frame) {
      v8::Local<v8::Context> context = web_frame->mainWorldScriptContext();
      v8::Context::Scope context_scope(context);
      V8ValueConverterImpl converter;
      converter.SetDateAllowed(true);
      converter.SetRegExpAllowed(true);
      base::Value* result_value = converter.FromV8Value(result, context);
      list.Set(0, result_value ? result_value : base::Value::CreateNullValue());
    } else {
      list.Set(0, base::Value::CreateNullValue());
    }
    Send(new ViewHostMsg_ScriptEvalResponse(routing_id_, id, list));
  }
}

bool RenderViewImpl::ShouldDisplayScrollbars(int width, int height) const {
  return (!send_preferred_size_changes_ ||
          (disable_scrollbars_size_limit_.width() <= width ||
           disable_scrollbars_size_limit_.height() <= height));
}

int RenderViewImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

bool RenderViewImpl::GetContentStateImmediately() const {
  return send_content_state_immediately_;
}

float RenderViewImpl::GetFilteredTimePerFrame() const {
  return filtered_time_per_frame();
}

blink::WebPageVisibilityState RenderViewImpl::GetVisibilityState() const {
  return visibilityState();
}

void RenderViewImpl::RunModalAlertDialog(blink::WebFrame* frame,
                                         const blink::WebString& message) {
  return runModalAlertDialog(frame, message);
}

void RenderViewImpl::DidStartLoading() {
  didStartLoading();
}

void RenderViewImpl::DidStopLoading() {
  didStopLoading();
}

void RenderViewImpl::DidPlay(blink::WebMediaPlayer* player) {
  Send(new ViewHostMsg_MediaPlayingNotification(routing_id_,
                                                reinterpret_cast<int64>(player),
                                                player->hasVideo(),
                                                player->hasAudio()));
}

void RenderViewImpl::DidPause(blink::WebMediaPlayer* player) {
  Send(new ViewHostMsg_MediaPausedNotification(
      routing_id_, reinterpret_cast<int64>(player)));
}

void RenderViewImpl::PlayerGone(blink::WebMediaPlayer* player) {
  DidPause(player);
}

void RenderViewImpl::SyncNavigationState() {
  if (!webview())
    return;

  const WebHistoryItem& item = webview()->mainFrame()->currentHistoryItem();
  SendUpdateState(item);
}
#if defined(S_MULTISELECTION_BOUNDS)
void RenderViewImpl::SyncSelectionIfRequired(bool isFlag)
#else
void RenderViewImpl::SyncSelectionIfRequired()
#endif
{
  WebFrame* frame = webview()->focusedFrame();
  if (!frame)
    return;

  base::string16 text;
  size_t offset;
  gfx::Range range;
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->GetSurroundingText(&text, &range);
    offset = 0;  // Pepper API does not support offset reporting.
    // TODO(kinaba): cut as needed.
  } else
#endif
  {
    size_t location, length;
    if (!webview()->caretOrSelectionRange(&location, &length))
    	{
    	   #if defined(S_PLM_P140714_04554)
		   if(frame->selectionAsText().length() == 0)
           		 UpdateSelectionBounds();
	   #endif
      	   return;
    	}
    range = gfx::Range(location, location + length);

    if (webview()->textInputInfo().type != blink::WebTextInputTypeNone) {
      // If current focused element is editable, we will send 100 more chars
      // before and after selection. It is for input method surrounding text
      // feature.
      if (location > kExtraCharsBeforeAndAfterSelection)
        offset = location - kExtraCharsBeforeAndAfterSelection;
      else
        offset = 0;
      length = location + length - offset + kExtraCharsBeforeAndAfterSelection;
      WebRange webrange = WebRange::fromDocumentRange(frame, offset, length);
      if (!webrange.isNull())
        text = WebRange::fromDocumentRange(frame, offset, length).toPlainText();
    } else {
      offset = location;
      text = frame->selectionAsText();
      // http://crbug.com/101435
      // In some case, frame->selectionAsText() returned text's length is not
      // equal to the length returned from webview()->caretOrSelectionRange().
      // So we have to set the range according to text.length().
      range.set_end(range.start() + text.length());
    }
  }

  // Sometimes we get repeated didChangeSelection calls from webkit when
  // the selection hasn't actually changed. We don't want to report these
  // because it will cause us to continually claim the X clipboard.
  if (selection_text_offset_ != offset ||
      selection_range_ != range ||
      selection_text_ != text) {
    selection_text_ = text;
    selection_text_offset_ = offset;
    selection_range_ = range;
    Send(new ViewHostMsg_SelectionChanged(routing_id_, text, offset, range));
  }
#if defined(S_MULTISELECTION_BOUNDS)
  UpdateSelectionBounds(isFlag);
#else
  UpdateSelectionBounds();
#endif
}

GURL RenderViewImpl::GetLoadingUrl(blink::WebFrame* frame) const {
  WebDataSource* ds = frame->dataSource();
  if (ds->hasUnreachableURL())
    return ds->unreachableURL();

  const WebURLRequest& request = ds->request();
  return request.url();
}

blink::WebPlugin* RenderViewImpl::GetWebPluginFromPluginDocument() {
  return webview()->mainFrame()->document().to<WebPluginDocument>().plugin();
}

void RenderViewImpl::OnFind(int request_id,
                            const base::string16& search_text,
                            const WebFindOptions& options) {
  WebFrame* main_frame = webview()->mainFrame();

  // Check if the plugin still exists in the document.
  if (main_frame->document().isPluginDocument() &&
      GetWebPluginFromPluginDocument()) {
    if (options.findNext) {
      // Just navigate back/forward.
      GetWebPluginFromPluginDocument()->selectFindResult(options.forward);
    } else {
      if (!GetWebPluginFromPluginDocument()->startFind(
          search_text, options.matchCase, request_id)) {
        // Send "no results".
        SendFindReply(request_id, 0, 0, gfx::Rect(), true);
      }
    }
    return;
  }

  WebFrame* frame_after_main = main_frame->traverseNext(true);
  WebFrame* focused_frame = webview()->focusedFrame();
  WebFrame* search_frame = focused_frame;  // start searching focused frame.

  bool multi_frame = (frame_after_main != main_frame);

  // If we have multiple frames, we don't want to wrap the search within the
  // frame, so we check here if we only have main_frame in the chain.
  bool wrap_within_frame = !multi_frame;

  WebRect selection_rect;
  bool result = false;

  // If something is selected when we start searching it means we cannot just
  // increment the current match ordinal; we need to re-generate it.
  WebRange current_selection = focused_frame->selectionRange();

  do {
    result = search_frame->find(
        request_id, search_text, options, wrap_within_frame, &selection_rect);

    if (!result) {
      // don't leave text selected as you move to the next frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"),
                                   GetFocusedNode());

      // Find the next frame, but skip the invisible ones.
      do {
        // What is the next frame to search? (we might be going backwards). Note
        // that we specify wrap=true so that search_frame never becomes NULL.
        search_frame = options.forward ?
            search_frame->traverseNext(true) :
            search_frame->traversePrevious(true);
      } while (!search_frame->hasVisibleContent() &&
               search_frame != focused_frame);

      // Make sure selection doesn't affect the search operation in new frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"),
                                   GetFocusedNode());

      // If we have multiple frames and we have wrapped back around to the
      // focused frame, we need to search it once more allowing wrap within
      // the frame, otherwise it will report 'no match' if the focused frame has
      // reported matches, but no frames after the focused_frame contain a
      // match for the search word(s).
      if (multi_frame && search_frame == focused_frame) {
        result = search_frame->find(
            request_id, search_text, options, true,  // Force wrapping.
            &selection_rect);
      }
    }

    webview()->setFocusedFrame(search_frame);
  } while (!result && search_frame != focused_frame);

  if (options.findNext && current_selection.isNull()) {
    // Force the main_frame to report the actual count.
    main_frame->increaseMatchCount(0, request_id);
  } else {
    // If nothing is found, set result to "0 of 0", otherwise, set it to
    // "-1 of 1" to indicate that we found at least one item, but we don't know
    // yet what is active.
    int ordinal = result ? -1 : 0;  // -1 here means, we might know more later.
    int match_count = result ? 1 : 0;  // 1 here means possibly more coming.

    // If we find no matches then this will be our last status update.
    // Otherwise the scoping effort will send more results.
    bool final_status_update = !result;

    SendFindReply(request_id, match_count, ordinal, selection_rect,
                  final_status_update);

    // Scoping effort begins, starting with the mainframe.
    search_frame = main_frame;

    main_frame->resetMatchCount();

    do {
      // Cancel all old scoping requests before starting a new one.
      search_frame->cancelPendingScopingEffort();

      // We don't start another scoping effort unless at least one match has
      // been found.
      if (result) {
        // Start new scoping request. If the scoping function determines that it
        // needs to scope, it will defer until later.
        search_frame->scopeStringMatches(request_id,
                                         search_text,
                                         options,
                                         true);  // reset the tickmarks
      }

      // Iterate to the next frame. The frame will not necessarily scope, for
      // example if it is not visible.
      search_frame = search_frame->traverseNext(true);
    } while (search_frame != main_frame);
  }
}

void RenderViewImpl::OnStopFinding(StopFindAction action) {
  WebView* view = webview();
  if (!view)
    return;

  WebDocument doc = view->mainFrame()->document();
  if (doc.isPluginDocument() && GetWebPluginFromPluginDocument()) {
    GetWebPluginFromPluginDocument()->stopFind();
    return;
  }

  bool clear_selection = action == STOP_FIND_ACTION_CLEAR_SELECTION;
  if (clear_selection) {
    view->focusedFrame()->executeCommand(WebString::fromUTF8("Unselect"),
                                         GetFocusedNode());
  }

  WebFrame* frame = view->mainFrame();
  while (frame) {
    frame->stopFinding(clear_selection);
    frame = frame->traverseNext(false);
  }

  if (action == STOP_FIND_ACTION_ACTIVATE_SELECTION) {
    WebFrame* focused_frame = view->focusedFrame();
    if (focused_frame) {
      WebDocument doc = focused_frame->document();
      if (!doc.isNull()) {
        WebNode node = doc.focusedNode();
        if (!node.isNull())
          node.simulateClick();
      }
    }
  }
}

#if defined(OS_ANDROID)
void RenderViewImpl::OnActivateNearestFindResult(int request_id,
                                                 float x, float y) {
  if (!webview())
      return;

  WebFrame* main_frame = webview()->mainFrame();
  WebRect selection_rect;
  int ordinal = main_frame->selectNearestFindMatch(WebFloatPoint(x, y),
                                                   &selection_rect);
  if (ordinal == -1) {
    // Something went wrong, so send a no-op reply (force the main_frame to
    // report the current match count) in case the host is waiting for a
    // response due to rate-limiting).
    main_frame->increaseMatchCount(0, request_id);
    return;
  }

  SendFindReply(request_id,
                -1 /* number_of_matches */,
                ordinal,
                selection_rect,
                true /* final_update */);
}

void RenderViewImpl::OnFindMatchRects(int current_version) {
  if (!webview())
      return;

  WebFrame* main_frame = webview()->mainFrame();
  std::vector<gfx::RectF> match_rects;

  int rects_version = main_frame->findMatchMarkersVersion();
  if (current_version != rects_version) {
    WebVector<WebFloatRect> web_match_rects;
    main_frame->findMatchRects(web_match_rects);
    match_rects.reserve(web_match_rects.size());
    for (size_t i = 0; i < web_match_rects.size(); ++i)
      match_rects.push_back(gfx::RectF(web_match_rects[i]));
  }

  gfx::RectF active_rect = main_frame->activeFindMatchRect();
  Send(new ViewHostMsg_FindMatchRects_Reply(routing_id_,
                                               rects_version,
                                               match_rects,
                                               active_rect));
}
#endif

void RenderViewImpl::OnZoom(PageZoom zoom) {
  if (!webview())  // Not sure if this can happen, but no harm in being safe.
    return;

  webview()->hidePopups();

  double old_zoom_level = webview()->zoomLevel();
  double zoom_level;
  if (zoom == PAGE_ZOOM_RESET) {
    zoom_level = 0;
  } else if (static_cast<int>(old_zoom_level) == old_zoom_level) {
    // Previous zoom level is a whole number, so just increment/decrement.
    zoom_level = old_zoom_level + zoom;
  } else {
    // Either the user hit the zoom factor limit and thus the zoom level is now
    // not a whole number, or a plugin changed it to a custom value.  We want
    // to go to the next whole number so that the user can always get back to
    // 100% with the keyboard/menu.
    if ((old_zoom_level > 1 && zoom > 0) ||
        (old_zoom_level < 1 && zoom < 0)) {
      zoom_level = static_cast<int>(old_zoom_level + zoom);
    } else {
      // We're going towards 100%, so first go to the next whole number.
      zoom_level = static_cast<int>(old_zoom_level);
    }
  }
  webview()->setZoomLevel(zoom_level);
  zoomLevelChanged();
}

void RenderViewImpl::OnZoomFactor(PageZoom zoom, int zoom_center_x,
                                  int zoom_center_y) {
  ZoomFactorHelper(zoom, zoom_center_x, zoom_center_y,
                   kScalingIncrementForGesture);
}

void RenderViewImpl::ZoomFactorHelper(PageZoom zoom,
                                      int zoom_center_x,
                                      int zoom_center_y,
                                      float scaling_increment) {
  if (!webview())  // Not sure if this can happen, but no harm in being safe.
    return;

  double old_page_scale_factor = webview()->pageScaleFactor();
  double page_scale_factor;
  if (zoom == PAGE_ZOOM_RESET) {
    page_scale_factor = 1.0;
  } else {
    page_scale_factor = old_page_scale_factor +
        (zoom > 0 ? scaling_increment : -scaling_increment);
  }
  if (page_scale_factor > 0) {
    webview()->setPageScaleFactor(page_scale_factor,
                                  WebPoint(zoom_center_x, zoom_center_y));
  }
}

void RenderViewImpl::OnSetZoomLevel(double zoom_level) {
  webview()->hidePopups();
  webview()->setZoomLevel(zoom_level);
  zoomLevelChanged();
}

void RenderViewImpl::OnSetZoomLevelForLoadingURL(const GURL& url,
                                                 double zoom_level) {
#if !defined(OS_ANDROID)
  // On Android, page zoom isn't used, and in case of WebView, text zoom is used
  // for legacy WebView text scaling emulation. Thus, the code that resets
  // the zoom level from this map will be effectively resetting text zoom level.
  host_zoom_levels_[url] = zoom_level;
#endif
}

void RenderViewImpl::OnSetPageEncoding(const std::string& encoding_name) {
  webview()->setPageEncoding(WebString::fromUTF8(encoding_name));
}

void RenderViewImpl::OnResetPageEncodingToDefault() {
  WebString no_encoding;
  webview()->setPageEncoding(no_encoding);
}

WebFrame* RenderViewImpl::GetChildFrame(const base::string16& xpath) const {
  if (xpath.empty())
    return webview()->mainFrame();

  // xpath string can represent a frame deep down the tree (across multiple
  // frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[0]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[0]
  std::vector<base::string16> xpaths;
  base::SplitString(xpath, '\n', &xpaths);

  WebFrame* frame = webview()->mainFrame();
  for (std::vector<base::string16>::const_iterator i = xpaths.begin();
       frame && i != xpaths.end(); ++i) {
    frame = frame->findChildByExpression(*i);
  }

  return frame;
}

void RenderViewImpl::OnScriptEvalRequest(const base::string16& frame_xpath,
                                         const base::string16& jscript,
                                         int id,
                                         bool notify_result) {
  TRACE_EVENT_INSTANT0("test_tracing", "OnScriptEvalRequest",
                       TRACE_EVENT_SCOPE_THREAD);
  EvaluateScript(frame_xpath, jscript, id, notify_result);
}

void RenderViewImpl::OnPostMessageEvent(
    const ViewMsg_PostMessage_Params& params) {
  // TODO(nasko): Support sending to subframes.
  WebFrame* frame = webview()->mainFrame();

  // Find the source frame if it exists.
  WebFrame* source_frame = NULL;
  if (params.source_routing_id != MSG_ROUTING_NONE) {
    RenderViewImpl* source_view = FromRoutingID(params.source_routing_id);
    if (source_view)
      source_frame = source_view->webview()->mainFrame();
  }

  // If the message contained MessagePorts, create the corresponding endpoints.
  DCHECK_EQ(params.message_port_ids.size(), params.new_routing_ids.size());
  blink::WebMessagePortChannelArray channels(params.message_port_ids.size());
  for (size_t i = 0;
       i < params.message_port_ids.size() && i < params.new_routing_ids.size();
       ++i) {
    channels[i] =
        new WebMessagePortChannelImpl(params.new_routing_ids[i],
                                      params.message_port_ids[i],
                                      base::MessageLoopProxy::current().get());
  }

  // Create an event with the message.  The final parameter to initMessageEvent
  // is the last event ID, which is not used with postMessage.
  WebDOMEvent event = frame->document().createEvent("MessageEvent");
  WebDOMMessageEvent msg_event = event.to<WebDOMMessageEvent>();
  msg_event.initMessageEvent("message",
                             // |canBubble| and |cancellable| are always false
                             false, false,
                             WebSerializedScriptValue::fromString(params.data),
                             params.source_origin, source_frame, "", channels);

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  WebSecurityOrigin target_origin;
  if (!params.target_origin.empty()) {
    target_origin =
        WebSecurityOrigin::createFromString(WebString(params.target_origin));
  }
  frame->dispatchMessageEventWithOriginCheck(target_origin, msg_event);
}

void RenderViewImpl::OnCSSInsertRequest(const base::string16& frame_xpath,
                                        const std::string& css) {
  WebFrame* frame = GetChildFrame(frame_xpath);
  if (!frame)
    return;

  frame->document().insertStyleSheet(WebString::fromUTF8(css));
}

void RenderViewImpl::OnAllowBindings(int enabled_bindings_flags) {
  if ((enabled_bindings_flags & BINDINGS_POLICY_WEB_UI) &&
      !(enabled_bindings_ & BINDINGS_POLICY_WEB_UI)) {
    new WebUIExtensionData(this);
  }

  enabled_bindings_ |= enabled_bindings_flags;

  // Keep track of the total bindings accumulated in this process.
  RenderProcess::current()->AddBindings(enabled_bindings_flags);
}

void RenderViewImpl::OnDragTargetDragEnter(const DropData& drop_data,
                                           const gfx::Point& client_point,
                                           const gfx::Point& screen_point,
                                           WebDragOperationsMask ops,
                                           int key_modifiers) {
  WebDragOperation operation = webview()->dragTargetDragEnter(
      DropDataToWebDragData(drop_data),
      client_point,
      screen_point,
      ops,
      key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderViewImpl::OnDragTargetDragOver(const gfx::Point& client_point,
                                          const gfx::Point& screen_point,
                                          WebDragOperationsMask ops,
                                          int key_modifiers) {
  WebDragOperation operation = webview()->dragTargetDragOver(
      client_point,
      screen_point,
      ops,
      key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderViewImpl::OnDragTargetDragLeave() {
  webview()->dragTargetDragLeave();
}

void RenderViewImpl::OnDragTargetDrop(const gfx::Point& client_point,
                                      const gfx::Point& screen_point,
                                      int key_modifiers) {
  webview()->dragTargetDrop(client_point, screen_point, key_modifiers);

  Send(new DragHostMsg_TargetDrop_ACK(routing_id_));
}

void RenderViewImpl::OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                              const gfx::Point& screen_point,
                                              bool ended,
                                              WebDragOperation op) {
  if (ended) {
    webview()->dragSourceEndedAt(client_point, screen_point, op);
  } else {
    webview()->dragSourceMovedTo(client_point, screen_point, op);
  }
}

void RenderViewImpl::OnDragSourceSystemDragEnded() {
  webview()->dragSourceSystemDragEnded();
}

void RenderViewImpl::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  ApplyWebPreferences(webkit_preferences_, webview());
}

void RenderViewImpl::OnUpdateTimezone() {
  if (webview())
    NotifyTimezoneChange(webview()->mainFrame());
}

void RenderViewImpl::OnSetTextZoomFactor(float zoom_factor) {
  if (!webview())
    return;
  // Hide selection and autofill popups.
  webview()->hidePopups();
  webview()->setTextZoomFactor(zoom_factor);
}

void RenderViewImpl::OnEnumerateDirectoryResponse(
    int id,
    const std::vector<base::FilePath>& paths) {
  if (!enumeration_completions_[id])
    return;

  WebVector<WebString> ws_file_names(paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    ws_file_names[i] = paths[i].AsUTF16Unsafe();

  enumeration_completions_[id]->didChooseFile(ws_file_names);
  enumeration_completions_.erase(id);
}

void RenderViewImpl::OnFileChooserResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  // This could happen if we navigated to a different page before the user
  // closed the chooser.
  if (file_chooser_completions_.empty())
    return;

  // Convert Chrome's SelectedFileInfo list to WebKit's.
  WebVector<WebFileChooserCompletion::SelectedFileInfo> selected_files(
      files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    WebFileChooserCompletion::SelectedFileInfo selected_file;
    selected_file.path = files[i].local_path.AsUTF16Unsafe();
    selected_file.displayName =
        base::FilePath(files[i].display_name).AsUTF16Unsafe();
    selected_files[i] = selected_file;
  }

  if (file_chooser_completions_.front()->completion)
    file_chooser_completions_.front()->completion->didChooseFile(
        selected_files);
  file_chooser_completions_.pop_front();

  // If there are more pending file chooser requests, schedule one now.
  if (!file_chooser_completions_.empty()) {
    Send(new ViewHostMsg_RunFileChooser(routing_id_,
        file_chooser_completions_.front()->params));
  }
}

void RenderViewImpl::OnEnableAutoResize(const gfx::Size& min_size,
                                        const gfx::Size& max_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;
  auto_resize_mode_ = true;
  webview()->enableAutoResizeMode(min_size, max_size);
}

void RenderViewImpl::OnDisableAutoResize(const gfx::Size& new_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;
  auto_resize_mode_ = false;
  webview()->disableAutoResizeMode();

  if (!new_size.IsEmpty()) {
    Resize(new_size,
           physical_backing_size_,
           overdraw_bottom_height_,
           resizer_rect_,
           is_fullscreen_,
           NO_RESIZE_ACK);
  }
}

void RenderViewImpl::OnEnablePreferredSizeChangedMode() {
  if (send_preferred_size_changes_)
    return;
  send_preferred_size_changes_ = true;

  // Start off with an initial preferred size notification (in case
  // |didUpdateLayout| was already called).
  didUpdateLayout();
}

void RenderViewImpl::OnDisableScrollbarsForSmallWindows(
    const gfx::Size& disable_scrollbar_size_limit) {
  disable_scrollbars_size_limit_ = disable_scrollbar_size_limit;
}

void RenderViewImpl::OnSetRendererPrefs(
    const RendererPreferences& renderer_prefs) {
  double old_zoom_level = renderer_preferences_.default_zoom_level;
  renderer_preferences_ = renderer_prefs;
  UpdateFontRenderingFromRendererPrefs();

#if defined(USE_DEFAULT_RENDER_THEME) || defined(TOOLKIT_GTK)
  if (renderer_prefs.use_custom_colors) {
    WebColorName name = blink::WebColorWebkitFocusRingColor;
    blink::setNamedColors(&name, &renderer_prefs.focus_ring_color, 1);
    blink::setCaretBlinkInterval(renderer_prefs.caret_blink_interval);
#if defined(TOOLKIT_GTK)
    ui::NativeTheme::instance()->SetScrollbarColors(
        renderer_prefs.thumb_inactive_color,
        renderer_prefs.thumb_active_color,
        renderer_prefs.track_color);
#endif  // defined(TOOLKIT_GTK)

    if (webview()) {
      webview()->setSelectionColors(
          renderer_prefs.active_selection_bg_color,
          renderer_prefs.active_selection_fg_color,
          renderer_prefs.inactive_selection_bg_color,
          renderer_prefs.inactive_selection_fg_color);
      webview()->themeChanged();
    }
  }
#endif  // defined(USE_DEFAULT_RENDER_THEME) || defined(TOOLKIT_GTK)

  if (RenderThreadImpl::current())  // Will be NULL during unit tests.
    RenderThreadImpl::current()->SetFlingCurveParameters(
        renderer_prefs.touchpad_fling_profile,
        renderer_prefs.touchscreen_fling_profile);

  // If the zoom level for this page matches the old zoom default, and this
  // is not a plugin, update the zoom level to match the new default.
  if (webview() && !webview()->mainFrame()->document().isPluginDocument() &&
      !ZoomValuesEqual(old_zoom_level,
                       renderer_preferences_.default_zoom_level) &&
      ZoomValuesEqual(webview()->zoomLevel(), old_zoom_level)) {
    webview()->setZoomLevel(renderer_preferences_.default_zoom_level);
    zoomLevelChanged();
  }
}

void RenderViewImpl::OnMediaPlayerActionAt(const gfx::Point& location,
                                           const WebMediaPlayerAction& action) {
  if (webview())
    webview()->performMediaPlayerAction(action, location);
}

void RenderViewImpl::OnOrientationChangeEvent(int orientation) {
  // Screen has rotated. 0 = default (portrait), 90 = one turn right, and so on.
  FOR_EACH_OBSERVER(RenderViewObserver,
                    observers_,
                    OrientationChangeEvent(orientation));
  webview()->mainFrame()->sendOrientationChangeEvent(orientation);
}

void RenderViewImpl::OnPluginActionAt(const gfx::Point& location,
                                      const WebPluginAction& action) {
  if (webview())
    webview()->performPluginAction(action, location);
}

void RenderViewImpl::OnGetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  // Prepare list to storage all savable resource links.
  std::vector<GURL> resources_list;
  std::vector<GURL> referrer_urls_list;
  std::vector<blink::WebReferrerPolicy> referrer_policies_list;
  std::vector<GURL> frames_list;
  SavableResourcesResult result(&resources_list,
                                &referrer_urls_list,
                                &referrer_policies_list,
                                &frames_list);

  // webkit/ doesn't know about Referrer.
  if (!GetAllSavableResourceLinksForCurrentPage(
          webview(),
          page_url,
          &result,
          const_cast<const char**>(GetSavableSchemes()))) {
    // If something is wrong when collecting all savable resource links,
    // send empty list to embedder(browser) to tell it failed.
    referrer_urls_list.clear();
    referrer_policies_list.clear();
    resources_list.clear();
    frames_list.clear();
  }

  std::vector<Referrer> referrers_list;
  CHECK_EQ(referrer_urls_list.size(), referrer_policies_list.size());
  for (unsigned i = 0; i < referrer_urls_list.size(); ++i) {
    referrers_list.push_back(
        Referrer(referrer_urls_list[i], referrer_policies_list[i]));
  }

  // Send result of all savable resource links to embedder.
  Send(new ViewHostMsg_SendCurrentPageAllSavableResourceLinks(routing_id(),
                                                              resources_list,
                                                              referrers_list,
                                                              frames_list));
}

void RenderViewImpl::OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<GURL>& links,
    const std::vector<base::FilePath>& local_paths,
    const base::FilePath& local_directory_name) {

  // Convert std::vector of GURLs to WebVector<WebURL>
  WebVector<WebURL> weburl_links(links);

  // Convert std::vector of base::FilePath to WebVector<WebString>
  WebVector<WebString> webstring_paths(local_paths.size());
  for (size_t i = 0; i < local_paths.size(); i++)
    webstring_paths[i] = local_paths[i].AsUTF16Unsafe();

  WebPageSerializer::serialize(webview()->mainFrame(), true, this, weburl_links,
                               webstring_paths,
                               local_directory_name.AsUTF16Unsafe());
}

void RenderViewImpl::OnShouldClose() {
  base::TimeTicks before_unload_start_time = base::TimeTicks::Now();
  bool should_close = webview()->dispatchBeforeUnloadEvent();
  base::TimeTicks before_unload_end_time = base::TimeTicks::Now();
  Send(new ViewHostMsg_ShouldClose_ACK(routing_id_, should_close,
                                       before_unload_start_time,
                                       before_unload_end_time));
}

void RenderViewImpl::OnSuppressDialogsUntilSwapOut() {
  // Don't show any more dialogs until we finish OnSwapOut.
  suppress_dialogs_until_swap_out_ = true;
}

void RenderViewImpl::OnSwapOut() {
  // Only run unload if we're not swapped out yet, but send the ack either way.
  if (!is_swapped_out_) {
    // Swap this RenderView out so the tab can navigate to a page rendered by a
    // different process.  This involves running the unload handler and clearing
    // the page.  Once WasSwappedOut is called, we also allow this process to
    // exit if there are no other active RenderViews in it.

    // Send an UpdateState message before we get swapped out.
    SyncNavigationState();

    // Synchronously run the unload handler before sending the ACK.
    webview()->dispatchUnloadEvent();

    // Swap out and stop sending any IPC messages that are not ACKs.
    SetSwappedOut(true);

    // Now that we're swapped out and filtering IPC messages, stop loading to
    // ensure that no other in-progress navigation continues.  We do this here
    // to avoid sending a DidStopLoading message to the browser process.
    OnStop();

    // Replace the page with a blank dummy URL. The unload handler will not be
    // run a second time, thanks to a check in FrameLoader::stopLoading.
    // TODO(creis): Need to add a better way to do this that avoids running the
    // beforeunload handler. For now, we just run it a second time silently.
    NavigateToSwappedOutURL(webview()->mainFrame());

    // Let WebKit know that this view is hidden so it can drop resources and
    // stop compositing.
    webview()->setVisibilityState(blink::WebPageVisibilityStateHidden, false);
  }

  // It is now safe to show modal dialogs again.
  suppress_dialogs_until_swap_out_ = false;

  Send(new ViewHostMsg_SwapOut_ACK(routing_id_));
}

void RenderViewImpl::NavigateToSwappedOutURL(blink::WebFrame* frame) {
  // We use loadRequest instead of loadHTMLString because the former commits
  // synchronously.  Otherwise a new navigation can interrupt the navigation
  // to kSwappedOutURL. If that happens to be to the page we had been
  // showing, then WebKit will never send a commit and we'll be left spinning.
  // TODO(creis): Until we move this to RenderFrame, we may call this from a
  // swapped out RenderFrame while our own is_swapped_out_ is false.
  RenderFrameImpl* rf = RenderFrameImpl::FromWebFrame(frame);
  CHECK(is_swapped_out_ || rf->is_swapped_out());
  GURL swappedOutURL(kSwappedOutURL);
  WebURLRequest request(swappedOutURL);
  frame->loadRequest(request);
}

void RenderViewImpl::OnClosePage() {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, ClosePage());
  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  webview()->dispatchUnloadEvent();

  Send(new ViewHostMsg_ClosePage_ACK(routing_id_));
}

void RenderViewImpl::OnThemeChanged() {
#if defined(USE_AURA)
  // Aura doesn't care if we switch themes.
#elif defined(OS_WIN)
  ui::NativeThemeWin::instance()->CloseHandles();
  if (webview())
    webview()->themeChanged();
#else  // defined(OS_WIN)
  // TODO(port): we don't support theming on non-Windows platforms yet
  NOTIMPLEMENTED();
#endif
}

void RenderViewImpl::OnMoveOrResizeStarted() {
  if (webview())
    webview()->hidePopups();
}

void RenderViewImpl::OnResize(const ViewMsg_Resize_Params& params) {
  if (webview()) {
    webview()->hidePopups();
    if (send_preferred_size_changes_) {
      webview()->mainFrame()->setCanHaveScrollbars(
          ShouldDisplayScrollbars(params.new_size.width(),
                                  params.new_size.height()));
    }
    UpdateScrollState(webview()->mainFrame());
  }

  RenderWidget::OnResize(params);
}

void RenderViewImpl::DidInitiatePaint() {
#if defined(ENABLE_PLUGINS)
  // Notify all instances that we painted.  The same caveats apply as for
  // ViewFlushedPaint regarding instances closing themselves, so we take
  // similar precautions.
  PepperPluginSet plugins = active_pepper_instances_;
  for (PepperPluginSet::iterator i = plugins.begin(); i != plugins.end(); ++i) {
    if (active_pepper_instances_.find(*i) != active_pepper_instances_.end())
      (*i)->ViewInitiatedPaint();
  }
#endif
}

void RenderViewImpl::DidFlushPaint() {
#if defined(ENABLE_PLUGINS)
  // Notify all instances that we flushed. This will call into the plugin, and
  // we it may ask to close itself as a result. This will, in turn, modify our
  // set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  PepperPluginSet plugins = active_pepper_instances_;
  for (PepperPluginSet::iterator i = plugins.begin(); i != plugins.end(); ++i) {
    // The copy above makes sure our iterator is never invalid if some plugins
    // are destroyed. But some plugin may decide to close all of its views in
    // response to a paint in one of them, so we need to make sure each one is
    // still "current" before using it.
    //
    // It's possible that a plugin was destroyed, but another one was created
    // with the same address. In this case, we'll call ViewFlushedPaint on that
    // new plugin. But that's OK for this particular case since we're just
    // notifying all of our instances that the view flushed, and the new one is
    // one of our instances.
    //
    // What about the case where a new one is created in a callback at a new
    // address and we don't issue the callback? We're still OK since this
    // callback is used for flush callbacks and we could not have possibly
    // started a new paint for the new plugin while processing a previous paint
    // for an existing one.
    if (active_pepper_instances_.find(*i) != active_pepper_instances_.end())
      (*i)->ViewFlushedPaint();
  }
#endif

  // If the RenderWidget is closing down then early-exit, otherwise we'll crash.
  // See crbug.com/112921.
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();

  // If we have a provisional frame we are between the start and commit stages
  // of loading and we don't want to save stats.
  if (!main_frame->provisionalDataSource()) {
    WebDataSource* ds = main_frame->dataSource();
    DocumentState* document_state = DocumentState::FromDataSource(ds);
    InternalDocumentStateData* data =
        InternalDocumentStateData::FromDocumentState(document_state);
    if (data->did_first_visually_non_empty_layout() &&
        !data->did_first_visually_non_empty_paint()) {
      data->set_did_first_visually_non_empty_paint(true);
      Send(new ViewHostMsg_DidFirstVisuallyNonEmptyPaint(routing_id_,
                                                         page_id_));
    }

    // TODO(jar): The following code should all be inside a method, probably in
    // NavigatorState.
    Time now = Time::Now();
    if (document_state->first_paint_time().is_null()) {
      document_state->set_first_paint_time(now);
    }
    if (document_state->first_paint_after_load_time().is_null() &&
        !document_state->finish_load_time().is_null()) {
      document_state->set_first_paint_after_load_time(now);
    }
  }

  if (is_loading_) {
    enableDrawSuspend();
  }
}

PepperPluginInstanceImpl* RenderViewImpl::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip,
    float* scale_factor) {
#if defined(ENABLE_PLUGINS)
  for (PepperPluginSet::iterator i = active_pepper_instances_.begin();
       i != active_pepper_instances_.end(); ++i) {
    PepperPluginInstanceImpl* instance = *i;
    // In Flash fullscreen , the plugin contents should be painted onto the
    // fullscreen widget instead of the web page.
    if (!instance->FlashIsFullscreenOrPending() &&
        instance->GetBitmapForOptimizedPluginPaint(paint_bounds, dib, location,
                                                   clip, scale_factor))
      return *i;
  }
#endif
  return NULL;
}

gfx::Vector2d RenderViewImpl::GetScrollOffset() {
  WebSize scroll_offset = webview()->mainFrame()->scrollOffset();
  return gfx::Vector2d(scroll_offset.width, scroll_offset.height);
}

void RenderViewImpl::OnClearFocusedNode() {
  if (webview())
    webview()->clearFocusedNode();
}

void RenderViewImpl::OnSetBackground(const SkBitmap& background) {
  if (webview())
    webview()->setIsTransparent(!background.empty());
  if (compositor_)
    compositor_->setHasTransparentBackground(!background.empty());

  SetBackground(background);
}

void RenderViewImpl::OnSetAccessibilityMode(unsigned int new_mode) {
  if (accessibility_mode_ == new_mode)
    return;
  accessibility_mode_ = new_mode;
  if (renderer_accessibility_) {
    delete renderer_accessibility_;
    renderer_accessibility_ = NULL;
  }
  if (accessibility_mode_ == AccessibilityModeOff)
    return;

  if (accessibility_mode_ & AccessibilityModeFlagPlatformFullTree)
    renderer_accessibility_ = new RendererAccessibilityComplete(this);
#if !defined(OS_ANDROID)
  else
    renderer_accessibility_ = new RendererAccessibilityFocusOnly(this);
#endif
}

void RenderViewImpl::OnSetActive(bool active) {
  if (webview())
    webview()->setIsActive(active);

#if defined(ENABLE_PLUGINS) && defined(OS_MACOSX)
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetWindowFocus(active);
  }
#endif
}

#if defined(OS_MACOSX)
void RenderViewImpl::OnSetWindowVisibility(bool visible) {
#if defined(ENABLE_PLUGINS)
  // Inform plugins that their container has changed visibility.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(visible);
  }
#endif
}

void RenderViewImpl::OnWindowFrameChanged(const gfx::Rect& window_frame,
                                          const gfx::Rect& view_frame) {
#if defined(ENABLE_PLUGINS)
  // Inform plugins that their window's frame has changed.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->WindowFrameChanged(window_frame, view_frame);
  }
#endif
}

void RenderViewImpl::OnPluginImeCompositionCompleted(const base::string16& text,
                                                     int plugin_id) {
  // WebPluginDelegateProxy is responsible for figuring out if this event
  // applies to it or not, so inform all the delegates.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->ImeCompositionCompleted(text, plugin_id);
  }
}
#endif  // OS_MACOSX

void RenderViewImpl::Close() {
  // We need to grab a pointer to the doomed WebView before we destroy it.
  WebView* doomed = webview();
  RenderWidget::Close();
  g_view_map.Get().erase(doomed);
  g_routing_id_view_map.Get().erase(routing_id_);
}

void RenderViewImpl::DidHandleKeyEvent() {
  ClearEditCommands();
}

bool RenderViewImpl::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);

#if defined(ENABLE_PLUGINS)
  // This method is called for every mouse event that the render view receives.
  // And then the mouse event is forwarded to WebKit, which dispatches it to the
  // event target. Potentially a Pepper plugin will receive the event.
  // In order to tell whether a plugin gets the last mouse event and which it
  // is, we set |pepper_last_mouse_event_target_| to NULL here. If a plugin gets
  // the event, it will notify us via DidReceiveMouseEvent() and set itself as
  // |pepper_last_mouse_event_target_|.
  pepper_last_mouse_event_target_ = NULL;
#endif

  // If the mouse is locked, only the current owner of the mouse lock can
  // process mouse events.
  return mouse_lock_dispatcher_->WillHandleMouseEvent(event);
}

bool RenderViewImpl::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);
  return false;
}

void RenderViewImpl::DidHandleMouseEvent(const WebMouseEvent& event) {
  disableDrawSuspend();
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidHandleMouseEvent(event));
}

void RenderViewImpl::DidHandleTouchEvent(const WebTouchEvent& event) {
  disableDrawSuspend();
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidHandleTouchEvent(event));
}

bool RenderViewImpl::HasTouchEventHandlersAt(const gfx::Point& point) const {
  if (!webview())
    return false;
  return webview()->hasTouchEventHandlersAt(point);
}

void RenderViewImpl::OnWasHidden() {
  RenderWidget::OnWasHidden();

#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  RenderThreadImpl::current()->video_capture_impl_manager()->
      SuspendDevices(true);
#endif

  if (webview())
    webview()->setVisibilityState(visibilityState(), false);

#if defined(ENABLE_PLUGINS)
  for (PepperPluginSet::iterator i = active_pepper_instances_.begin();
       i != active_pepper_instances_.end(); ++i)
    (*i)->PageVisibilityChanged(false);

#if defined(OS_MACOSX)
  // Inform NPAPI plugins that their container is no longer visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(false);
  }
#endif  // OS_MACOSX
#endif // ENABLE_PLUGINS
}

void RenderViewImpl::OnWasShown(bool needs_repainting) {
  if (draw_suspend_timer.IsRunning())
    return;

  RenderWidget::OnWasShown(needs_repainting);

#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  RenderThreadImpl::current()->video_capture_impl_manager()->
      SuspendDevices(false);
#endif

  if (webview())
    webview()->setVisibilityState(visibilityState(), false);

#if defined(ENABLE_PLUGINS)
  for (PepperPluginSet::iterator i = active_pepper_instances_.begin();
       i != active_pepper_instances_.end(); ++i)
    (*i)->PageVisibilityChanged(true);

#if defined(OS_MACOSX)
  // Inform NPAPI plugins that their container is now visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(true);
  }
#endif  // OS_MACOSX
#endif  // ENABLE_PLUGINS
}

GURL RenderViewImpl::GetURLForGraphicsContext3D() {
  DCHECK(webview());
  if (webview()->mainFrame())
    return GURL(webview()->mainFrame()->document().url());
  else
    return GURL("chrome://gpu/RenderViewImpl::CreateGraphicsContext3D");
}

bool RenderViewImpl::ForceCompositingModeEnabled() {
  return webkit_preferences_.force_compositing_mode;
}

void RenderViewImpl::OnSetFocus(bool enable) {
  RenderWidget::OnSetFocus(enable);

#if defined(ENABLE_PLUGINS)
  if (webview() && webview()->isActive()) {
    // Notify all NPAPI plugins.
    std::set<WebPluginDelegateProxy*>::iterator plugin_it;
    for (plugin_it = plugin_delegates_.begin();
         plugin_it != plugin_delegates_.end(); ++plugin_it) {
#if defined(OS_MACOSX)
      // RenderWidget's call to setFocus can cause the underlying webview's
      // activation state to change just like a call to setIsActive.
      if (enable)
        (*plugin_it)->SetWindowFocus(true);
#endif
      (*plugin_it)->SetContentAreaFocus(enable);
    }
  }
  // Notify all Pepper plugins.
  for (PepperPluginSet::iterator i = active_pepper_instances_.begin();
       i != active_pepper_instances_.end(); ++i)
    (*i)->SetContentAreaFocus(enable);
#endif
  // Notify all BrowserPlugins of the RenderView's focus state.
  if (browser_plugin_manager_.get())
    browser_plugin_manager_->UpdateFocusState();
}

void RenderViewImpl::OnImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeSetComposition(
        text, underlines, selection_start, selection_end);
    return;
  }

#if defined(OS_WIN)
  // When a plug-in has focus, we create platform-specific IME data used by
  // our IME emulator and send it directly to the focused plug-in, i.e. we
  // bypass WebKit. (WebPluginDelegate dispatches this IME data only when its
  // instance ID is the same one as the specified ID.)
  if (focused_plugin_id_ >= 0) {
    std::vector<int> clauses;
    std::vector<int> target;
    for (size_t i = 0; i < underlines.size(); ++i) {
      clauses.push_back(underlines[i].startOffset);
      clauses.push_back(underlines[i].endOffset);
      if (underlines[i].thick) {
        target.clear();
        target.push_back(underlines[i].startOffset);
        target.push_back(underlines[i].endOffset);
      }
    }
    std::set<WebPluginDelegateProxy*>::iterator it;
    for (it = plugin_delegates_.begin(); it != plugin_delegates_.end(); ++it) {
      (*it)->ImeCompositionUpdated(text, clauses, target, selection_end,
                                   focused_plugin_id_);
    }
    return;
  }
#endif  // OS_WIN
#endif  // ENABLE_PLUGINS
  RenderWidget::OnImeSetComposition(text,
                                    underlines,
                                    selection_start,
                                    selection_end);
}

void RenderViewImpl::OnImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeConfirmComposition(
        text, replacement_range, keep_selection);
    return;
  }
#if defined(OS_WIN)
  // Same as OnImeSetComposition(), we send the text from IMEs directly to
  // plug-ins. When we send IME text directly to plug-ins, we should not send
  // it to WebKit to prevent WebKit from controlling IMEs.
  // TODO(thakis): Honor |replacement_range| for plugins?
  if (focused_plugin_id_ >= 0) {
    std::set<WebPluginDelegateProxy*>::iterator it;
    for (it = plugin_delegates_.begin();
          it != plugin_delegates_.end(); ++it) {
      (*it)->ImeCompositionCompleted(text, focused_plugin_id_);
    }
    return;
  }
#endif  // OS_WIN
#endif  // ENABLE_PLUGINS
  if (replacement_range.IsValid() && webview()) {
    // Select the text in |replacement_range|, it will then be replaced by
    // text added by the call to RenderWidget::OnImeConfirmComposition().
    if (WebFrame* frame = webview()->focusedFrame()) {
      WebRange webrange = WebRange::fromDocumentRange(
          frame, replacement_range.start(), replacement_range.length());
      if (!webrange.isNull())
        frame->selectRange(webrange);
    }
  }
  RenderWidget::OnImeConfirmComposition(text,
                                        replacement_range,
                                        keep_selection);
}

void RenderViewImpl::SetDeviceScaleFactor(float device_scale_factor) {
  RenderWidget::SetDeviceScaleFactor(device_scale_factor);
  if (webview()) {
    webview()->setDeviceScaleFactor(device_scale_factor);
    webview()->settings()->setAcceleratedCompositingForFixedPositionEnabled(
        ShouldUseFixedPositionCompositing(device_scale_factor_));
  webview()->settings()->setAcceleratedCompositingForOverflowScrollEnabled(
      ShouldUseAcceleratedCompositingForOverflowScroll(device_scale_factor_));
    webview()->settings()->setAcceleratedCompositingForTransitionEnabled(
        ShouldUseTransitionCompositing(device_scale_factor_));
    webview()->settings()->
        setAcceleratedCompositingForFixedRootBackgroundEnabled(
            ShouldUseAcceleratedFixedRootBackground(device_scale_factor_));
    webview()->settings()->setAcceleratedCompositingForScrollableFramesEnabled(
        ShouldUseAcceleratedCompositingForScrollableFrames(
            device_scale_factor_));
    webview()->settings()->setCompositedScrollingForFramesEnabled(
        ShouldUseCompositedScrollingForFrames(device_scale_factor_));
  }
  if (auto_resize_mode_)
    AutoResizeCompositor();

  if (browser_plugin_manager_.get())
    browser_plugin_manager_->UpdateDeviceScaleFactor(device_scale_factor_);
}

ui::TextInputType RenderViewImpl::GetTextInputType() {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->text_input_type();
#endif
  return RenderWidget::GetTextInputType();
}

void RenderViewImpl::GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    gfx::Rect caret = focused_pepper_plugin_->GetCaretBounds();
    *start = caret;
    *end = caret;
    return;
  }
#endif
  RenderWidget::GetSelectionBounds(start, end);
}

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
void RenderViewImpl::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();

#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    return;
  }
#endif

  if (!webview())
    return;
  size_t start_offset = 0;
  size_t character_count = 0;
  if (!webview()->compositionRange(&start_offset, &character_count))
    return;
  if (character_count == 0)
    return;

  blink::WebFrame* frame = webview()->focusedFrame();
  if (!frame)
    return;

  bounds->reserve(character_count);
  blink::WebRect webrect;
  for (size_t i = 0; i < character_count; ++i) {
    if (!frame->firstRectForCharacterRange(start_offset + i, 1, webrect)) {
      DLOG(ERROR) << "Could not retrieve character rectangle at " << i;
      bounds->clear();
      return;
    }
    bounds->push_back(webrect);
  }
}

void RenderViewImpl::GetCompositionRange(gfx::Range* range) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    return;
  }
#endif
  RenderWidget::GetCompositionRange(range);
}
#endif

bool RenderViewImpl::CanComposeInline() {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->IsPluginAcceptingCompositionEvents();
#endif
  return true;
}

void RenderViewImpl::InstrumentWillBeginFrame(int frame_id) {
  if (!webview())
    return;
#ifdef ENABLE_DEV_TOOLS
  if (!webview()->devToolsAgent())
    return;
  webview()->devToolsAgent()->didBeginFrame(frame_id);
#endif
}

void RenderViewImpl::InstrumentDidBeginFrame() {
  if (!webview())
    return;
#ifdef ENABLE_DEV_TOOLS
  if (!webview()->devToolsAgent())
    return;
  // TODO(jamesr/caseq): Decide if this needs to be renamed.
  webview()->devToolsAgent()->didComposite();
#endif
}

void RenderViewImpl::InstrumentDidCancelFrame() {
  if (!webview())
    return;
#ifdef ENABLE_DEV_TOOLS
  if (!webview()->devToolsAgent())
    return;
  webview()->devToolsAgent()->didCancelFrame();
#endif
}

void RenderViewImpl::InstrumentWillComposite() {
  if (!webview())
    return;
#ifdef ENABLE_DEV_TOOLS
  if (!webview()->devToolsAgent())
    return;
  webview()->devToolsAgent()->willComposite();
#endif
}

bool RenderViewImpl::AllowPartialSwap() const {
  return allow_partial_swap_;
}

void RenderViewImpl::SetScreenMetricsEmulationParameters(
    float device_scale_factor,
    const gfx::Point& root_layer_offset,
    float root_layer_scale) {
  if (webview()) {
    webview()->setCompositorDeviceScaleFactorOverride(device_scale_factor);
    webview()->setRootLayerTransform(
        blink::WebSize(root_layer_offset.x(), root_layer_offset.y()),
        root_layer_scale);
  }
}

bool RenderViewImpl::ScheduleFileChooser(
    const FileChooserParams& params,
    WebFileChooserCompletion* completion) {
  static const size_t kMaximumPendingFileChooseRequests = 4;
  if (file_chooser_completions_.size() > kMaximumPendingFileChooseRequests) {
    // This sanity check prevents too many file choose requests from getting
    // queued which could DoS the user. Getting these is most likely a
    // programming error (there are many ways to DoS the user so it's not
    // considered a "real" security check), either in JS requesting many file
    // choosers to pop up, or in a plugin.
    //
    // TODO(brettw) we might possibly want to require a user gesture to open
    // a file picker, which will address this issue in a better way.
    return false;
  }

  file_chooser_completions_.push_back(linked_ptr<PendingFileChooser>(
      new PendingFileChooser(params, completion)));
  if (file_chooser_completions_.size() == 1) {
    // Actually show the browse dialog when this is the first request.
    Send(new ViewHostMsg_RunFileChooser(routing_id_, params));
  }
  return true;
}

blink::WebGeolocationClient* RenderViewImpl::geolocationClient() {
  if (!geolocation_dispatcher_)
    geolocation_dispatcher_ = new GeolocationDispatcher(this);
  return geolocation_dispatcher_;
}

blink::WebSpeechInputController* RenderViewImpl::speechInputController(
    blink::WebSpeechInputListener* listener) {
#if defined(ENABLE_INPUT_SPEECH)
  if (!input_tag_speech_dispatcher_)
    input_tag_speech_dispatcher_ =
        new InputTagSpeechDispatcher(this, listener);
#endif
  return input_tag_speech_dispatcher_;
}

blink::WebSpeechRecognizer* RenderViewImpl::speechRecognizer() {
  if (!speech_recognition_dispatcher_)
    speech_recognition_dispatcher_ = new SpeechRecognitionDispatcher(this);
  return speech_recognition_dispatcher_;
}

void RenderViewImpl::zoomLimitsChanged(double minimum_level,
                                       double maximum_level) {
  // For now, don't remember plugin zoom values.  We don't want to mix them with
  // normal web content (i.e. a fixed layout plugin would usually want them
  // different).
  bool remember = !webview()->mainFrame()->document().isPluginDocument();

  int minimum_percent = static_cast<int>(
      ZoomLevelToZoomFactor(minimum_level) * 100);
  int maximum_percent = static_cast<int>(
      ZoomLevelToZoomFactor(maximum_level) * 100);

  Send(new ViewHostMsg_UpdateZoomLimits(
      routing_id_, minimum_percent, maximum_percent, remember));
}

void RenderViewImpl::zoomLevelChanged() {
  bool remember = !webview()->mainFrame()->document().isPluginDocument();
  float zoom_level = webview()->zoomLevel();

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, ZoomLevelChanged());

  // Tell the browser which url got zoomed so it can update the menu and the
  // saved values if necessary
  Send(new ViewHostMsg_DidZoomURL(
      routing_id_, zoom_level, remember,
      GURL(webview()->mainFrame()->document().url())));
}

double RenderViewImpl::zoomLevelToZoomFactor(double zoom_level) const {
  return ZoomLevelToZoomFactor(zoom_level);
}

double RenderViewImpl::zoomFactorToZoomLevel(double factor) const {
  return ZoomFactorToZoomLevel(factor);
}

void RenderViewImpl::registerProtocolHandler(const WebString& scheme,
                                             const WebURL& base_url,
                                             const WebURL& url,
                                             const WebString& title) {
  bool user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  GURL base(base_url);
  GURL absolute_url = base.Resolve(base::UTF16ToUTF8(url.string()));
  
  //since we are alreadychecking for orgin in navigatorcontentutils, no need to check again.
#if !defined(S_HTML5_CUSTOM_HANDLER_SUPPORT)
  if (base.GetOrigin() != absolute_url.GetOrigin()) {
    return;
  }
#endif 
  Send(new ViewHostMsg_RegisterProtocolHandler(routing_id_,
                                               base::UTF16ToUTF8(scheme),
                                               absolute_url,
                                               title,
                                               user_gesture));
}

#if defined(S_HTML5_CUSTOM_HANDLER_SUPPORT) 
void RenderViewImpl::unregisterProtocolHandler(const WebString& scheme,
                                               const WebURL& base_url,
                                               const WebURL& url) {
  bool user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  GURL base(base_url);
  GURL absolute_url = base.Resolve(base::UTF16ToUTF8(url.string()));
  if (base.GetOrigin() != absolute_url.GetOrigin())
    return;
  Send(new ViewHostMsg_UnregisterProtocolHandler(routing_id_,
                                                 base::UTF16ToUTF8(scheme),
                                                 absolute_url,
                                                 user_gesture));
}
#endif
blink::WebPageVisibilityState RenderViewImpl::visibilityState() const {
  blink::WebPageVisibilityState current_state = is_hidden() ?
      blink::WebPageVisibilityStateHidden :
      blink::WebPageVisibilityStateVisible;
  blink::WebPageVisibilityState override_state = current_state;
  // TODO(jam): move this method to WebFrameClient.
  if (GetContentClient()->renderer()->
          ShouldOverridePageVisibilityState(main_render_frame_.get(),
                                            &override_state))
    return override_state;
  return current_state;
}

blink::WebUserMediaClient* RenderViewImpl::userMediaClient() {
  // This can happen in tests, in which case it's OK to return NULL.
  if (!InitializeMediaStreamClient())
    return NULL;

  return web_user_media_client_;
}

blink::WebMIDIClient* RenderViewImpl::webMIDIClient() {
  if (!midi_dispatcher_)
    midi_dispatcher_ = new MidiDispatcher(this);
  return midi_dispatcher_;
}

#if defined(ENABLE_PUSH_API)
blink::WebPushClient* RenderViewImpl::webPushClient() {
  if (!push_registration_dispatcher_)
    push_registration_dispatcher_ = new PushRegistrationDispatcher(this);
  return push_registration_dispatcher_;
}
#endif

void RenderViewImpl::draggableRegionsChanged() {
  FOR_EACH_OBSERVER(
      RenderViewObserver,
      observers_,
      DraggableRegionsChanged(webview()->mainFrame()));
}

WebMediaPlayer* RenderViewImpl::CreateWebMediaPlayerForMediaStream(
    WebFrame* frame,
    const blink::WebURL& url,
    WebMediaPlayerClient* client) {
#if defined(ENABLE_WEBRTC)
  if (!InitializeMediaStreamClient()) {
    LOG(ERROR) << "Failed to initialize MediaStreamClient";
    return NULL;
  }
  if (media_stream_client_->IsMediaStream(url)) {
#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
    bool found_neon =
        (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0;
    UMA_HISTOGRAM_BOOLEAN("Platform.WebRtcNEONFound", found_neon);
#endif  // defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
    return new WebMediaPlayerMS(frame, client, AsWeakPtr(),
                                media_stream_client_, new RenderMediaLog());
  }
#endif  // defined(ENABLE_WEBRTC)
  return NULL;
}

#if defined(OS_ANDROID)
WebContentDetectionResult RenderViewImpl::detectContentAround(
    const WebHitTestResult& touch_hit) {
  DCHECK(!touch_hit.isNull());
  DCHECK(!touch_hit.node().isNull());
  DCHECK(touch_hit.node().isTextNode());

  // Process the position with all the registered content detectors until
  // a match is found. Priority is provided by their relative order.
  for (ContentDetectorList::const_iterator it = content_detectors_.begin();
      it != content_detectors_.end(); ++it) {
    ContentDetector::Result content = (*it)->FindTappedContent(touch_hit);
    if (content.valid) {
      return WebContentDetectionResult(content.content_boundaries,
          base::UTF8ToUTF16(content.text), content.intent_url);
    }
  }
  return WebContentDetectionResult();
}

void RenderViewImpl::scheduleContentIntent(const WebURL& intent) {
  // Introduce a short delay so that the user can notice the content.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&RenderViewImpl::LaunchAndroidContentIntent,
                 AsWeakPtr(),
                 intent,
                 expected_content_intent_id_),
      base::TimeDelta::FromMilliseconds(kContentIntentDelayMilliseconds));
}

void RenderViewImpl::cancelScheduledContentIntents() {
  ++expected_content_intent_id_;
}

#if defined(S_FP_HIDDEN_FORM_FIX)
void RenderViewImpl::checkFormVisibilityAndAutofill() {
 FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, CheckFormVisibilityAndAutofill());
}
#endif

void RenderViewImpl::LaunchAndroidContentIntent(const GURL& intent,
                                                size_t request_id) {
  if (request_id != expected_content_intent_id_)
      return;

  // Remove the content highlighting if any.
  scheduleComposite();

  if (!intent.is_empty())
    Send(new ViewHostMsg_StartContentIntent(routing_id_, intent));
}

bool RenderViewImpl::openDateTimeChooser(
    const blink::WebDateTimeChooserParams& params,
    blink::WebDateTimeChooserCompletion* completion) {
  // JavaScript may try to open a date time chooser while one is already open.
  if (date_time_picker_client_)
    return false;
  date_time_picker_client_.reset(
      new RendererDateTimePicker(this, params, completion));
  return date_time_picker_client_->Open();
}

void RenderViewImpl::DismissDateTimeDialog() {
  DCHECK(date_time_picker_client_);
  date_time_picker_client_.reset(NULL);
}

WebMediaPlayer* RenderViewImpl::CreateAndroidWebMediaPlayer(
      WebFrame* frame,
      const blink::WebURL& url,
      WebMediaPlayerClient* client) {
  GpuChannelHost* gpu_channel_host =
      RenderThreadImpl::current()->EstablishGpuChannelSync(
          CAUSE_FOR_GPU_LAUNCH_VIDEODECODEACCELERATOR_INITIALIZE);
  if (!gpu_channel_host) {
    LOG(ERROR) << "Failed to establish GPU channel for media player";
    return NULL;
  }

  scoped_ptr<StreamTextureFactory> stream_texture_factory;
  if (UsingSynchronousRendererCompositor()) {
    SynchronousCompositorFactory* factory =
        SynchronousCompositorFactory::GetInstance();
    stream_texture_factory = factory->CreateStreamTextureFactory(routing_id_);
  } else {
    scoped_refptr<webkit::gpu::ContextProviderWebContext> context_provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider();

    if (!context_provider.get()) {
      LOG(ERROR) << "Failed to get context3d for media player";
      return NULL;
    }

    stream_texture_factory.reset(new StreamTextureFactoryImpl(
        context_provider, gpu_channel_host, routing_id_));
  }

  return new WebMediaPlayerAndroid(
      frame,
      client,
      AsWeakPtr(),
      media_player_manager_,
      stream_texture_factory.release(),
      RenderThreadImpl::current()->GetMediaThreadMessageLoopProxy(),
      new RenderMediaLog());
}

#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
void RenderViewImpl::OnSelectPopupMenuItem(int selected_index) {
  if (external_popup_menu_ == NULL) {
    // Crash reports from the field indicate that we can be notified with a
    // NULL external popup menu (we probably get notified twice).
    // If you hit this please file a bug against jcivelli and include the page
    // and steps to repro.
    NOTREACHED();
    return;
  }
  external_popup_menu_->DidSelectItem(selected_index);
  external_popup_menu_.reset();
}
#endif

#if defined(OS_ANDROID)
void RenderViewImpl::OnSelectPopupMenuItems(
    bool canceled,
    const std::vector<int>& selected_indices) {
  // It is possible to receive more than one of these calls if the user presses
  // a select faster than it takes for the show-select-popup IPC message to make
  // it to the browser UI thread.  Ignore the extra-messages.
  // TODO(jcivelli): http:/b/5793321 Implement a better fix, as detailed in bug.
  if (!external_popup_menu_)
    return;
#if !defined(S_PLM_P141031_04993)
  canceled = canceled || !IsSelectNode(GetFocusedNode());
#endif
  external_popup_menu_->DidSelectItems(canceled, selected_indices);
  if (canceled)
    external_popup_menu_.reset();
}
#endif

void RenderViewImpl::OnShowContextMenu(const gfx::Point& location) {
  context_menu_source_type_ = ui::MENU_SOURCE_TOUCH_EDIT_MENU;
  touch_editing_context_menu_location_ = location;
  if (webview())
    webview()->showContextMenu();
}

void RenderViewImpl::OnEnableViewSourceMode() {
  if (!webview())
    return;
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;
  main_frame->enableViewSourceMode(true);
}

void RenderViewImpl::OnDisownOpener() {
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (main_frame && main_frame->opener())
    main_frame->setOpener(NULL);
}

#if defined(OS_ANDROID)
bool RenderViewImpl::didTapMultipleTargets(
    const blink::WebGestureEvent& event,
    const WebVector<WebRect>& target_rects) {
  // Never show a disambiguation popup when accessibility is enabled,
  // as this interferes with "touch exploration".
  bool matchesAccessibilityModeComplete =
      (accessibility_mode_ & AccessibilityModeComplete) ==
      AccessibilityModeComplete;
  if (matchesAccessibilityModeComplete)
    return false;

  gfx::Rect finger_rect(
      event.x - event.data.tap.width / 2, event.y - event.data.tap.height / 2,
      event.data.tap.width, event.data.tap.height);
  gfx::Rect zoom_rect;

#if defined(S_NATIVE_SUPPORT)
  DisambiguationPopupHelper::setDeviceScaleFactor(device_scale_factor_);
#endif
  float new_total_scale =
      DisambiguationPopupHelper::ComputeZoomAreaAndScaleFactor(
          finger_rect, target_rects, GetSize(),
          gfx::Rect(webview()->mainFrame()->visibleContentRect()).size(),
          device_scale_factor_ * webview()->pageScaleFactor(), &zoom_rect);
  if (!new_total_scale)
    return false;

  bool handled = false;
  switch (renderer_preferences_.tap_multiple_targets_strategy) {
    case TAP_MULTIPLE_TARGETS_STRATEGY_ZOOM:
      handled = webview()->zoomToMultipleTargetsRect(zoom_rect);
      break;
    case TAP_MULTIPLE_TARGETS_STRATEGY_POPUP: {
      gfx::Size canvas_size =
          gfx::ToCeiledSize(gfx::ScaleSize(zoom_rect.size(), new_total_scale));
      TransportDIB* transport_dib = NULL;
      {
        scoped_ptr<skia::PlatformCanvas> canvas(
            RenderProcess::current()->GetDrawingCanvas(&transport_dib,
                                                       gfx::Rect(canvas_size)));
        if (!canvas) {
          handled = false;
          break;
        }

        // TODO(trchen): Cleanup the device scale factor mess.
        // device scale will be applied in WebKit
        // --> zoom_rect doesn't include device scale,
        //     but WebKit will still draw on zoom_rect * device_scale_factor_
        canvas->scale(new_total_scale / device_scale_factor_,
                      new_total_scale / device_scale_factor_);
        canvas->translate(-zoom_rect.x() * device_scale_factor_,
                          -zoom_rect.y() * device_scale_factor_);

        webwidget_->paint(
            canvas.get(),
            zoom_rect,
            WebWidget::ForceSoftwareRenderingAndIgnoreGPUResidentContent);
      }

      gfx::Rect physical_window_zoom_rect = gfx::ToEnclosingRect(
          ClientRectToPhysicalWindowRect(gfx::RectF(zoom_rect)));
      Send(new ViewHostMsg_ShowDisambiguationPopup(routing_id_,
                                                   physical_window_zoom_rect,
                                                   canvas_size,
                                                   transport_dib->id()));
      handled = true;
      break;
    }
    case TAP_MULTIPLE_TARGETS_STRATEGY_NONE:
      // No-op.
      break;
  }

  return handled;
}
#endif

unsigned RenderViewImpl::GetLocalSessionHistoryLengthForTesting() const {
  return history_list_length_;
}

void RenderViewImpl::SetFocusAndActivateForTesting(bool enable) {
  if (enable) {
    if (has_focus())
      return;
    OnSetActive(true);
    OnSetFocus(true);
  } else {
    if (!has_focus())
      return;
    OnSetFocus(false);
    OnSetActive(false);
  }
}

void RenderViewImpl::SetDeviceScaleFactorForTesting(float factor) {
  ViewMsg_Resize_Params params;
  params.screen_info = screen_info_;
  params.screen_info.deviceScaleFactor = factor;
  params.new_size = size();
  params.physical_backing_size =
      gfx::ToCeiledSize(gfx::ScaleSize(size(), factor));
  params.overdraw_bottom_height = 0.f;
  params.resizer_rect = WebRect();
  params.is_fullscreen = is_fullscreen();
  OnResize(params);
}

void RenderViewImpl::ForceResizeForTesting(const gfx::Size& new_size) {
  gfx::Rect new_position(rootWindowRect().x,
                         rootWindowRect().y,
                         new_size.width(),
                         new_size.height());
  ResizeSynchronously(new_position);
}

void RenderViewImpl::UseSynchronousResizeModeForTesting(bool enable) {
  resizing_mode_selector_->set_is_synchronous_mode(enable);
}

void RenderViewImpl::EnableAutoResizeForTesting(const gfx::Size& min_size,
                                                const gfx::Size& max_size) {
  OnEnableAutoResize(min_size, max_size);
}

void RenderViewImpl::DisableAutoResizeForTesting(const gfx::Size& new_size) {
  OnDisableAutoResize(new_size);
}

void RenderViewImpl::SetMediaStreamClientForTesting(
    MediaStreamClient* media_stream_client) {
  DCHECK(!media_stream_client_);
  DCHECK(!web_user_media_client_);
  media_stream_client_ = media_stream_client;
}

void RenderViewImpl::OnReleaseDisambiguationPopupDIB(
    TransportDIB::Handle dib_handle) {
  TransportDIB* dib = TransportDIB::CreateWithHandle(dib_handle);
  RenderProcess::current()->ReleaseTransportDIB(dib);
}

#if defined(S_TRANSPORT_DIB_FOR_SOFT_BITMAP)
void RenderViewImpl::OnReleaseSnapshotDIB(
    TransportDIB::Handle dib_handle) {
  LOG(INFO)<<"RenderViewImpl::OnReleaseSnapshotDIB :: Going to release transport DIB for snapshot";  
  TransportDIB* dib = TransportDIB::CreateWithHandle(dib_handle);
  RenderProcess::current()->ReleaseTransportDIB(dib);
}
#endif //S_TRANSPORT_DIB_FOR_SOFT_BITMAP

void RenderViewImpl::OnGetSelectionVisibilityStatus() {
  if (!webview())
    return;

  bool selection_visible = webview()->isSelectionWithinVisibleRect();
  Send(new ViewHostMsg_SelectionVisibilityStatusReceived(routing_id_, selection_visible));
}

#if defined(S_SCROLL_EVENT)
void RenderViewImpl::updateTextFieldBounds(blink::WebRect inputFieldBounds)
{
  gfx::Rect input_rect = gfx::Rect(inputFieldBounds.x, inputFieldBounds.y, inputFieldBounds.width, inputFieldBounds.height);
  Send(new ViewHostMsg_UpdateTextFieldBounds(routing_id_, input_rect));
}
#endif
void RenderViewImpl::OnCheckBelongToSelection(int x, int y) {
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  bool on_region = main_frame->pointOnRegion(x, y);
  Send(new ViewHostMsg_PointOnRegion(routing_id_, on_region));
}

void RenderViewImpl::OnGetSelectionBitmap() {
  if (!webview())
    return;

  SkBitmap selected_region;
  webview()->selectionAsBitmap(selected_region);
  Send(new ViewHostMsg_SelectedBitmap(routing_id_, selected_region));
}

void RenderViewImpl::OnSelectClosestWord(int x, int y) {
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  main_frame->selectClosestWord(x, y);
}

void RenderViewImpl::OnClearTextSelection() {
  if (!webview())
    return;
#if defined(S_PLM_P140714_04554)
	base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
#endif
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  main_frame->clearTextSelection();
}

void RenderViewImpl::OnHandleSelectionDrop(int x, int y,
                                           const base::string16& text) {
  if (!webview())
    return;

  webview()->onHandleSelectionDrop(x, y, WebString(text));
}

void RenderViewImpl::OnHandleSelectionDropOnFocusedInput(const base::string16& text,
    int dropAction) {
  if (!webview())
    return;

  webview()->handleSelectionDropOnFocusedInput(WebString(text), dropAction);
}

void RenderViewImpl::OnGetFocusedInputInfo() {
  if (!webview())
    return;
  WebRect focusedInputBounds = WebRect();
  bool isMultiLineInput = false;
  bool isContentRichlyEditable = false;
  gfx::Rect inputRect = gfx::Rect();
  bool isValidFocusedInput = webview()->getFocusedInputInfo(focusedInputBounds, isMultiLineInput, isContentRichlyEditable);
  if (isValidFocusedInput) {
      inputRect = gfx::Rect(focusedInputBounds.x, focusedInputBounds.y, focusedInputBounds.width, focusedInputBounds.height);
  } 
  Send(new ViewHostMsg_UpdateFocusedInputInfo(routing_id_, inputRect, isMultiLineInput, isContentRichlyEditable));
}

void RenderViewImpl::OnGetSelectionMarkup() {
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  base::string16 markup = main_frame->selectionAsMarkup();
  Send(new ViewHostMsg_SelectedMarkup(routing_id_, markup));
}
//MULTI-SELECTION >>
#if defined(SBROWSER_MULTI_SELECTION)
void RenderViewImpl::OnGetSelectionMarkupWithBounds() {
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  base::string16 markup = main_frame->selectionAsMarkup();
  WebRect anchor_webrect;
  gfx::Rect selection_start_content_rect = gfx::Rect();
  bool is_selected = webview()->getSelectionStartContentBounds(anchor_webrect);
  if(is_selected) {
    selection_start_content_rect = gfx::Rect(anchor_webrect.x, anchor_webrect.y, anchor_webrect.width, anchor_webrect.height);
  }
  Send(new ViewHostMsg_SelectedMarkupWithStartContentRect(routing_id_, markup, selection_start_content_rect));
}
#endif
//MULTI-SELECTION <<
void RenderViewImpl::OnGetCurrentSelectionRect() {
  if (!webview())
    return;

  WebRect selectionRect = webview()->currentSelectionRect();
  gfx::Rect rect = gfx::Rect(selectionRect.x, selectionRect.y,
    selectionRect.width, selectionRect.height);
  Send(new ViewHostMsg_UpdateSelectionRect(routing_id_, rect));
}

void RenderViewImpl::OnGetTouchedFixedElementHeight(int x, int y) {
    int height = 0;
    //call engine api and get height of currently touched fixed element
    if (webview()) {
        height = webview()->getHeightOfFixedElement(x,y);
        //LOG(INFO) << __FUNCTION__ << " Fixed element (" << x << ", " << y <<") height = " << height;
    }
    Send( new ViewHostMsg_UpdateTouchedFixedElementHeight(routing_id_,height));
}

void RenderViewImpl::OnLoadDataWithBaseUrl(const std::string& data,
                                           const std::string& base_url,
                                           const std::string& mime_type,
                                           const std::string& encoding,
                                           const std::string& history_url) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (main_frame) {
  	LOG(INFO)<<"SBRContent ::RenderViewImpl::OnLoadDataWithBaseUrl";
      main_frame->loadData(WebData(data.data(), data.size()),
                           WebString::fromUTF8(mime_type),
                           WebString::fromUTF8(encoding),
                           WebURL(GURL(base_url.data())),
                           WebURL(GURL(history_url.data())), true);
  }
}

void RenderViewImpl::OnMoveNextTextOrSelectElement() {
  webview()->moveFocusToNext();
}

void RenderViewImpl::OnMovePreviousTextOrSelectElement() {
  webview()->moveFocusToPrevious();
}

void RenderViewImpl::OnRecognizeArticle(int mode) {
  clock_t startTime = 0;
  if (kSamsungReaderDebugFlag)
    startTime = clock();

  std::string reader_result_str = webview()->mainFrame()->recognizeArticle(
                                    static_cast<WebFrame::ReaderMode>(mode));

  if (kSamsungReaderDebugFlag) {
    VLOG(0) << "recognizeArticle native Time : "
            << (1000.0 * ((double)(clock() - startTime) / CLOCKS_PER_SEC))
            << "ms";
  }
  Send(new ViewHostMsg_OnRecognizeArticleResult(routing_id_, reader_result_str));
}

void RenderViewImpl::DidCommitCompositorFrame() {
  RenderWidget::DidCommitCompositorFrame();
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidCommitCompositorFrame());
}

void RenderViewImpl::SendUpdateFaviconURL(const std::vector<FaviconURL>& urls) {
  if (!urls.empty())
    Send(new ViewHostMsg_UpdateFaviconURL(routing_id_, page_id_, urls));
}

void RenderViewImpl::DidStopLoadingIcons() {
  int icon_types = WebIconURL::TypeFavicon;
  if (TouchEnabled())
    icon_types |= WebIconURL::TypeTouchPrecomposed | WebIconURL::TypeTouch;

  WebVector<WebIconURL> icon_urls =
      webview()->mainFrame()->iconURLs(icon_types);

  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    WebURL url = icon_urls[i].iconURL();
    if (!url.isEmpty())
      urls.push_back(FaviconURL(url,
                                ToFaviconType(icon_urls[i].iconType())));
  }
  SendUpdateFaviconURL(urls);
}
#if defined(S_PLM_P140721_03683)
void RenderViewImpl::OnEnterKeyOnFocused(){
  WebNode node = GetFocusedNode();
  if (node.isNull() || IsEditableNode(node) || IsSelectNode(node))
     return;
  WebRect box_rect = webview()->focusedElementBounds();
   if (box_rect == WebRect())
       return;
  int x = (box_rect.x + box_rect.width / 2)*webview()->pageScaleFactor();
  int y = (box_rect.y + box_rect.height / 2)*webview()->pageScaleFactor();
  SetLongPressSelectionPoint( x,  y);
}
#endif

#if defined(S_FP_NEW_TAB_FIX)
void RenderViewImpl::checkforRPPBeforeTabClose(){
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, CheckforRPPBeforeTabClose());
}
#endif

void RenderViewImpl::OnLongPressOnFocused(
                                const blink::WebInputEvent* longpress_event) {
  WebNode node = GetFocusedNode();
  if (node.isNull() || IsEditableNode(node) || IsSelectNode(node))
    return;

  WebRect box_rect = webview()->focusedElementBounds();
  if (box_rect == WebRect())
    return;

  WebGestureEvent* gesture_longpress_event =
    const_cast<WebGestureEvent*>(
      static_cast<const WebGestureEvent*>(longpress_event));
#if defined(S_PLM_P140616_04291)
  gesture_longpress_event->x = (box_rect.x + box_rect.width / 2)*webview()->pageScaleFactor();
  gesture_longpress_event->y = (box_rect.y + box_rect.height / 2)*webview()->pageScaleFactor();
#else
  gesture_longpress_event->x = box_rect.x + box_rect.width / 2;
  gesture_longpress_event->y = box_rect.y + box_rect.height / 2;
#endif
  //Sent to browser for setting last touch point for long press enter key : start
  SetLongPressSelectionPoint(gesture_longpress_event->x,gesture_longpress_event->y);
  //Sent to browser for setting last touch point for long press enter key : end
  OnHandleInputEvent(gesture_longpress_event, ui::LatencyInfo(), false);
}
void RenderViewImpl::OnHoverHighlight(const blink::WebInputEvent* hover_event, bool high_light) {
  const WebGestureEvent& gesture_hover_event =
    *static_cast<const WebGestureEvent*>(hover_event);

  webview()->hoverHighlight(gesture_hover_event, high_light);
}

void RenderViewImpl::OnCaptureRendererContentSnapShot(
    const gfx::Rect& src_subrect,
    float page_scale_factor) {

  if (!PaintContentRectToBitmap(src_subrect, page_scale_factor))
    Send(new ViewHostMsg_Snapshot(routing_id(), false, SkBitmap()));

}
#if defined(S_TRANSPORT_DIB_FOR_SOFT_BITMAP)
bool RenderViewImpl::PaintContentRectToBitmap(const gfx::Rect& src_subrect,
                                              float page_scale_factor) {

  WebView* view = webview();
  if (!view || !view->mainFrame())
    return false;

#if defined (S_PLM_P140507_05160)
  if(view->hasWebGLOr2DCanvasContent()){
    LOG(INFO) << "RenderViewImpl::PaintContentRectToBitmap returning false because page has WebGLContent or 2Dcanvas content in viewport";
    return false;
  }
#endif

  // FIXME:: Because we are taking a software rendered bitmap what if
  // the content is completely hardware accelerated.
  const WebSize& size = view->size();
  if (size.isEmpty())
    return false;
  float deviceScale = view->deviceScaleFactor();
  int height = src_subrect.height();
  WebSize contentSize = view->mainFrame()->contentsSize();
  if (height > contentSize.height)
    height = contentSize.height;

  gfx::Size canvas_size(size.width * deviceScale,
                                                height * deviceScale);

  TransportDIB* transport_dib = NULL;
  scoped_ptr<skia::PlatformCanvas> canvas(
            RenderProcess::current()->GetDrawingCanvas(&transport_dib,
                                                       gfx::Rect(canvas_size)));
   if (!canvas) {
	   LOG(INFO)<<"PaintContentRectToBitmap :: Canvas not created";		
          return false;
   }
		
  
  if (!canvas)
    return false;
  int x = src_subrect.x();
  int y = src_subrect.y();
  if (page_scale_factor == -1) {
/*
#if defined (SBROWSER_SOFTBITMAP_IMPL)  	
    page_scale_factor = view->getPageScaleFactor();
#else
    page_scale_factor = view->pageScaleFactor();
#endif
*/
/*
   //Chandra:: Due to "FrameLoader::restoreScrollPositionAndViewState" page scale factor is coming wrong 
   when we try to get page scale factor  at the time of first layout.
   In this function page scale factor is rest from the history which is not maintained correctly.
   If we disbale this then all cases working fine. But for now due to project critical time we are not going to modify anyhting.
   Hence getting the page scale factor from page always.

   Expected issues: When we try to get soft bitmap at the time of first layout we always render with the page scale available.
   P140804-01714, P140822-07154, P140822-02310
      
   // There is a race condition between the layout and load completion that affects restoring the scroll position.
   // We try to restore the scroll position at both the first layout and upon load completion.
   // 1) If first layout happens before the load completes, we want to restore the scroll position then so that the
   // first time we draw the page is already scrolled to the right place, instead of starting at the top and later
   // jumping down. It is possible that the old scroll position is past the part of the doc laid out so far, in
   // which case the restore silent fails and we will fix it in when we try to restore on doc completion.
   // 2) If the layout happens after the load completes, the attempt to restore at load completion time silently
   // fails. We then successfully restore it when the layout happens.
   //void FrameLoader::restoreScrollPositionAndViewState(RestorePolicy restorePolicy)
*/
    page_scale_factor = view->pageScaleFactor();
    WebSize offset = view->mainFrame()->scrollOffset();
    x = offset.width;
    y = offset.height;
  }
  LOG(INFO)<<"DEBUG :: PaintContentRectToBitmap :: pagescalefactor = "<<page_scale_factor << "and scale = "<<page_scale_factor * deviceScale;
  
  canvas.get()->scale(page_scale_factor * deviceScale, page_scale_factor * deviceScale);
  canvas.get()->translate(-x, -y);
#if defined (S_PLM_P140519_04934)//fix for P140519-04934 
  canvas.get()->drawARGB(255,255,255,255);
#endif
  view->paintSoftBitmap(canvas.get(),
                        WebRect(0, 0, contentSize.width, contentSize.height));
	
#if defined (SBROWSER_SOFTBITMAP_IMPL)					
  if (!view->mainFrame()->isSoftBmpCaptured()) {
     return false;
  }  
#endif
  
  Send(new ViewHostMsg_SnapshotDIB(routing_id(), true,canvas_size,
                                                   transport_dib->id()));
  
  return true;
}
#else
bool RenderViewImpl::PaintContentRectToBitmap(const gfx::Rect& src_subrect,
                                              float page_scale_factor) {

  WebView* view = webview();
  if (!view || !view->mainFrame())
    return false;

#if defined (S_PLM_P140507_05160)
  if(view->hasWebGLOr2DCanvasContent()){
    LOG(INFO) << "RenderViewImpl::PaintContentRectToBitmap returning false because page has WebGLContent or 2Dcanvas content in viewport";
    return false;
  }
#endif

  // FIXME:: Because we are taking a software rendered bitmap what if
  // the content is completely hardware accelerated.
  const WebSize& size = view->size();
  if (size.isEmpty())
    return false;
  float deviceScale = view->deviceScaleFactor();
  int height = src_subrect.height();
  WebSize contentSize = view->mainFrame()->contentsSize();
  if (height > contentSize.height)
    height = contentSize.height;
#if defined(S_SCALING_FOR_SOFT_BITMAP)
  int soft_bitmap_scale_fraction = 1;
  base::StringToInt(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kSoftBitmapScaleFraction), &soft_bitmap_scale_fraction);
  if(deviceScale > 2 && soft_bitmap_scale_fraction > 1)
  	deviceScale /= soft_bitmap_scale_fraction;//Scaling down by half as of now
  else
  	LOG(INFO)<<"PaintContentRectToBitmap :: soft_bitmap_scale_fraction is one, so not doing any scale down";
#endif//S_SCALING_FOR_SOFT_BITMAP
  skia::RefPtr<SkCanvas> canvas =
      skia::AdoptRef(CreatePlatformRGB565Canvas(size.width * deviceScale,
                                                height * deviceScale,
                                                true,
                                                NULL));
  LOG(INFO)<<"PaintContentRectToBitmap :: Canvas created with size.width :: height :: "<<size.width<<" :: "<<height;
  if (!canvas)
    return false;
  int x = src_subrect.x();
  int y = src_subrect.y();
  if (page_scale_factor == -1) {
/*
#if defined (SBROWSER_SOFTBITMAP_IMPL)	  
 	  page_scale_factor = view->getPageScaleFactor();
#else
 	  page_scale_factor = view->pageScaleFactor();
#endif
*/
/*
   //Chandra:: Due to "FrameLoader::restoreScrollPositionAndViewState" page scale factor is coming wrong 
    when we try to get page scale factor  at the time of first layout.
    In this function page scale factor is rest from the history which is not maintained correctly.
    If we disbale this then all cases working fine. But for now due to project critical time we are not going to modify anyhting.
    Hence getting the page scale factor from page always.

    Expected issues: When we try to get soft bitmap at the time of first layout we always render with the page scale available.
    P140804-01714, P140822-07154, P140822-02310
	
   // There is a race condition between the layout and load completion that affects restoring the scroll position.
   // We try to restore the scroll position at both the first layout and upon load completion.
   // 1) If first layout happens before the load completes, we want to restore the scroll position then so that the
   // first time we draw the page is already scrolled to the right place, instead of starting at the top and later
   // jumping down. It is possible that the old scroll position is past the part of the doc laid out so far, in
   // which case the restore silent fails and we will fix it in when we try to restore on doc completion.
   // 2) If the layout happens after the load completes, the attempt to restore at load completion time silently
   // fails. We then successfully restore it when the layout happens.
   //void FrameLoader::restoreScrollPositionAndViewState(RestorePolicy restorePolicy)
*/
    page_scale_factor = view->pageScaleFactor();

    WebSize offset = view->mainFrame()->scrollOffset();
    x = offset.width;
    y = offset.height;
  }
  LOG(INFO)<<"DEBUG :: PaintContentRectToBitmap :: pagescalefactor = "<<page_scale_factor << "and scale = "<<page_scale_factor * deviceScale;
  
  canvas.get()->scale(page_scale_factor * deviceScale, page_scale_factor * deviceScale);
  canvas.get()->translate(-x, -y);
#if defined (S_PLM_P140519_04934)//fix for P140519-04934 
  canvas.get()->drawARGB(255,255,255,255);
#endif
  view->paintSoftBitmap(canvas.get(),
                        WebRect(0, 0, contentSize.width, contentSize.height));
	
#if defined (SBROWSER_SOFTBITMAP_IMPL)					
  if (!view->mainFrame()->isSoftBmpCaptured()) {
     return false;
  }  
#endif

  const SkBitmap& bitmap = skia::GetTopDevice(*canvas)->accessBitmap(false);

/*Fix for PLM P140807-07024 :: Checking for validity of the created bitmap 
 *before sending to Browser process via IPC*/
  if(bitmap.isNull())
     return false;
  LOG(INFO)<<"PaintContentRectToBitmap :: The size of the Bitmap before sending via IPC channel :: "<<bitmap.getSize();
  LOG(INFO)<<"PaintContentRectToBitmap :: The SAFE size of the Bitmap before sending via IPC channel :: "<<bitmap.getSafeSize();
  LOG(INFO)<<"PaintContentRectToBitmap :: TIMESTAMP :: Before Sending";
  Send(new ViewHostMsg_Snapshot(routing_id(), true, bitmap));

  return true;
}
#endif //S_TRANSPORT_DIB_FOR_SOFT_BITMAP



/*Samsung SoftBitmap Update START */
SkCanvas* RenderViewImpl::CreatePlatformRGB565Canvas(int width, int height, bool is_opaque,
                               uint8_t* data) {
  skia::RefPtr<SkBaseDevice> dev = skia::AdoptRef(
                      CreateRGB565Device(width, height, is_opaque, data));
  return CreateCanvas(dev, skia::RETURN_NULL_ON_FAILURE);
}

skia::BitmapPlatformDevice* RenderViewImpl::CreateRGB565Device(int width, int height,
                                                   bool is_opaque,
                                                   uint8_t* data) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kRGB_565_Config, width, height, 0,
                   is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType);
  if (data)
    bitmap.setPixels(data);
  else if (!bitmap.allocPixels())
    return NULL;
  return new skia::BitmapPlatformDevice(bitmap);
}
/*Samsung SoftBitmap Update END */

bool RenderViewImpl::IsValidBitmap(const SkBitmap& bitmap,int width,int height )
{
  int x, y;
  int noOfPixelJump = 50 ;
			
  for(x=0,y=0;(y<(height/2) && x<(width/2));)
  {
  	if(bitmap.getColor(x, y) != 0 
  	|| bitmap.getColor(width - (x+1), y) != 0
  	|| bitmap.getColor(x, height - (y+1)) != 0 
  	|| bitmap.getColor(width - (x+1), height - (y+1)) != 0)
  		return true ;
				
    	x = x + noOfPixelJump ;
	y = y + noOfPixelJump;
  }
  return false;
}

// SBROWSER_HANDLE_MOUSECLICK_CTRL ++
void RenderViewImpl::OnHandleMouseClickWithCtrlkey(int x, int y) {
  webview()->onHandleMouseClickWithCtrlkey(x, y);
}

void RenderViewImpl::openUrlInNewTab(const base::string16& mouseClickUrl) {
  Send(new ViewHostMsg_OpenUrlInNewTab(routing_id_, mouseClickUrl));
}

//Sent to browser for setting last touch point for long press enter key : start
void RenderViewImpl::SetLongPressSelectionPoint(int x, int y) {	
	Send(new ViewHostMsg_SetLongPressSelectionPoint(routing_id_, x, y));
}
//Sent to browser for setting last touch point for long press enter key : end

// SBROWSER_HANDLE_MOUSECLICK_CTRL --

#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
 void RenderViewImpl::autoLoginFailure()
{ 
     Send(new ViewHostMsg_AutoLoginFailure(routing_id_));
}
#endif

#if defined(S_INTUITIVE_HOVER)
void RenderViewImpl::hoverHitTestResult(int contentType) {
  Send(new ViewHostMsg_HoverHitTestResult(routing_id_, contentType));
}
#endif

}  // namespace content
