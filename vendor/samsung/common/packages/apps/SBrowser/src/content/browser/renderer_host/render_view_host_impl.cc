// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/cross_site_request_manager.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/cross_site_transferring_request.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/accessibility_messages.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/speech_recognition_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/touch/touch_enabled.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "webkit/browser/fileapi/isolated_context.h"

#if defined(OS_MACOSX)
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#elif defined(OS_ANDROID)
#include "content/browser/media/android/browser_media_player_manager.h"
#elif defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(S_NATIVE_SUPPORT)
#include "content/public/browser/child_process_security_policy.h"
#endif

using base::TimeDelta;
using blink::WebConsoleMessage;
using blink::WebDragOperation;
using blink::WebDragOperationNone;
using blink::WebDragOperationsMask;
using blink::WebInputEvent;
using blink::WebMediaPlayerAction;
using blink::WebPluginAction;

namespace content {
namespace {

// Delay to wait on closing the WebContents for a beforeunload/unload handler to
// fire.
const int kUnloadTimeoutMS = 1000;

// Translate a WebKit text direction into a base::i18n one.
base::i18n::TextDirection WebTextDirectionToChromeTextDirection(
    blink::WebTextDirection dir) {
  switch (dir) {
    case blink::WebTextDirectionLeftToRight:
      return base::i18n::LEFT_TO_RIGHT;
    case blink::WebTextDirectionRightToLeft:
      return base::i18n::RIGHT_TO_LEFT;
    default:
      NOTREACHED();
      return base::i18n::UNKNOWN_DIRECTION;
  }
}

#if defined(OS_WIN)

const int kVirtualKeyboardDisplayWaitTimeoutMs = 100;
const int kMaxVirtualKeyboardDisplayRetries = 5;

void DismissVirtualKeyboardTask() {
  static int virtual_keyboard_display_retries = 0;
  // If the virtual keyboard is not yet visible, then we execute the task again
  // waiting for it to show up.
  if (!base::win::DismissVirtualKeyboard()) {
    if (virtual_keyboard_display_retries < kMaxVirtualKeyboardDisplayRetries) {
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(base::IgnoreResult(&DismissVirtualKeyboardTask)),
          TimeDelta::FromMilliseconds(kVirtualKeyboardDisplayWaitTimeoutMs));
      ++virtual_keyboard_display_retries;
    } else {
      virtual_keyboard_display_retries = 0;
    }
  }
}
#endif

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, public:

// static
bool RenderViewHostImpl::IsRVHStateActive(RenderViewHostImplState rvh_state) {
  if (rvh_state == STATE_DEFAULT ||
      rvh_state == STATE_WAITING_FOR_UNLOAD_ACK ||
      rvh_state == STATE_WAITING_FOR_COMMIT ||
      rvh_state == STATE_WAITING_FOR_CLOSE)
    return true;
  return false;
}

// static
RenderViewHost* RenderViewHost::FromID(int render_process_id,
                                       int render_view_id) {
  return RenderViewHostImpl::FromID(render_process_id, render_view_id);
}

// static
RenderViewHost* RenderViewHost::From(RenderWidgetHost* rwh) {
  DCHECK(rwh->IsRenderView());
  return static_cast<RenderViewHostImpl*>(RenderWidgetHostImpl::From(rwh));
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, public:

// static
RenderViewHostImpl* RenderViewHostImpl::FromID(int render_process_id,
                                               int render_view_id) {
  RenderWidgetHost* widget =
      RenderWidgetHost::FromID(render_process_id, render_view_id);
  if (!widget || !widget->IsRenderView())
    return NULL;
  return static_cast<RenderViewHostImpl*>(RenderWidgetHostImpl::From(widget));
}

RenderViewHostImpl::RenderViewHostImpl(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int routing_id,
    int main_frame_routing_id,
    bool swapped_out,
    bool hidden)
    : RenderWidgetHostImpl(widget_delegate,
                           instance->GetProcess(),
                           routing_id,
                           hidden),
      frames_ref_count_(0),
      delegate_(delegate),
      instance_(static_cast<SiteInstanceImpl*>(instance)),
      waiting_for_drag_context_response_(false),
      enabled_bindings_(0),
      navigations_suspended_(false),
      has_accessed_initial_document_(false),
      main_frame_id_(-1),
      main_frame_routing_id_(main_frame_routing_id),
      run_modal_reply_msg_(NULL),
      run_modal_opener_id_(MSG_ROUTING_NONE),
      is_waiting_for_beforeunload_ack_(false),
      unload_ack_is_for_cross_site_transition_(false),
      are_javascript_messages_suppressed_(false),
      sudden_termination_allowed_(false),
      render_view_termination_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      virtual_keyboard_requested_(false),
      weak_factory_(this) {
  DCHECK(instance_.get());
  CHECK(delegate_);  // http://crbug.com/82827
#if defined(SBROWSER_QC_OPTIMIZATION_PRECREATE_RENDERER)
  extra_site_instance_ = NULL;
#endif
  GetProcess()->EnableSendQueue();

  if (swapped_out) {
    rvh_state_ = STATE_SWAPPED_OUT;
  } else {
    rvh_state_ = STATE_DEFAULT;
    instance_->increment_active_view_count();
  }

  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostCreated,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(), GetRoutingID()));
  }

#if defined(OS_ANDROID)
  media_player_manager_.reset(BrowserMediaPlayerManager::Create(this));
#endif

  unload_event_monitor_timeout_.reset(new TimeoutMonitor(base::Bind(
      &RenderViewHostImpl::OnSwappedOut, weak_factory_.GetWeakPtr(), true)));
}

RenderViewHostImpl::~RenderViewHostImpl() {
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostDeleted,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(), GetRoutingID()));
  }

  delegate_->RenderViewDeleted(this);

  // Be sure to clean up any leftover state from cross-site requests.
  CrossSiteRequestManager::GetInstance()->SetHasPendingCrossSiteRequest(
      GetProcess()->GetID(), GetRoutingID(), false);

  // If this was swapped out, it already decremented the active view
  // count of the SiteInstance it belongs to.
  if (IsRVHStateActive(rvh_state_))
    instance_->decrement_active_view_count();
}

RenderViewHostDelegate* RenderViewHostImpl::GetDelegate() const {
  return delegate_;
}

SiteInstance* RenderViewHostImpl::GetSiteInstance() const {
  return instance_.get();
}
#if defined(SBROWSER_QC_OPTIMIZATION_PRECREATE_RENDERER)
void RenderViewHostImpl::SetExtraSiteInstance(SiteInstance* extra_site_instance) {
    extra_site_instance_ = extra_site_instance;
}
#endif
bool RenderViewHostImpl::CreateRenderView(
    const base::string16& frame_name,
    int opener_route_id,
    int32 max_page_id) {
  TRACE_EVENT0("renderer_host", "RenderViewHostImpl::CreateRenderView");
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;
  DCHECK(GetProcess()->HasConnection());
  DCHECK(GetProcess()->GetBrowserContext());
  renderer_initialized_ = true;

  GpuSurfaceTracker::Get()->SetSurfaceHandle(
      surface_id(), GetCompositingSurface());

  // Ensure the RenderView starts with a next_page_id larger than any existing
  // page ID it might be asked to render.
  int32 next_page_id = 1;
  if (max_page_id > -1)
    next_page_id = max_page_id + 1;

  ViewMsg_New_Params params;
  params.renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext());
  params.web_preferences = delegate_->GetWebkitPrefs();
  params.view_id = GetRoutingID();
  params.main_frame_routing_id = main_frame_routing_id_;
  params.surface_id = surface_id();
  params.session_storage_namespace_id =
      delegate_->GetSessionStorageNamespace(instance_)->id();
  params.frame_name = frame_name;
  // Ensure the RenderView sets its opener correctly.
  params.opener_route_id = opener_route_id;
  params.swapped_out = !IsRVHStateActive(rvh_state_);
  params.hidden = is_hidden();
  params.next_page_id = next_page_id;
  GetWebScreenInfo(&params.screen_info);
  params.accessibility_mode = accessibility_mode();
  params.allow_partial_swap = !GetProcess()->IsGuest();

  Send(new ViewMsg_New(params));

  // If it's enabled, tell the renderer to set up the Javascript bindings for
  // sending messages back to the browser.
  if (GetProcess()->IsGuest())
    DCHECK_EQ(0, enabled_bindings_);
  Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
  // Let our delegate know that we created a RenderView.
  delegate_->RenderViewCreated(this);

  return true;
}

bool RenderViewHostImpl::IsRenderViewLive() const {
  return GetProcess()->HasConnection() && renderer_initialized_;
}

void RenderViewHostImpl::SyncRendererPrefs() {
  Send(new ViewMsg_SetRendererPrefs(GetRoutingID(),
                                    delegate_->GetRendererPrefs(
                                        GetProcess()->GetBrowserContext())));
}

WebPreferences RenderViewHostImpl::GetWebkitPrefs(const GURL& url) {
  TRACE_EVENT0("browser", "RenderViewHostImpl::GetWebkitPrefs");
  WebPreferences prefs;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  prefs.javascript_enabled =
      !command_line.HasSwitch(switches::kDisableJavaScript);
  prefs.web_security_enabled =
      !command_line.HasSwitch(switches::kDisableWebSecurity);
  prefs.plugins_enabled =
      !command_line.HasSwitch(switches::kDisablePlugins);
  prefs.java_enabled =
      !command_line.HasSwitch(switches::kDisableJava);

  prefs.remote_fonts_enabled =
      !command_line.HasSwitch(switches::kDisableRemoteFonts);
  prefs.xslt_enabled =
      !command_line.HasSwitch(switches::kDisableXSLT);
  prefs.xss_auditor_enabled =
      !command_line.HasSwitch(switches::kDisableXSSAuditor);
  prefs.application_cache_enabled =
      !command_line.HasSwitch(switches::kDisableApplicationCache);

  prefs.local_storage_enabled =
      !command_line.HasSwitch(switches::kDisableLocalStorage);
  prefs.databases_enabled =
      !command_line.HasSwitch(switches::kDisableDatabases);
#if defined(OS_ANDROID) && defined(ARCH_CPU_X86)
  prefs.webaudio_enabled =
      command_line.HasSwitch(switches::kEnableWebAudio);
#else
  prefs.webaudio_enabled =
      !command_line.HasSwitch(switches::kDisableWebAudio);
#endif

  prefs.experimental_webgl_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisable3DAPIs) &&
      !command_line.HasSwitch(switches::kDisableExperimentalWebGL);

  prefs.pepper_3d_enabled =
      !command_line.HasSwitch(switches::kDisablePepper3d);

  prefs.flash_3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlash3d);
  prefs.flash_stage3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);
  prefs.flash_stage3d_baseline_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);

  prefs.gl_multisampling_enabled =
      !command_line.HasSwitch(switches::kDisableGLMultisampling);
  prefs.privileged_webgl_extensions_enabled =
      command_line.HasSwitch(switches::kEnablePrivilegedWebGLExtensions);
  prefs.site_specific_quirks_enabled =
      !command_line.HasSwitch(switches::kDisableSiteSpecificQuirks);
  prefs.allow_file_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowFileAccessFromFiles);

  prefs.layer_squashing_enabled = false;
  if (command_line.HasSwitch(switches::kEnableLayerSquashing))
      prefs.layer_squashing_enabled = true;
  if (command_line.HasSwitch(switches::kDisableLayerSquashing))
      prefs.layer_squashing_enabled = false;

  prefs.show_paint_rects =
      command_line.HasSwitch(switches::kShowPaintRects);
  prefs.accelerated_compositing_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableAcceleratedCompositing);
  prefs.force_compositing_mode =
      content::IsForceCompositingModeEnabled() &&
      !command_line.HasSwitch(switches::kDisableForceCompositingMode);
  prefs.accelerated_2d_canvas_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  prefs.antialiased_2d_canvas_disabled =
      command_line.HasSwitch(switches::kDisable2dCanvasAntialiasing);
  prefs.accelerated_2d_canvas_msaa_sample_count =
      atoi(command_line.GetSwitchValueASCII(
      switches::kAcceleratedCanvas2dMSAASampleCount).c_str());
  prefs.accelerated_filters_enabled =
      GpuProcessHost::gpu_enabled() &&
      command_line.HasSwitch(switches::kEnableAcceleratedFilters);
  prefs.deferred_filters_enabled =
      command_line.HasSwitch(switches::kEnableDeferredFilters);
  prefs.accelerated_compositing_for_3d_transforms_enabled =
      prefs.accelerated_compositing_for_animation_enabled =
          !command_line.HasSwitch(switches::kDisableAcceleratedLayers);
  prefs.accelerated_compositing_for_plugins_enabled =
      !command_line.HasSwitch(switches::kDisableAcceleratedPlugins);
  prefs.accelerated_compositing_for_video_enabled =
      !command_line.HasSwitch(switches::kDisableAcceleratedVideo);
  prefs.fullscreen_enabled =
      !command_line.HasSwitch(switches::kDisableFullScreen);
  prefs.lazy_layout_enabled =
      command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  prefs.region_based_columns_enabled =
      command_line.HasSwitch(switches::kEnableRegionBasedColumns);
  prefs.threaded_html_parser =
      !command_line.HasSwitch(switches::kDisableThreadedHTMLParser);
  prefs.experimental_websocket_enabled =
      command_line.HasSwitch(switches::kEnableExperimentalWebSocket);
  if (command_line.HasSwitch(cc::switches::kEnablePinchVirtualViewport)) {
    prefs.pinch_virtual_viewport_enabled = true;
    prefs.pinch_overlay_scrollbar_thickness = 10;
  }
  prefs.use_solid_color_scrollbars = IsOverlayScrollbarEnabled();

#if defined(OS_ANDROID)
  prefs.user_gesture_required_for_media_playback = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForMediaPlayback);
  prefs.user_gesture_required_for_media_fullscreen = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForMediaFullscreen);
#endif

  prefs.touch_enabled = ui::AreTouchEventsEnabled();
  prefs.device_supports_touch = prefs.touch_enabled &&
      ui::IsTouchDevicePresent();
#if defined(OS_ANDROID)
  prefs.device_supports_mouse = false;
#endif

  prefs.pointer_events_max_touch_points = ui::MaxTouchPoints();

  prefs.touch_adjustment_enabled =
      !command_line.HasSwitch(switches::kDisableTouchAdjustment);
  prefs.compositor_touch_hit_testing =
      !command_line.HasSwitch(cc::switches::kDisableCompositorTouchHitTesting);

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  bool default_enable_scroll_animator = true;
#else
  bool default_enable_scroll_animator = false;
#endif
  prefs.enable_scroll_animator = default_enable_scroll_animator;
  if (command_line.HasSwitch(switches::kEnableSmoothScrolling))
    prefs.enable_scroll_animator = true;
  if (command_line.HasSwitch(switches::kDisableSmoothScrolling))
    prefs.enable_scroll_animator = false;

  prefs.visual_word_movement_enabled =
      command_line.HasSwitch(switches::kEnableVisualWordMovement);

  // Certain GPU features might have been blacklisted.
  GpuDataManagerImpl::GetInstance()->UpdateRendererWebPrefs(&prefs);

  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    prefs.loads_images_automatically = true;
    prefs.javascript_enabled = true;
  }

  prefs.is_online = !net::NetworkChangeNotifier::IsOffline();

#if !defined(USE_AURA)
  // Force accelerated compositing and 2d canvas off for chrome: and about:
  // pages (unless it's specifically allowed).
  if ((url.SchemeIs(kChromeUIScheme) ||
       (url.SchemeIs(chrome::kAboutScheme) && url.spec() != kAboutBlankURL)) &&
      !command_line.HasSwitch(switches::kAllowWebUICompositing)) {
    prefs.accelerated_compositing_enabled = false;
    prefs.accelerated_2d_canvas_enabled = false;
  }
#endif

  prefs.fixed_position_creates_stacking_context = !command_line.HasSwitch(
      switches::kDisableFixedPositionCreatesStackingContext);

#if defined(OS_CHROMEOS)
  prefs.gesture_tap_highlight_enabled = !command_line.HasSwitch(
      switches::kDisableGestureTapHighlight);
#else
  prefs.gesture_tap_highlight_enabled = command_line.HasSwitch(
      switches::kEnableGestureTapHighlight);
#endif

  prefs.number_of_cpu_cores = base::SysInfo::NumberOfProcessors();

  prefs.viewport_meta_enabled =
      command_line.HasSwitch(switches::kEnableViewportMeta);

  prefs.viewport_enabled =
      command_line.HasSwitch(switches::kEnableViewport) ||
      prefs.viewport_meta_enabled;

  prefs.main_frame_resizes_are_orientation_changes =
      command_line.HasSwitch(switches::kMainFrameResizesAreOrientationChanges);

  prefs.deferred_image_decoding_enabled =
      command_line.HasSwitch(switches::kEnableDeferredImageDecoding) ||
      cc::switches::IsImplSidePaintingEnabled();

  prefs.spatial_navigation_enabled = command_line.HasSwitch(
      switches::kEnableSpatialNavigation);

  GetContentClient()->browser()->OverrideWebkitPrefs(this, url, &prefs);

  // Disable compositing in guests until we have compositing path implemented
  // for guests.
  bool guest_compositing_enabled = !command_line.HasSwitch(
      switches::kDisableBrowserPluginCompositing);
  if (GetProcess()->IsGuest() && !guest_compositing_enabled) {
    prefs.force_compositing_mode = false;
    prefs.accelerated_compositing_enabled = false;
  }

  return prefs;
}

void RenderViewHostImpl::Navigate(const FrameMsg_Navigate_Params& params) {
  TRACE_EVENT0("renderer_host", "RenderViewHostImpl::Navigate");
  delegate_->GetFrameTree()->GetMainFrame()->Navigate(params);
}

void RenderViewHostImpl::NavigateToURL(const GURL& url) {
  delegate_->GetFrameTree()->GetMainFrame()->NavigateToURL(url);
}
void RenderViewHostImpl::moveToNextInput() {
#if defined(S_NATIVE_SUPPORT) //SBROWSER_FORM_NAVIGATION
  Send(new ViewMsg_MoveToNextTextOrSelectElement(GetRoutingID()));
#endif
}

void RenderViewHostImpl::moveToPrevInput() {
#if defined(S_NATIVE_SUPPORT) //SBROWSER_FORM_NAVIGATION
  Send(new ViewMsg_MoveToPreviousTextOrSelectElement(GetRoutingID()));
#endif
}
void RenderViewHostImpl::SetNavigationsSuspended(
    bool suspend,
    const base::TimeTicks& proceed_time) {
  // This should only be called to toggle the state.
  DCHECK(navigations_suspended_ != suspend);

  navigations_suspended_ = suspend;
  if (!suspend && suspended_nav_params_) {
    // There's navigation message params waiting to be sent.  Now that we're not
    // suspended anymore, resume navigation by sending them.  If we were swapped
    // out, we should also stop filtering out the IPC messages now.
    SetState(STATE_DEFAULT);

    DCHECK(!proceed_time.is_null());
    suspended_nav_params_->browser_navigation_start = proceed_time;
    LOG(INFO) << "[SBRCHECK_LU] RenderViewHostImpl::SetNavigationsSuspended: Send IPC [LoadURL] [BrowserProcess] [RoutingID=" << GetRoutingID() << "]";
    Send(new FrameMsg_Navigate(
        main_frame_routing_id_, *suspended_nav_params_.get()));
    suspended_nav_params_.reset();
  }
}

void RenderViewHostImpl::CancelSuspendedNavigations() {
  // Clear any state if a pending navigation is canceled or pre-empted.
  if (suspended_nav_params_)
    suspended_nav_params_.reset();
  navigations_suspended_ = false;
}

void RenderViewHostImpl::FirePageBeforeUnload(bool for_cross_site_transition) {
  if (!IsRenderViewLive()) {
    // This RenderViewHostImpl doesn't have a live renderer, so just
    // skip running the onbeforeunload handler.
    is_waiting_for_beforeunload_ack_ = true;  // Checked by OnShouldCloseACK.
    unload_ack_is_for_cross_site_transition_ = for_cross_site_transition;
    base::TimeTicks now = base::TimeTicks::Now();
    OnShouldCloseACK(true, now, now);
    return;
  }

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if she clicks the tab close button then the browser close
  // button), and we only send the message once.
  if (is_waiting_for_beforeunload_ack_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    unload_ack_is_for_cross_site_transition_ =
        unload_ack_is_for_cross_site_transition_ && for_cross_site_transition;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_beforeunload_ack_ = true;
    unload_ack_is_for_cross_site_transition_ = for_cross_site_transition;
    // Increment the in-flight event count, to ensure that input events won't
    // cancel the timeout timer.
    increment_in_flight_event_count();
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));
    send_should_close_start_time_ = base::TimeTicks::Now();
    Send(new ViewMsg_ShouldClose(GetRoutingID()));
  }
}

void RenderViewHostImpl::OnCrossSiteResponse(
    const GlobalRequestID& global_request_id,
    scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    PageTransition page_transition,
    int64 frame_id,
    bool should_replace_current_entry) {
  FrameTreeNode* node = NULL;
  if (frame_id != -1 &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess)) {
    node = delegate_->GetFrameTree()->FindByFrameID(frame_id);
  }

  // TODO(creis): We should always be able to get the RFHM for a frame_id,
  // but today the frame_id is -1 for the main frame.
  RenderViewHostDelegate::RendererManagement* manager = node ?
      node->render_manager() : delegate_->GetRendererManagementDelegate();
  manager->OnCrossSiteResponse(this, global_request_id,
                               cross_site_transferring_request.Pass(),
                               transfer_url_chain, referrer, page_transition,
                               frame_id, should_replace_current_entry);
}

void RenderViewHostImpl::SuppressDialogsUntilSwapOut() {
  Send(new ViewMsg_SuppressDialogsUntilSwapOut(GetRoutingID()));
}

void RenderViewHostImpl::SwapOut() {
  SetState(STATE_WAITING_FOR_UNLOAD_ACK);
  unload_event_monitor_timeout_->Start(
      base::TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  if (IsRenderViewLive()) {
    Send(new ViewMsg_SwapOut(GetRoutingID()));
  }
  delegate_->SwappedOut(this);
}

void RenderViewHostImpl::OnSwapOutACK() {
  OnSwappedOut(false);
}

void RenderViewHostImpl::OnSwappedOut(bool timed_out) {
  // Ignore spurious swap out ack.
  if (!IsWaitingForUnloadACK())
    return;
  unload_event_monitor_timeout_->Stop();
  if (timed_out) {
    base::ProcessHandle process_handle = GetProcess()->GetHandle();
    int views = 0;

    // Count the number of active widget hosts for the process, which
    // is equivalent to views using the process as of this writing.
    scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
    while (RenderWidgetHost* widget = widgets->GetNextHost()) {
      if (widget->GetProcess()->GetID() == GetProcess()->GetID())
        ++views;
    }

    if (!RenderProcessHost::run_renderer_in_process() &&
        process_handle && views <= 1) {
      // The process can safely be terminated, only if WebContents sets
      // SuddenTerminationAllowed, which indicates that the timer has expired.
      // This is not the case if we load data URLs or about:blank. The reason
      // is that those have no network requests and this code is hit without
      // setting the unresponsiveness timer. This allows a corner case where a
      // navigation to a data URL will leave a process running, if the
      // beforeunload handler completes fine, but the unload handler hangs.
      // At this time, the complexity to solve this edge case is not worthwhile.
      if (SuddenTerminationAllowed()) {
        // We should kill the process, but for now, just log the data so we can
        // diagnose the kill rate and investigate if separate timer is needed.
        // http://crbug.com/104346.

        // Log a histogram point to help us diagnose how many of those kills
        // we have performed. 1 is the enum value for RendererType Normal for
        // the histogram.
        UMA_HISTOGRAM_PERCENTAGE(
            "BrowserRenderProcessHost.ChildKillsUnresponsive", 1);
      }
    }
  }

  switch (rvh_state_) {
    case STATE_WAITING_FOR_UNLOAD_ACK:
      SetState(STATE_WAITING_FOR_COMMIT);
      break;
    case STATE_PENDING_SWAP_OUT:
      SetState(STATE_SWAPPED_OUT);
      break;
    case STATE_PENDING_SHUTDOWN:
      DCHECK(!pending_shutdown_on_swap_out_.is_null());
      pending_shutdown_on_swap_out_.Run();
      break;
    default:
      NOTREACHED();
  }
}

void RenderViewHostImpl::WasSwappedOut(
    const base::Closure& pending_delete_on_swap_out) {
  Send(new ViewMsg_WasSwappedOut(GetRoutingID()));
  if (rvh_state_ == STATE_WAITING_FOR_UNLOAD_ACK) {
    if (instance_->active_view_count())
      SetState(STATE_PENDING_SWAP_OUT);
    else
      SetPendingShutdown(pending_delete_on_swap_out);
  } else if (rvh_state_ == STATE_WAITING_FOR_COMMIT) {
    SetState(STATE_SWAPPED_OUT);
  } else if (rvh_state_ == STATE_DEFAULT) {
    // When the RenderView is not live, the RenderFrameHostManager will call
    // CommitPending directly, without calling SwapOut on the old RVH. This will
    // cause WasSwappedOut to be called directly on the live old RVH.
    DCHECK(!IsRenderViewLive());
    SetState(STATE_SWAPPED_OUT);
  } else {
    NOTREACHED();
  }
}

void RenderViewHostImpl::SetPendingShutdown(const base::Closure& on_swap_out) {
  pending_shutdown_on_swap_out_ = on_swap_out;
  SetState(STATE_PENDING_SHUTDOWN);
}

void RenderViewHostImpl::ClosePage() {
  SetState(STATE_WAITING_FOR_CLOSE);
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  if (IsRenderViewLive()) {
    // Since we are sending an IPC message to the renderer, increase the event
    // count to prevent the hang monitor timeout from being stopped by input
    // event acknowledgements.
    increment_in_flight_event_count();

    // TODO(creis): Should this be moved to Shutdown?  It may not be called for
    // RenderViewHosts that have been swapped out.
    NotificationService::current()->Notify(
        NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
        Source<RenderViewHost>(this),
        NotificationService::NoDetails());

    Send(new ViewMsg_ClosePage(GetRoutingID()));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the unload
    // event and close the page.
    ClosePageIgnoringUnloadEvents();
  }
}

void RenderViewHostImpl::ClosePageIgnoringUnloadEvents() {
  StopHangMonitorTimeout();
  is_waiting_for_beforeunload_ack_ = false;

  sudden_termination_allowed_ = true;
  delegate_->Close(this);
}

bool RenderViewHostImpl::HasPendingCrossSiteRequest() {
  return CrossSiteRequestManager::GetInstance()->HasPendingCrossSiteRequest(
      GetProcess()->GetID(), GetRoutingID());
}

void RenderViewHostImpl::SetHasPendingCrossSiteRequest(
    bool has_pending_request) {
  CrossSiteRequestManager::GetInstance()->SetHasPendingCrossSiteRequest(
      GetProcess()->GetID(), GetRoutingID(), has_pending_request);
}

#if defined(OS_ANDROID)
void RenderViewHostImpl::ActivateNearestFindResult(int request_id,
                                                   float x,
                                                   float y) {
  Send(new InputMsg_ActivateNearestFindResult(GetRoutingID(),
                                              request_id, x, y));
}

void RenderViewHostImpl::RequestFindMatchRects(int current_version) {
  Send(new ViewMsg_FindMatchRects(GetRoutingID(), current_version));
}

void RenderViewHostImpl::DisableFullscreenEncryptedMediaPlayback() {
  media_player_manager_->DisableFullscreenEncryptedMediaPlayback();
}
#endif

void RenderViewHostImpl::DragTargetDragEnter(
    const DropData& drop_data,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed,
    int key_modifiers) {
  const int renderer_id = GetProcess()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // The URL could have been cobbled together from any highlighted text string,
  // and can't be interpreted as a capability.
  DropData filtered_data(drop_data);
  GetProcess()->FilterURL(true, &filtered_data.url);

  // The filenames vector, on the other hand, does represent a capability to
  // access the given files.
  fileapi::IsolatedContext::FileInfoSet files;
  for (std::vector<DropData::FileInfo>::iterator iter(
           filtered_data.filenames.begin());
       iter != filtered_data.filenames.end(); ++iter) {
    // A dragged file may wind up as the value of an input element, or it
    // may be used as the target of a navigation instead.  We don't know
    // which will happen at this point, so generously grant both access
    // and request permissions to the specific file to cover both cases.
    // We do not give it the permission to request all file:// URLs.
    base::FilePath path =
        base::FilePath::FromUTF8Unsafe(base::UTF16ToUTF8(iter->path));

    // Make sure we have the same display_name as the one we register.
    if (iter->display_name.empty()) {
      std::string name;
      files.AddPath(path, &name);
      iter->display_name = base::UTF8ToUTF16(name);
    } else {
      files.AddPathWithName(path, base::UTF16ToUTF8(iter->display_name));
    }

    policy->GrantRequestSpecificFileURL(renderer_id,
                                        net::FilePathToFileURL(path));

    // If the renderer already has permission to read these paths, we don't need
    // to re-grant them. This prevents problems with DnD for files in the CrOS
    // file manager--the file manager already had read/write access to those
    // directories, but dragging a file would cause the read/write access to be
    // overwritten with read-only access, making them impossible to delete or
    // rename until the renderer was killed.
    if (!policy->CanReadFile(renderer_id, path))
      policy->GrantReadFile(renderer_id, path);
  }

  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  std::string filesystem_id = isolated_context->RegisterDraggedFileSystem(
      files);
  if (!filesystem_id.empty()) {
    // Grant the permission iff the ID is valid.
    policy->GrantReadFileSystem(renderer_id, filesystem_id);
  }
  filtered_data.filesystem_id = base::UTF8ToUTF16(filesystem_id);

  Send(new DragMsg_TargetDragEnter(GetRoutingID(), filtered_data, client_pt,
                                   screen_pt, operations_allowed,
                                   key_modifiers));
}

void RenderViewHostImpl::DragTargetDragOver(
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed,
    int key_modifiers) {
  Send(new DragMsg_TargetDragOver(GetRoutingID(), client_pt, screen_pt,
                                  operations_allowed, key_modifiers));
}

void RenderViewHostImpl::DragTargetDragLeave() {
  Send(new DragMsg_TargetDragLeave(GetRoutingID()));
}

void RenderViewHostImpl::DragTargetDrop(
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    int key_modifiers) {
  Send(new DragMsg_TargetDrop(GetRoutingID(), client_pt, screen_pt,
                              key_modifiers));
}

void RenderViewHostImpl::DesktopNotificationPermissionRequestDone(
    int callback_context) {
  Send(new DesktopNotificationMsg_PermissionRequestDone(
      GetRoutingID(), callback_context));
}

void RenderViewHostImpl::DesktopNotificationPostDisplay(int callback_context) {
  Send(new DesktopNotificationMsg_PostDisplay(GetRoutingID(),
                                              callback_context));
}

void RenderViewHostImpl::DesktopNotificationPostError(
    int notification_id,
    const base::string16& message) {
  Send(new DesktopNotificationMsg_PostError(
      GetRoutingID(), notification_id, message));
}

void RenderViewHostImpl::DesktopNotificationPostClose(int notification_id,
                                                      bool by_user) {
  Send(new DesktopNotificationMsg_PostClose(
      GetRoutingID(), notification_id, by_user));
}

void RenderViewHostImpl::DesktopNotificationPostClick(int notification_id) {
  Send(new DesktopNotificationMsg_PostClick(GetRoutingID(), notification_id));
}

void RenderViewHostImpl::ExecuteJavascriptInWebFrame(
    const base::string16& frame_xpath,
    const base::string16& jscript) {
  Send(new ViewMsg_ScriptEvalRequest(GetRoutingID(), frame_xpath, jscript,
                                     0, false));
}

#if defined(S_SCROLL_EVENT)
void RenderViewHostImpl::OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect)
{
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (!view)
      return;
  view->OnTextFieldBoundsChanged(input_edit_rect);
}
#endif
void RenderViewHostImpl::ExecuteJavascriptInWebFrameCallbackResult(
     const base::string16& frame_xpath,
     const base::string16& jscript,
     const JavascriptResultCallback& callback) {
  static int next_id = 1;
  int key = next_id++;
  Send(new ViewMsg_ScriptEvalRequest(GetRoutingID(), frame_xpath, jscript,
                                     key, true));
  javascript_callbacks_.insert(std::make_pair(key, callback));
}

void RenderViewHostImpl::JavaScriptDialogClosed(
    IPC::Message* reply_msg,
    bool success,
    const base::string16& user_input) {
  GetProcess()->SetIgnoreInputEvents(false);
  bool is_waiting = is_waiting_for_beforeunload_ack_ || IsWaitingForUnloadACK();

  // If we are executing as part of (before)unload event handling, we don't
  // want to use the regular hung_renderer_delay_ms_ if the user has agreed to
  // leave the current page. In this case, use the regular timeout value used
  // during the (before)unload handling.
  if (is_waiting) {
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(
        success ? kUnloadTimeoutMS : hung_renderer_delay_ms_));
  }

  ViewHostMsg_RunJavaScriptMessage::WriteReplyParams(reply_msg,
                                                     success, user_input);
  Send(reply_msg);

  // If we are waiting for an unload or beforeunload ack and the user has
  // suppressed messages, kill the tab immediately; a page that's spamming
  // alerts in onbeforeunload is presumably malicious, so there's no point in
  // continuing to run its script and dragging out the process.
  // This must be done after sending the reply since RenderView can't close
  // correctly while waiting for a response.
  if (is_waiting && are_javascript_messages_suppressed_)
    delegate_->RendererUnresponsive(
        this, is_waiting_for_beforeunload_ack_, IsWaitingForUnloadACK());
}

void RenderViewHostImpl::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y,
    WebDragOperation operation) {
  Send(new DragMsg_SourceEndedOrMoved(
      GetRoutingID(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      true, operation));
}

void RenderViewHostImpl::DragSourceMovedTo(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new DragMsg_SourceEndedOrMoved(
      GetRoutingID(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      false, WebDragOperationNone));
}

void RenderViewHostImpl::DragSourceSystemDragEnded() {
  Send(new DragMsg_SourceSystemDragEnded(GetRoutingID()));
}

RenderFrameHost* RenderViewHostImpl::GetMainFrame() {
  return RenderFrameHost::FromID(GetProcess()->GetID(), main_frame_routing_id_);
}

void RenderViewHostImpl::AllowBindings(int bindings_flags) {
  // Never grant any bindings to browser plugin guests.
  if (GetProcess()->IsGuest()) {
    NOTREACHED() << "Never grant bindings to a guest process.";
    return;
  }

  // Ensure we aren't granting WebUI bindings to a process that has already
  // been used for non-privileged views.
  if (bindings_flags & BINDINGS_POLICY_WEB_UI &&
      GetProcess()->HasConnection() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    // This process has no bindings yet. Make sure it does not have more
    // than this single active view.
    RenderProcessHostImpl* process =
        static_cast<RenderProcessHostImpl*>(GetProcess());
    if (process->GetActiveViewCount() > 1)
      return;
  }

  if (bindings_flags & BINDINGS_POLICY_WEB_UI) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        GetProcess()->GetID());
  }

  enabled_bindings_ |= bindings_flags;
  if (renderer_initialized_)
    Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
}

int RenderViewHostImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

void RenderViewHostImpl::SetWebUIProperty(const std::string& name,
                                          const std::string& value) {
  // This is a sanity check before telling the renderer to enable the property.
  // It could lie and send the corresponding IPC messages anyway, but we will
  // not act on them if enabled_bindings_ doesn't agree. If we get here without
  // WebUI bindings, kill the renderer process.
  if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI) {
    Send(new ViewMsg_SetWebUIProperty(GetRoutingID(), name, value));
  } else {
    RecordAction(
        base::UserMetricsAction("BindingsMismatchTerminate_RVH_WebUI"));
    base::KillProcess(
        GetProcess()->GetHandle(), content::RESULT_CODE_KILLED, false);
  }
}

void RenderViewHostImpl::GotFocus() {
  RenderWidgetHostImpl::GotFocus();  // Notifies the renderer it got focus.

  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->GotFocus();
}

void RenderViewHostImpl::LostCapture() {
  RenderWidgetHostImpl::LostCapture();
  delegate_->LostCapture();
}

void RenderViewHostImpl::LostMouseLock() {
  RenderWidgetHostImpl::LostMouseLock();
  delegate_->LostMouseLock();
}

void RenderViewHostImpl::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(GetRoutingID(), reverse));
}

void RenderViewHostImpl::FilesSelectedInChooser(
    const std::vector<ui::SelectedFileInfo>& files,
    FileChooserParams::Mode permissions) {
  // Grant the security access requested to the given files.
  for (size_t i = 0; i < files.size(); ++i) {
    const ui::SelectedFileInfo& file = files[i];
    if (permissions == FileChooserParams::Save) {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantCreateReadWriteFile(
          GetProcess()->GetID(), file.local_path);
    } else {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
          GetProcess()->GetID(), file.local_path);
    }
  }
  Send(new ViewMsg_RunFileChooserResponse(GetRoutingID(), files));
}

void RenderViewHostImpl::DirectoryEnumerationFinished(
    int request_id,
    const std::vector<base::FilePath>& files) {
  // Grant the security access requested to the given files.
  for (std::vector<base::FilePath>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        GetProcess()->GetID(), *file);
  }
  Send(new ViewMsg_EnumerateDirectoryResponse(GetRoutingID(),
                                              request_id,
                                              files));
}

void RenderViewHostImpl::LoadStateChanged(
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64 upload_position,
    uint64 upload_size) {
  delegate_->LoadStateChanged(url, load_state, upload_position, upload_size);
}

bool RenderViewHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_ ||
      GetProcess()->SuddenTerminationAllowed();
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, IPC message handlers:

bool RenderViewHostImpl::OnMessageReceived(const IPC::Message& msg) {
  if (!BrowserMessageFilter::CheckCanDispatchOnUI(msg, this))
    return true;

  // Filter out most IPC messages if this renderer is swapped out.
  // We still want to handle certain ACKs to keep our state consistent.
  if (IsSwappedOut()) {
    if (!SwappedOutMessages::CanHandleWhileSwappedOut(msg)) {
      // If this is a synchronous message and we decided not to handle it,
      // we must send an error reply, or else the renderer will be stuck
      // and won't respond to future requests.
      if (msg.is_sync()) {
        IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
        reply->set_reply_error();
        Send(reply);
      }
      // Don't continue looking for someone to handle it.
      return true;
    }
  }

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderViewHostImpl, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowFullscreenWidget,
                        OnShowFullscreenWidget)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunModal, OnRunModal)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateState, OnUpdateState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTitle, OnUpdateTitle)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateEncoding, OnUpdateEncoding)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnUpdateTargetURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateInspectorSetting,
                        OnUpdateInspectorSetting)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeLoadProgress,
                        OnDidChangeLoadProgress)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDisownOpener, OnDidDisownOpener)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentAvailableInMainFrame,
                        OnDocumentAvailableInMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentOnLoadCompletedInMainFrame,
                        OnDocumentOnLoadCompletedInMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ToggleFullscreen, OnToggleFullscreen)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidContentsPreferredSizeChange,
                        OnDidContentsPreferredSizeChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeScrollOffset,
                        OnDidChangeScrollOffset)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeScrollbarsForMainFrame,
                        OnDidChangeScrollbarsForMainFrame)
#if defined(S_SCROLL_EVENT)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTextFieldBounds, 
                        OnTextFieldBoundsChanged)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeScrollOffsetPinningForMainFrame,
                        OnDidChangeScrollOffsetPinningForMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeNumWheelEvents,
                        OnDidChangeNumWheelEvents)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RouteCloseEvent,
                        OnRouteCloseEvent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RouteMessageEvent, OnRouteMessageEvent)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunJavaScriptMessage,
                                    OnRunJavaScriptMessage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunBeforeUnloadConfirm,
                                    OnRunBeforeUnloadConfirm)
    IPC_MESSAGE_HANDLER(DragHostMsg_StartDragging, OnStartDragging)
    IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(DragHostMsg_TargetDrop_ACK, OnTargetDropACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetBingAsCurrentSearchDefault,
                        OnSetBingAsCurrentSearchDefault)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeChanged, OnFocusedNodeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShouldClose_ACK, OnShouldCloseACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionChanged, OnSelectionChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionBoundsChanged,
                        OnSelectionBoundsChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ScriptEvalResponse, OnScriptEvalResponse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_RequestPermission,
                        OnRequestDesktopNotificationPermission)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_Show,
                        OnShowDesktopNotification)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_Cancel,
                        OnCancelDesktopNotification)
#if defined(OS_MACOSX) || defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowPopup, OnShowPopup)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_RunFileChooser, OnRunFileChooser)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidAccessInitialDocument,
                        OnDidAccessInitialDocument)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DomOperationResponse,
                        OnDomOperationResponse)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_Events, OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_LocationChanges,
                        OnAccessibilityLocationChanges)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeTouched, OnFocusedNodeTouched)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnGetBitmapFromCachedResource,
                        OnReceiveBitmapFromCache)

    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectedMarkup, OnSelectedMarkup)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionVisibilityStatusReceived,
                        OnSelectionVisibilityStatusReceived)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateSelectionRect, OnUpdateSelectionRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PointOnRegion, OnPointOnRegion)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectedBitmap, OnSelectedBitmap)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenUrlInNewTab, OnOpenUrlInNewTab)
	IPC_MESSAGE_HANDLER(ViewHostMsg_SetLongPressSelectionPoint, SetLongPressSelectionPoint)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTouchedFixedElementHeight,OnUpdateTouchedFixedElementHeight)

    #if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AutoLoginFailure, OnAutoLoginFailure)
    #endif
    
#if defined(S_INTUITIVE_HOVER)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HoverHitTestResult, OnHoverHitTestResult)
#endif
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnSSRMModeCallback, OnSSRMModeCallback)
#endif
#if defined(S_NOTIFY_ROTATE_STATUS)
    IPC_MESSAGE_HANDLER(ViewHostMsg_NotifyRotateStatus, OnNotifyRotateStatus)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnRecognizeArticleResult,
                        OnRecognizeArticleResult)
    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(
        handled = RenderWidgetHostImpl::OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message had a handler, but its de-serialization failed.
    // Kill the renderer.
    RecordAction(base::UserMetricsAction("BadMessageTerminate_RVH"));
    GetProcess()->ReceivedBadMessage();
  }

  return handled;
}

void RenderViewHostImpl::Init() {
  RenderWidgetHostImpl::Init();
}

void RenderViewHostImpl::Shutdown() {
  // If we are being run modally (see RunModal), then we need to cleanup.
  if (run_modal_reply_msg_) {
    Send(run_modal_reply_msg_);
    run_modal_reply_msg_ = NULL;
    RenderViewHostImpl* opener =
        RenderViewHostImpl::FromID(GetProcess()->GetID(), run_modal_opener_id_);
    if (opener) {
      opener->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(
          hung_renderer_delay_ms_));
      // Balance out the decrement when we got created.
      opener->increment_in_flight_event_count();
    }
    run_modal_opener_id_ = MSG_ROUTING_NONE;
  }

  RenderWidgetHostImpl::Shutdown();
}

bool RenderViewHostImpl::IsRenderView() const {
  return true;
}

void RenderViewHostImpl::CreateNewWindow(
    int route_id,
    int main_frame_route_id,
    const ViewHostMsg_CreateWindow_Params& params,
    SessionStorageNamespace* session_storage_namespace) {
  ViewHostMsg_CreateWindow_Params validated_params(params);
  GetProcess()->FilterURL(false, &validated_params.target_url);
  GetProcess()->FilterURL(false, &validated_params.opener_url);
  GetProcess()->FilterURL(true, &validated_params.opener_security_origin);

  delegate_->CreateNewWindow(
      GetProcess()->GetID(), route_id, main_frame_route_id, validated_params,
      session_storage_namespace);
}

void RenderViewHostImpl::CreateNewWidget(int route_id,
                                     blink::WebPopupType popup_type) {
  delegate_->CreateNewWidget(GetProcess()->GetID(), route_id, popup_type);
}

void RenderViewHostImpl::CreateNewFullscreenWidget(int route_id) {
  delegate_->CreateNewFullscreenWidget(GetProcess()->GetID(), route_id);
}

void RenderViewHostImpl::OnShowView(int route_id,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_pos,
                                    bool user_gesture) {
  if (IsRVHStateActive(rvh_state_)) {
    delegate_->ShowCreatedWindow(
        route_id, disposition, initial_pos, user_gesture);
  }
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnShowWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  if (IsRVHStateActive(rvh_state_))
    delegate_->ShowCreatedWidget(route_id, initial_pos);
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnShowFullscreenWidget(int route_id) {
  if (IsRVHStateActive(rvh_state_))
    delegate_->ShowCreatedFullscreenWidget(route_id);
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnRunModal(int opener_id, IPC::Message* reply_msg) {
  DCHECK(!run_modal_reply_msg_);
  run_modal_reply_msg_ = reply_msg;
  run_modal_opener_id_ = opener_id;

  RecordAction(base::UserMetricsAction("ShowModalDialog"));

  RenderViewHostImpl* opener =
      RenderViewHostImpl::FromID(GetProcess()->GetID(), run_modal_opener_id_);
  if (opener) {
    opener->StopHangMonitorTimeout();
    // The ack for the mouse down won't come until the dialog closes, so fake it
    // so that we don't get a timeout.
    opener->decrement_in_flight_event_count();
  }

  // TODO(darin): Bug 1107929: Need to inform our delegate to show this view in
  // an app-modal fashion.
}

void RenderViewHostImpl::OnRenderViewReady() {
  render_view_termination_status_ = base::TERMINATION_STATUS_STILL_RUNNING;
  SendScreenRects();
  WasResized();
  delegate_->RenderViewReady(this);
}

void RenderViewHostImpl::OnRenderProcessGone(int status, int exit_code) {
#if defined(S_RENDERER_START_EXIT_LOGGING)
  LOG(INFO)<<"[Renderer][Exit]	RenderViewHostImpl::OnRenderProcessGone ";
#endif
  // Keep the termination status so we can get at it later when we
  // need to know why it died.
  render_view_termination_status_ =
      static_cast<base::TerminationStatus>(status);

  // Reset frame tree state associated with this process.
  main_frame_id_ = -1;
  delegate_->GetFrameTree()->RenderProcessGone(this);

  // Our base class RenderWidgetHost needs to reset some stuff.
  RendererExited(render_view_termination_status_, exit_code);

  delegate_->RenderViewTerminated(this,
                                  static_cast<base::TerminationStatus>(status),
                                  exit_code);
}

void RenderViewHostImpl::OnDidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& url) {
  NOTREACHED();
}

void RenderViewHostImpl::LoadDataWithBaseUrl(const std::string& data,
                                             const std::string& base_url,
                                             const std::string& mime_type,
                                             const std::string& encoding,
                                             const std::string& history_url) {
  Send(new ViewMsg_LoadDataWithBaseUrl(GetRoutingID(), data, base_url,
                                       mime_type, encoding, history_url));
}

void RenderViewHostImpl::OnNavigate(const IPC::Message& msg) {
  // TODO(nasko): Forward calls to the top level RenderFrameHost until all
  // callers of this method on RenderViewHost are removed.
  delegate_->GetFrameTree()->GetMainFrame()->OnMessageReceived(msg);
}

void RenderViewHostImpl::OnUpdateState(int32 page_id, const PageState& state) {
  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(state)) {
    GetProcess()->ReceivedBadMessage();
    return;
  }

  delegate_->UpdateState(this, page_id, state);
}

void RenderViewHostImpl::OnUpdateTitle(
    int32 page_id,
    const base::string16& title,
    blink::WebTextDirection title_direction) {
  if (title.length() > kMaxTitleChars) {
    NOTREACHED() << "Renderer sent too many characters in title.";
    return;
  }

  delegate_->UpdateTitle(this, page_id, title,
                         WebTextDirectionToChromeTextDirection(
                             title_direction));
}

void RenderViewHostImpl::OnUpdateEncoding(const std::string& encoding_name) {
  delegate_->UpdateEncoding(this, encoding_name);
}

void RenderViewHostImpl::OnUpdateTargetURL(int32 page_id, const GURL& url) {
  if (IsRVHStateActive(rvh_state_))
    delegate_->UpdateTargetURL(page_id, url);

  // Send a notification back to the renderer that we are ready to
  // receive more target urls.
  Send(new ViewMsg_UpdateTargetURL_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnUpdateInspectorSetting(
    const std::string& key, const std::string& value) {
  GetContentClient()->browser()->UpdateInspectorSetting(
      this, key, value);
}

void RenderViewHostImpl::OnClose() {
  // If the renderer is telling us to close, it has already run the unload
  // events, and we can take the fast path.
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::OnRequestMove(const gfx::Rect& pos) {
  if (IsRVHStateActive(rvh_state_))
    delegate_->RequestMove(pos);
  Send(new ViewMsg_Move_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnDidChangeLoadProgress(double load_progress) {
  delegate_->DidChangeLoadProgress(load_progress);
#if defined(SBROWSER_QC_OPTIMIZATION_PRECREATE_RENDERER)
  if (load_progress == 1 && extra_site_instance_)
    extra_site_instance_->GetProcess()->Init();
#endif
}

void RenderViewHostImpl::OnDidDisownOpener() {
  delegate_->DidDisownOpener(this);
}

void RenderViewHostImpl::OnDocumentAvailableInMainFrame() {
  delegate_->DocumentAvailableInMainFrame(this);
}

void RenderViewHostImpl::OnDocumentOnLoadCompletedInMainFrame(
    int32 page_id) {
  delegate_->DocumentOnLoadCompletedInMainFrame(this, page_id);
}

void RenderViewHostImpl::OnToggleFullscreen(bool enter_fullscreen) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->ToggleFullscreenMode(enter_fullscreen);
  // We need to notify the contents that its fullscreen state has changed. This
  // is done as part of the resize message.
  WasResized();
}

void RenderViewHostImpl::OnOpenURL(
    const ViewHostMsg_OpenURL_Params& params) {
  GURL validated_url(params.url);
  GetProcess()->FilterURL(false, &validated_url);

  delegate_->RequestOpenURL(
      this, validated_url, params.referrer, params.disposition, params.frame_id,
      params.should_replace_current_entry, params.user_gesture);
}

void RenderViewHostImpl::OnDidContentsPreferredSizeChange(
    const gfx::Size& new_size) {
  delegate_->UpdatePreferredSize(new_size);
}

void RenderViewHostImpl::OnRenderAutoResized(const gfx::Size& new_size) {
  delegate_->ResizeDueToAutoResize(new_size);
}

void RenderViewHostImpl::OnDidChangeScrollOffset() {
  if (view_)
    view_->ScrollOffsetChanged();
}

void RenderViewHostImpl::OnDidChangeScrollbarsForMainFrame(
    bool has_horizontal_scrollbar, bool has_vertical_scrollbar) {
  if (view_)
    view_->SetHasHorizontalScrollbar(has_horizontal_scrollbar);
}

void RenderViewHostImpl::OnDidChangeScrollOffsetPinningForMainFrame(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  if (view_)
    view_->SetScrollOffsetPinning(is_pinned_to_left, is_pinned_to_right);
}

void RenderViewHostImpl::OnDidChangeNumWheelEvents(int count) {
}

void RenderViewHostImpl::OnSelectionChanged(const base::string16& text,
                                            size_t offset,
                                            const gfx::Range& range) {
  if (view_)
    view_->SelectionChanged(text, offset, range);
}

void RenderViewHostImpl::OnSelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (view_) {
    view_->SelectionBoundsChanged(params);
  }
}

void RenderViewHostImpl::OnRouteCloseEvent() {
  // Have the delegate route this to the active RenderViewHost.
  delegate_->RouteCloseEvent(this);
}

void RenderViewHostImpl::OnRouteMessageEvent(
    const ViewMsg_PostMessage_Params& params) {
  // Give to the delegate to route to the active RenderViewHost.
  delegate_->RouteMessageEvent(this, params);
}

void RenderViewHostImpl::OnRunJavaScriptMessage(
    const base::string16& message,
    const base::string16& default_prompt,
    const GURL& frame_url,
    JavaScriptMessageType type,
    IPC::Message* reply_msg) {
  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  StopHangMonitorTimeout();
  delegate_->RunJavaScriptMessage(this, message, default_prompt, frame_url,
                                  type, reply_msg,
                                  &are_javascript_messages_suppressed_);
}

void RenderViewHostImpl::OnRunBeforeUnloadConfirm(const GURL& frame_url,
                                                  const base::string16& message,
                                                  bool is_reload,
                                                  IPC::Message* reply_msg) {
  // While a JS before unload dialog is showing, tabs in the same process
  // shouldn't process input events.
  GetProcess()->SetIgnoreInputEvents(true);
  StopHangMonitorTimeout();
  delegate_->RunBeforeUnloadConfirm(this, message, is_reload, reply_msg);
}

void RenderViewHostImpl::OnStartDragging(
    const DropData& drop_data,
    WebDragOperationsMask drag_operations_mask,
    const SkBitmap& bitmap,
    const gfx::Vector2d& bitmap_offset_in_dip,
    const DragEventSourceInfo& event_info) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (!view)
    return;

  DropData filtered_data(drop_data);
  RenderProcessHost* process = GetProcess();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Allow drag of Javascript URLs to enable bookmarklet drag to bookmark bar.
  if (!filtered_data.url.SchemeIs(kJavaScriptScheme))
    process->FilterURL(true, &filtered_data.url);
  process->FilterURL(false, &filtered_data.html_base_url);
  // Filter out any paths that the renderer didn't have access to. This prevents
  // the following attack on a malicious renderer:
  // 1. StartDragging IPC sent with renderer-specified filesystem paths that it
  //    doesn't have read permissions for.
  // 2. We initiate a native DnD operation.
  // 3. DnD operation immediately ends since mouse is not held down. DnD events
  //    still fire though, which causes read permissions to be granted to the
  //    renderer for any file paths in the drop.
  filtered_data.filenames.clear();
  for (std::vector<DropData::FileInfo>::const_iterator it =
           drop_data.filenames.begin();
       it != drop_data.filenames.end(); ++it) {
    base::FilePath path(
        base::FilePath::FromUTF8Unsafe(base::UTF16ToUTF8(it->path)));
    if (policy->CanReadFile(GetProcess()->GetID(), path))
      filtered_data.filenames.push_back(*it);
  }
  float scale = ui::GetImageScale(GetScaleFactorForView(GetView()));
  gfx::ImageSkia image(gfx::ImageSkiaRep(bitmap, scale));
  view->StartDragging(filtered_data, drag_operations_mask, image,
      bitmap_offset_in_dip, event_info);
}

void RenderViewHostImpl::OnUpdateDragCursor(WebDragOperation current_op) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->UpdateDragCursor(current_op);
}

void RenderViewHostImpl::OnTargetDropACK() {
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_VIEW_HOST_DID_RECEIVE_DRAG_TARGET_DROP_ACK,
      Source<RenderViewHost>(this),
      NotificationService::NoDetails());
}

void RenderViewHostImpl::OnSetBingAsCurrentSearchDefault() {
  GetContentClient()->browser()->SetBingAsCurrentSearchDefault(this);
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHostImpl::OnFocusedNodeChanged(bool is_editable_node,bool is_select_node,
                                              long node_id) {
  if (view_)
    view_->FocusedNodeChanged(is_editable_node,is_select_node, node_id);
#if defined(OS_WIN)
  if (!is_editable_node && virtual_keyboard_requested_) {
    virtual_keyboard_requested_ = false;
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(base::IgnoreResult(&DismissVirtualKeyboardTask)),
        TimeDelta::FromMilliseconds(kVirtualKeyboardDisplayWaitTimeoutMs));
  }
#endif
  NotificationService::current()->Notify(
      NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      Source<RenderViewHost>(this),
      Details<const bool>(&is_editable_node));
}

void RenderViewHostImpl::OnAddMessageToConsole(
    int32 level,
    const base::string16& message,
    int32 line_no,
    const base::string16& source_id) {
  if (delegate_->AddMessageToConsole(level, message, line_no, source_id))
    return;

  // Pass through log level only on WebUI pages to limit console spew.
  int32 resolved_level = HasWebUIScheme(delegate_->GetURL()) ? level : 0;

  if (resolved_level >= ::logging::GetMinLogLevel()) {
    logging::LogMessage("CONSOLE", line_no, resolved_level).stream() << "\"" <<
        message << "\", source: " << "" << " (" << line_no << ")";
  }
}

void RenderViewHostImpl::OnUserGesture() {
  delegate_->OnUserGesture();
}

void RenderViewHostImpl::OnShouldCloseACK(
    bool proceed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  decrement_in_flight_event_count();
  StopHangMonitorTimeout();
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in OnNavigate, in which case we can ignore
  // this message.
  if (!is_waiting_for_beforeunload_ack_ || rvh_state_ != STATE_DEFAULT)
    return;

  is_waiting_for_beforeunload_ack_ = false;

  RenderViewHostDelegate::RendererManagement* management_delegate =
      delegate_->GetRendererManagementDelegate();
  if (management_delegate) {
    base::TimeTicks before_unload_end_time;
    if (!send_should_close_start_time_.is_null() &&
        !renderer_before_unload_start_time.is_null() &&
        !renderer_before_unload_end_time.is_null()) {
      // When passing TimeTicks across process boundaries, we need to compensate
      // for any skew between the processes. Here we are converting the
      // renderer's notion of before_unload_end_time to TimeTicks in the browser
      // process. See comments in inter_process_time_ticks_converter.h for more.
      InterProcessTimeTicksConverter converter(
          LocalTimeTicks::FromTimeTicks(send_should_close_start_time_),
          LocalTimeTicks::FromTimeTicks(base::TimeTicks::Now()),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      LocalTimeTicks browser_before_unload_end_time =
          converter.ToLocalTimeTicks(
              RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();
    }
    management_delegate->ShouldClosePage(
        unload_ack_is_for_cross_site_transition_, proceed,
        before_unload_end_time);
  }

  // If canceled, notify the delegate to cancel its pending navigation entry.
  if (!proceed)
    delegate_->DidCancelLoading();
}

void RenderViewHostImpl::OnClosePageACK() {
  decrement_in_flight_event_count();
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::NotifyRendererUnresponsive() {
  LOG(INFO)<<"[RenderViewHostImpl][NotifyRendererUnresponsive]";
  delegate_->RendererUnresponsive(
      this, is_waiting_for_beforeunload_ack_, IsWaitingForUnloadACK());
}

void RenderViewHostImpl::NotifyRendererResponsive() {
  LOG(INFO)<<"[RenderViewHostImpl][NotifyRendererResponsive]";
  delegate_->RendererResponsive(this);
}

void RenderViewHostImpl::RequestToLockMouse(bool user_gesture,
                                            bool last_unlocked_by_target) {
  delegate_->RequestToLockMouse(user_gesture, last_unlocked_by_target);
}

bool RenderViewHostImpl::IsFullscreen() const {
  return delegate_->IsFullscreenForCurrentTab();
}

void RenderViewHostImpl::OnFocus() {
  // Note: We allow focus and blur from swapped out RenderViewHosts, even when
  // the active RenderViewHost is in a different BrowsingInstance (e.g., WebUI).
  delegate_->Activate();
}

void RenderViewHostImpl::OnBlur() {
  delegate_->Deactivate();
}

gfx::Rect RenderViewHostImpl::GetRootWindowResizerRect() const {
  return delegate_->GetRootWindowResizerRect();
}

void RenderViewHostImpl::ForwardMouseEvent(
    const blink::WebMouseEvent& mouse_event) {

  // We make a copy of the mouse event because
  // RenderWidgetHost::ForwardMouseEvent will delete |mouse_event|.
  blink::WebMouseEvent event_copy(mouse_event);
  RenderWidgetHostImpl::ForwardMouseEvent(event_copy);

  switch (event_copy.type) {
    case WebInputEvent::MouseMove:
      delegate_->HandleMouseMove();
      break;
    case WebInputEvent::MouseLeave:
      delegate_->HandleMouseLeave();
      break;
    case WebInputEvent::MouseDown:
      delegate_->HandleMouseDown();
      break;
    case WebInputEvent::MouseWheel:
      if (ignore_input_events())
        delegate_->OnIgnoredUIEvent();
      break;
    case WebInputEvent::MouseUp:
      delegate_->HandleMouseUp();
    default:
      // For now, we don't care about the rest.
      break;
  }
}

void RenderViewHostImpl::OnPointerEventActivate() {
  delegate_->HandlePointerActivate();
}

void RenderViewHostImpl::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (ignore_input_events()) {
    if (key_event.type == WebInputEvent::RawKeyDown)
      delegate_->OnIgnoredUIEvent();
    return;
  }
  RenderWidgetHostImpl::ForwardKeyboardEvent(key_event);
}

#if defined(OS_ANDROID)
void RenderViewHostImpl::DidSelectPopupMenuItems(
    const std::vector<int>& selected_indices) {
  Send(new ViewMsg_SelectPopupMenuItems(GetRoutingID(), false,
                                        selected_indices));
}

void RenderViewHostImpl::DidCancelPopupMenu() {
  Send(new ViewMsg_SelectPopupMenuItems(GetRoutingID(), true,
                                        std::vector<int>()));
}

void RenderViewHostImpl::OnSSRMModeCallback(int SSRMCaller, int count) {
  if (view_)
    view_->OnSSRMModeCallback(SSRMCaller, count);
}
#endif

#if defined(S_NOTIFY_ROTATE_STATUS)
void RenderViewHostImpl::OnNotifyRotateStatus() {
    RenderViewHostDelegateView* view =  delegate_->GetDelegateView();
    if (view)
        view->NotifyRotateStatus();
}
#endif

#if defined(OS_MACOSX)
void RenderViewHostImpl::DidSelectPopupMenuItem(int selected_index) {
  Send(new ViewMsg_SelectPopupMenuItem(GetRoutingID(), selected_index));
}

void RenderViewHostImpl::DidCancelPopupMenu() {
  Send(new ViewMsg_SelectPopupMenuItem(GetRoutingID(), -1));
}
#endif

void RenderViewHostImpl::SendOrientationChangeEvent(int orientation) {
  Send(new ViewMsg_OrientationChangeEvent(GetRoutingID(), orientation));
}

void RenderViewHostImpl::ToggleSpeechInput() {
  Send(new InputTagSpeechMsg_ToggleSpeechInput(GetRoutingID()));
}

bool RenderViewHostImpl::IsWaitingForUnloadACK() const {
  return rvh_state_ == STATE_WAITING_FOR_UNLOAD_ACK ||
         rvh_state_ == STATE_WAITING_FOR_CLOSE ||
         rvh_state_ == STATE_PENDING_SHUTDOWN ||
         rvh_state_ == STATE_PENDING_SWAP_OUT;
}

void RenderViewHostImpl::ExitFullscreen() {
  RejectMouseLockOrUnlockIfNecessary();
  // Notify delegate_ and renderer of fullscreen state change.
  OnToggleFullscreen(false);
}

WebPreferences RenderViewHostImpl::GetWebkitPreferences() {
  return delegate_->GetWebkitPrefs();
}

void RenderViewHostImpl::DisownOpener() {
  // This should only be called when swapped out.
  DCHECK(IsSwappedOut());

  Send(new ViewMsg_DisownOpener(GetRoutingID()));
}

void RenderViewHostImpl::SetAccessibilityCallbackForTesting(
    const base::Callback<void(ui::AXEvent)>& callback) {
  accessibility_testing_callback_ = callback;
}

void RenderViewHostImpl::UpdateWebkitPreferences(const WebPreferences& prefs) {
  Send(new ViewMsg_UpdateWebPreferences(GetRoutingID(), prefs));
}

void RenderViewHostImpl::NotifyTimezoneChange() {
  Send(new ViewMsg_TimezoneChange(GetRoutingID()));
}

void RenderViewHostImpl::GetAudioOutputControllers(
    const GetAudioOutputControllersCallback& callback) const {
  AudioRendererHost* audio_host =
      static_cast<RenderProcessHostImpl*>(GetProcess())->audio_renderer_host();
  audio_host->GetOutputControllers(GetRoutingID(), callback);
}

void RenderViewHostImpl::SetTextZoomFactor(float factor) {
  Send(new ViewMsg_SetTextZoomFactor(GetRoutingID(), factor));
}

void RenderViewHostImpl::ClearFocusedNode() {
  Send(new ViewMsg_ClearFocusedNode(GetRoutingID()));
}

void RenderViewHostImpl::Zoom(PageZoom zoom) {
  Send(new ViewMsg_Zoom(GetRoutingID(), zoom));
}

void RenderViewHostImpl::ReloadFrame() {
  Send(new ViewMsg_ReloadFrame(GetRoutingID()));
}

void RenderViewHostImpl::InsertCSS(const base::string16& frame_xpath,
                                   const std::string& css) {
  Send(new ViewMsg_CSSInsertRequest(GetRoutingID(), frame_xpath, css));
}

void RenderViewHostImpl::DisableScrollbarsForThreshold(const gfx::Size& size) {
  Send(new ViewMsg_DisableScrollbarsForSmallWindows(GetRoutingID(), size));
}

void RenderViewHostImpl::EnablePreferredSizeMode() {
  Send(new ViewMsg_EnablePreferredSizeChangedMode(GetRoutingID()));
}

void RenderViewHostImpl::EnableAutoResize(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  SetShouldAutoResize(true);
  Send(new ViewMsg_EnableAutoResize(GetRoutingID(), min_size, max_size));
}

void RenderViewHostImpl::DisableAutoResize(const gfx::Size& new_size) {
  SetShouldAutoResize(false);
  Send(new ViewMsg_DisableAutoResize(GetRoutingID(), new_size));
}

void RenderViewHostImpl::CopyImageAt(int x, int y) {
  Send(new ViewMsg_CopyImageAt(GetRoutingID(), x, y));
}

void RenderViewHostImpl::ExecuteMediaPlayerActionAtLocation(
  const gfx::Point& location, const blink::WebMediaPlayerAction& action) {
  Send(new ViewMsg_MediaPlayerActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::ExecutePluginActionAtLocation(
  const gfx::Point& location, const blink::WebPluginAction& action) {
  Send(new ViewMsg_PluginActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::NotifyMoveOrResizeStarted() {
  Send(new ViewMsg_MoveOrResizeStarted(GetRoutingID()));
}

void RenderViewHostImpl::OnAccessibilityEvents(
    const std::vector<AccessibilityHostMsg_EventParams>& params) {
  if ((accessibility_mode() & AccessibilityModeFlagPlatform) && view_ &&
      IsRVHStateActive(rvh_state_)) {
    view_->CreateBrowserAccessibilityManagerIfNeeded();
    BrowserAccessibilityManager* manager =
        view_->GetBrowserAccessibilityManager();
    if (manager)
      manager->OnAccessibilityEvents(params);
  }

  // Always send an ACK or the renderer can be in a bad state.
  Send(new AccessibilityMsg_Events_ACK(GetRoutingID()));

  // The rest of this code is just for testing; bail out if we're not
  // in that mode.
  if (accessibility_testing_callback_.is_null())
    return;

  for (unsigned i = 0; i < params.size(); i++) {
    const AccessibilityHostMsg_EventParams& param = params[i];
    if (static_cast<int>(param.event_type) < 0)
      continue;
    ui::AXTreeUpdate update;
    update.nodes = param.nodes;
    if (!ax_tree_)
      ax_tree_.reset(new ui::AXTree(update));
    else
      CHECK(ax_tree_->Unserialize(update)) << ax_tree_->error();
    accessibility_testing_callback_.Run(param.event_type);
  }
}

void RenderViewHostImpl::OnAccessibilityLocationChanges(
    const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  if (view_ && IsRVHStateActive(rvh_state_)) {
    view_->CreateBrowserAccessibilityManagerIfNeeded();
    BrowserAccessibilityManager* manager =
        view_->GetBrowserAccessibilityManager();
    if (manager)
      manager->OnLocationChanges(params);
  }
}

void RenderViewHostImpl::OnScriptEvalResponse(int id,
                                              const base::ListValue& result) {
  const base::Value* result_value;
  if (!result.Get(0, &result_value)) {
    // Programming error or rogue renderer.
    NOTREACHED() << "Got bad arguments for OnScriptEvalResponse";
    return;
  }

  std::map<int, JavascriptResultCallback>::iterator it =
      javascript_callbacks_.find(id);
  if (it != javascript_callbacks_.end()) {
    // ExecuteJavascriptInWebFrameCallbackResult was used; do callback.
    it->second.Run(result_value);
    javascript_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received script response for unknown request";
  }
}

void RenderViewHostImpl::OnDidZoomURL(double zoom_level,
                                      bool remember,
                                      const GURL& url) {
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForBrowserContext(GetProcess()->GetBrowserContext()));
  if (remember) {
    host_zoom_map->
        SetZoomLevelForHost(net::GetHostOrSpecFromURL(url), zoom_level);
  } else {
    host_zoom_map->SetTemporaryZoomLevel(
        GetProcess()->GetID(), GetRoutingID(), zoom_level);
  }
}

void RenderViewHostImpl::OnRequestDesktopNotificationPermission(
    const GURL& source_origin, int callback_context) {
  GetContentClient()->browser()->RequestDesktopNotificationPermission(
      source_origin, callback_context, GetProcess()->GetID(), GetRoutingID());
}

void RenderViewHostImpl::OnShowDesktopNotification(
    const ShowDesktopNotificationHostMsgParams& params) {
  GetContentClient()->browser()->ShowDesktopNotification(
      params, GetProcess()->GetID(), GetRoutingID(), false);
}

void RenderViewHostImpl::OnCancelDesktopNotification(int notification_id) {
  GetContentClient()->browser()->CancelDesktopNotification(
      GetProcess()->GetID(), GetRoutingID(), notification_id);
}

void RenderViewHostImpl::OnRunFileChooser(const FileChooserParams& params) {
  delegate_->RunFileChooser(this, params);
}

void RenderViewHostImpl::OnDidAccessInitialDocument() {
  has_accessed_initial_document_ = true;
  delegate_->DidAccessInitialDocument();
}

void RenderViewHostImpl::OnDomOperationResponse(
    const std::string& json_string, int automation_id) {
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      NOTIFICATION_DOM_OPERATION_RESPONSE,
      Source<RenderViewHost>(this),
      Details<DomOperationNotificationDetails>(&details));
}

void RenderViewHostImpl::OnFocusedNodeTouched(bool editable) {
#if defined(OS_WIN)
  if (editable) {
    virtual_keyboard_requested_ = base::win::DisplayVirtualKeyboard();
  } else {
    virtual_keyboard_requested_ = false;
    base::win::DismissVirtualKeyboard();
  }
#endif
}

#if defined(OS_MACOSX) || defined(OS_ANDROID)
void RenderViewHostImpl::OnShowPopup(
    const ViewHostMsg_ShowPopup_Params& params) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view) {
    //SBROWSER_FORM_NAVIGATION
    view->ShowPopupMenu(params.bounds,
                        params.item_height,
                        params.item_font_size,
                        params.selected_item,
                        params.popup_items,
                        params.right_aligned,
                        params.allow_multiple_selection,
                        params.advanced_ime_options);
  }
}
#endif

void RenderViewHostImpl::GetSelectionMarkup() {
  Send(new ViewMsg_GetSelectionMarkup(GetRoutingID()));
}

void RenderViewHostImpl::GetFocusedInputInfo() {
  Send(new ViewMsg_GetFocusedInputInfo(GetRoutingID()));
}
// MULTI-SELECTION >>
#if defined(SBROWSER_MULTI_SELECTION)
void RenderViewHostImpl::GetSelectionMarkupWithBounds() {
  Send(new ViewMsg_GetSelectionMarkupWithBounds(GetRoutingID()));
}
#endif
// MULTI-SELECTION <<
void RenderViewHostImpl::SetState(RenderViewHostImplState rvh_state) {
  // We update the number of RenderViews in a SiteInstance when the
  // swapped out status of this RenderView gets flipped to/from live.
  if (!IsRVHStateActive(rvh_state_) && IsRVHStateActive(rvh_state))
    instance_->increment_active_view_count();
  else if (IsRVHStateActive(rvh_state_) && !IsRVHStateActive(rvh_state))
    instance_->decrement_active_view_count();

  // Whenever we change the RVH state to and from live or swapped out state, we
  // should not be waiting for beforeunload or unload acks.  We clear them here
  // to be safe, since they can cause navigations to be ignored in OnNavigate.
  if (rvh_state == STATE_DEFAULT ||
      rvh_state == STATE_SWAPPED_OUT ||
      rvh_state_ == STATE_DEFAULT ||
      rvh_state_ == STATE_SWAPPED_OUT) {
    is_waiting_for_beforeunload_ack_ = false;
  }
  rvh_state_ = rvh_state;

}

bool RenderViewHostImpl::CanAccessFilesOfPageState(
    const PageState& state) const {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const std::vector<base::FilePath>& file_paths = state.GetReferencedFiles();
  for (std::vector<base::FilePath>::const_iterator file = file_paths.begin();
       file != file_paths.end(); ++file) {
    if (!policy->CanReadFile(GetProcess()->GetID(), *file))
      return false;
  }
  return true;
}

void RenderViewHostImpl::AttachToFrameTree() {
  FrameTree* frame_tree = delegate_->GetFrameTree();

  frame_tree->ResetForMainFrameSwap();
  if (main_frame_id() != FrameTreeNode::kInvalidFrameId) {
    frame_tree->OnFirstNavigationAfterSwap(main_frame_id());
  }
}

void RenderViewHostImpl::OnSelectedMarkup(const base::string16& markup) {
   RenderViewHostDelegateView* view = delegate_->GetDelegateView();
   if (view)
      view->SelectedMarkup(markup);
}

void RenderViewHostImpl::HandleSelectionDrop(int x, int y, base::string16& text) {
  Send(new ViewMsg_HandleSelectionDrop(GetRoutingID(), x, y, text));
}

// SBROWSER_HANDLE_MOUSECLICK_CTRL ++
void RenderViewHostImpl::HandleMouseClickWithCtrlkey(int x, int y){
  Send(new ViewMsg_HandleMouseClickWithCtrlkey(GetRoutingID(), x, y));
}

void RenderViewHostImpl::OnOpenUrlInNewTab(const base::string16& mouse_click_url){
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (!view)
    return;
  view->OnOpenUrlInNewTab(mouse_click_url);
}
// SBROWSER_HANDLE_MOUSECLICK_CTRL --

//Sent to browser for setting last touch point for long press enter key : start
void RenderViewHostImpl::SetLongPressSelectionPoint(int x, int y)
{
    RenderViewHostDelegateView* view = delegate_->GetDelegateView();
    if (!view)
        return;
    view->SetLongPressSelectionPoint(x, y);
}
//Sent to browser for setting last touch point for long press enter key : end

void RenderViewHostImpl::OnUpdateTouchedFixedElementHeight(int height)
{
    RenderViewHostDelegateView* view = delegate_->GetDelegateView();
    if (!view)
        return;
    view->UpdateTouchedFixedElementHeight(height);
}

void RenderViewHostImpl::HandleSelectionDropOnFocusedInput(base::string16& text, int dropAction) {
  Send(new ViewMsg_HandleSelectionDropOnFocusedInput(GetRoutingID(), text, dropAction));
}

void RenderViewHostImpl::OnSelectionVisibilityStatusReceived(bool isVisible) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->SetSelectionVisibility(isVisible);
}

void RenderViewHostImpl::OnUpdateSelectionRect(const gfx::Rect& selectionRect) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->UpdateSelectionRect(selectionRect);
}

void RenderViewHostImpl::OnPointOnRegion(bool isOnRegion) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->PointOnRegion(isOnRegion);
}

void RenderViewHostImpl::OnSelectedBitmap(const SkBitmap& bitmap) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->SelectedBitmap(bitmap);
}

void RenderViewHostImpl::OnRecognizeArticleResult(std::string reader_result_str) {
  if (view_)
    view_->OnRecognizeArticleResult(reader_result_str);
}

void RenderViewHostImpl::SavePageAs(
    const base::FilePath::StringType& pure_file_name) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->SavePageFileName(pure_file_name);
}

void RenderViewHostImpl::OnReceiveBitmapFromCache(const SkBitmap& bitmap) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->OnReceiveBitmapFromCache(bitmap);
}

#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
void RenderViewHostImpl::OnAutoLoginFailure()
{
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  LOG(INFO)<<"FP: " << __FUNCTION__;
  if(view)
    view->ShowAutoLoginFailureMsg();
}
#endif

#if defined(S_INTUITIVE_HOVER)
void RenderViewHostImpl::OnHoverHitTestResult(int contentType) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->OnHoverHitTestResult(contentType);
}
#endif

}  // namespace content
