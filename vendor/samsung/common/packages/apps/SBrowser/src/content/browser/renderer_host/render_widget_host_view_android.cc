// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/worker_pool.h"
#include "cc/base/latency_info_swap_promise.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "cc/trees/layer_tree_host.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/overscroll_glow.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "skia/ext/image_operations.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/android/window_android.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#if defined(S_NATIVE_SUPPORT)//SBROWSER_FORM_NAVIGATION
#include "sbrowser/content/native/browser/android/sbr/sbr_content_view_core_impl.h"
#endif
namespace content {

namespace {

const int kUndefinedOutputSurfaceId = -1;
static const char kAsyncReadBackString[] = "Compositing.CopyFromSurfaceTime";

void InsertSyncPointAndAckForCompositor(
    int renderer_host_id,
    uint32 output_surface_id,
    int route_id,
    const gpu::Mailbox& return_mailbox,
    const gfx::Size return_size) {
  cc::CompositorFrameAck ack;
  ack.gl_frame_data.reset(new cc::GLFrameData());
  if (!return_mailbox.IsZero()) {
    ack.gl_frame_data->mailbox = return_mailbox;
    ack.gl_frame_data->size = return_size;
    ack.gl_frame_data->sync_point =
        ImageTransportFactoryAndroid::GetInstance()->InsertSyncPoint();
  }
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(
      route_id, output_surface_id, renderer_host_id, ack);
}

// Sends an acknowledgement to the renderer of a processed IME event.
void SendImeEventAck(RenderWidgetHostImpl* host) {
  host->Send(new ViewMsg_ImeEventAck(host->GetRoutingID()));
}

void CopyFromCompositingSurfaceFinished(
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    const base::TimeTicks& start_time,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  bitmap_pixels_lock.reset();
  release_callback->Run(0, false);
  UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                      base::TimeTicks::Now() - start_time);
  callback.Run(result, *bitmap);
}

ui::LatencyInfo CreateLatencyInfo(const blink::WebInputEvent& event) {
  ui::LatencyInfo latency_info;
  // The latency number should only be added if the timestamp is valid.
  if (event.timeStampSeconds) {
    const int64 time_micros = static_cast<int64>(
        event.timeStampSeconds * base::Time::kMicrosecondsPerSecond);
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
        0,
        0,
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(time_micros),
        1);
  }
  return latency_info;
}

OverscrollGlow::DisplayParameters CreateOverscrollDisplayParameters(
    const cc::CompositorFrameMetadata& frame_metadata) {
  const float scale_factor =
      frame_metadata.page_scale_factor * frame_metadata.device_scale_factor;

  // Compute the size and offsets for each edge, where each effect is sized to
  // the viewport and offset by the distance of each viewport edge to the
  // respective content edge.
  OverscrollGlow::DisplayParameters params;
  params.size = gfx::ScaleSize(frame_metadata.viewport_size, scale_factor);
  params.edge_offsets[OverscrollGlow::EDGE_TOP] =
      -frame_metadata.root_scroll_offset.y() * scale_factor;
  params.edge_offsets[OverscrollGlow::EDGE_LEFT] =
      -frame_metadata.root_scroll_offset.x() * scale_factor;
  params.edge_offsets[OverscrollGlow::EDGE_BOTTOM] =
      (frame_metadata.root_layer_size.height() -
       frame_metadata.root_scroll_offset.y() -
       frame_metadata.viewport_size.height()) * scale_factor;
  params.edge_offsets[OverscrollGlow::EDGE_RIGHT] =
      (frame_metadata.root_layer_size.width() -
       frame_metadata.root_scroll_offset.x() -
       frame_metadata.viewport_size.width()) * scale_factor;
  params.device_scale_factor = frame_metadata.device_scale_factor;

  return params;
}

}  // anonymous namespace

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    ContentViewCoreImpl* content_view_core)
    : host_(widget_host),
      needs_begin_frame_(false),
      is_showing_(!widget_host->is_hidden()),
      content_view_core_(NULL),
      ime_adapter_android_(this),
      cached_background_color_(SK_ColorWHITE),
      texture_id_in_layer_(0),
      last_output_surface_id_(kUndefinedOutputSurfaceId),
      weak_ptr_factory_(this),
      overscroll_effect_enabled_(
          !CommandLine::ForCurrentProcess()->
              HasSwitch(switches::kDisableOverscrollEdgeEffect)),
      flush_input_requested_(false),
      accelerated_surface_route_id_(0),
      using_synchronous_compositor_(SynchronousCompositorImpl::FromID(
                                        widget_host->GetProcess()->GetID(),
                                        widget_host->GetRoutingID()) != NULL),
      frame_evictor_(new DelegatedFrameEvictor(this)),
      using_delegated_renderer_(CommandLine::ForCurrentProcess()->HasSwitch(
                                    switches::kEnableDelegatedRenderer) &&
                                !CommandLine::ForCurrentProcess()->HasSwitch(
                                    switches::kDisableDelegatedRenderer)) {
  if (!using_delegated_renderer_) {
    texture_layer_ = cc::TextureLayer::Create(NULL);
    layer_ = texture_layer_;
  }

  host_->SetView(this);
  SetContentViewCore(content_view_core);
  ImageTransportFactoryAndroid::AddObserver(this);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  ImageTransportFactoryAndroid::RemoveObserver(this);
  SetContentViewCore(NULL);
  DCHECK(ack_callbacks_.empty());
  if (texture_id_in_layer_) {
    ImageTransportFactoryAndroid::GetInstance()->DeleteTexture(
        texture_id_in_layer_);
  }

  if (texture_layer_.get())
    texture_layer_->ClearClient();

  if (resource_collection_.get())
    resource_collection_->SetClient(NULL);
}


bool RenderWidgetHostViewAndroid::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAndroid, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartContentIntent, OnStartContentIntent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeBodyBackgroundColor,
                        OnDidChangeBodyBackgroundColor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetNeedsBeginFrame,
                        OnSetNeedsBeginFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputStateChanged,
                        OnTextInputStateChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SmartClipDataExtracted,
                        OnSmartClipDataExtracted)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateFocusedInputInfo,
                        OnUpdateFocusedInputInfo)
#if defined(SBROWSER_MULTI_SELECTION)                       
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectedMarkupWithStartContentRect, 
                        OnSelectedMarkupWithStartContentRect)
#endif                        
#if defined(SBROWSER_HIDE_URLBAR_HYBRID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnRendererInitializeComplete,
                        OnRendererInitializeComplete)
#endif    
#if defined(SBROWSER_HIDE_URLBAR_EOP)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnUpdateEndOfPageState,
                        OnUpdateEndOfPageState)
#endif   
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnScrollEnd,
                        OnScrollEnd)
#endif 
                   
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

RenderWidgetHost*
RenderWidgetHostViewAndroid::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAndroid::WasShown() {
  if (!host_ || !host_->is_hidden())
    return;

  LOG(INFO) << "RenderWidgetHostViewAndroid::WasShown() : "<<host_->is_hidden();	
  host_->WasShown();

  if (content_view_core_ && content_view_core_->GetWindowAndroid() && !using_synchronous_compositor_)
    content_view_core_->GetWindowAndroid()->AddObserver(this);
}

void RenderWidgetHostViewAndroid::WasHidden() {
  RunAckCallbacks();

  if (!host_ || host_->is_hidden())
    return;

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();

  if (content_view_core_ && content_view_core_->GetWindowAndroid() && !using_synchronous_compositor_)
    content_view_core_->GetWindowAndroid()->RemoveObserver(this);
}

void RenderWidgetHostViewAndroid::WasResized() {
  host_->WasResized();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  default_size_ = size;
  WasResized();
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewAndroid::GetScaledContentBitmap(
    float scale,
    gfx::Size* out_size,
    gfx::Rect src_subrect,
    const base::Callback<void(bool, const SkBitmap&)>& result_callback) {
  if (!IsSurfaceAvailableForCopy()) {
    result_callback.Run(false, SkBitmap());
    return;
  }

  gfx::Size bounds = layer_->bounds();
  if (src_subrect.IsEmpty())
    src_subrect = gfx::Rect(bounds);
  DCHECK_LE(src_subrect.width() + src_subrect.x(), bounds.width());
  DCHECK_LE(src_subrect.height() + src_subrect.y(), bounds.height());
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  DCHECK_GT(device_scale_factor, 0);
  gfx::Size dst_size(
      gfx::ToCeiledSize(gfx::ScaleSize(bounds, scale / device_scale_factor)));
  *out_size = dst_size;
  CopyFromCompositingSurface(
      src_subrect, dst_size, result_callback, SkBitmap::kARGB_8888_Config);
}

#if defined(S_NATIVE_SUPPORT)
bool RenderWidgetHostViewAndroid::PopulateBitmapWithContents(jobject jbitmap) {
  if (!CompositorImpl::IsInitialized() ||
      texture_id_in_layer_ == 0 ||
      texture_size_in_layer_.IsEmpty())
    return false;

  gfx::JavaBitmap bitmap(jbitmap);

  // TODO(dtrainor): Eventually add support for multiple formats here.
  DCHECK(bitmap.format() == ANDROID_BITMAP_FORMAT_RGBA_8888);

  GLHelper* helper = ImageTransportFactoryAndroid::GetInstance()->GetGLHelper();

  GLuint texture = helper->CopyAndScaleTexture(
      texture_id_in_layer_,
      texture_size_in_layer_,
      bitmap.size(),
      true,
      GLHelper::SCALER_QUALITY_FAST);
  if (texture == 0u)
    return false;

  helper->ReadbackTextureSync(texture,
                              gfx::Rect(bitmap.size()),
                              static_cast<unsigned char*> (bitmap.pixels()),
                              SkBitmap::kARGB_8888_Config);

  gpu::gles2::GLES2Interface* gl =
      ImageTransportFactoryAndroid::GetInstance()->GetContextGL();
  gl->DeleteTextures(1, &texture);

  return true;
}
#endif

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  if (!content_view_core_)
    return false;
  if (!layer_)
    return false;

  if (texture_size_in_layer_.IsEmpty())
    return false;

  if (using_delegated_renderer_) {
    if (!delegated_renderer_layer_.get())
      return false;
  } else {
    if (texture_id_in_layer_ == 0)
      return false;
  }

  return true;
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() const {
  return content_view_core_->GetViewAndroid();
}

gfx::NativeViewId RenderWidgetHostViewAndroid::GetNativeViewId() const {
  return reinterpret_cast<gfx::NativeViewId>(
      const_cast<RenderWidgetHostViewAndroid*>(this));
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<WebPluginGeometry>& moves) {
  // We don't have plugin windows on Android. Do nothing. Note: this is called
  // from RenderWidgetHost::OnUpdateRect which is itself invoked while
  // processing the corresponding message from Renderer.
}

void RenderWidgetHostViewAndroid::Focus() {
  host_->Focus();
  host_->SetInputMethodActive(true);
  ResetClipping();
  if (overscroll_effect_enabled_)
    overscroll_effect_->Enable();
}

void RenderWidgetHostViewAndroid::Blur() {
  host_->ExecuteEditCommand("Unselect", "");
  host_->SetInputMethodActive(false);
  host_->Blur();
  overscroll_effect_->Disable();
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  if (!content_view_core_)
    return false;  // ContentViewCore not created yet.

  return content_view_core_->HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  return HasValidFrame();
}

void RenderWidgetHostViewAndroid::Show() {
  LOG(INFO) << "RenderWidgetHostViewAndroid::Show() :: is_showing_ : "<<is_showing_;
  if (is_showing_)
    return;

  is_showing_ = true;
  if (layer_)
    layer_->SetHideLayerAndSubtree(false);

  frame_evictor_->SetVisible(true);
  WasShown();
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  is_showing_ = false;
  if (layer_)
    layer_->SetHideLayerAndSubtree(true);

  frame_evictor_->SetVisible(false);
  WasHidden();
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // ContentViewCoreImpl represents the native side of the Java
  // ContentViewCore.  It being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_showing_ && content_view_core_;
}

void RenderWidgetHostViewAndroid::LockResources() {
  DCHECK(HasValidFrame());
  DCHECK(host_);
  DCHECK(!host_->is_hidden());
  frame_evictor_->LockFrame();
}

void RenderWidgetHostViewAndroid::UnlockResources() {
  DCHECK(HasValidFrame());
  frame_evictor_->UnlockFrame();
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  if (!content_view_core_)
    return gfx::Rect(default_size_);

  gfx::Size size = content_view_core_->GetViewportSizeDip();
  gfx::Size offset = content_view_core_->GetViewportSizeOffsetDip();
  size.Enlarge(-offset.width(), -offset.height());

  return gfx::Rect(size);
}

gfx::Size RenderWidgetHostViewAndroid::GetPhysicalBackingSize() const {
  if (!content_view_core_)
    return gfx::Size();

  return content_view_core_->GetPhysicalBackingSize();
}

#if defined (SBROWSER_MULTIINSTANCE_TAB_DRAG_AND_DROP)
bool RenderWidgetHostViewAndroid::GetTabDragAndDropIsInProgress() const {
  if (!content_view_core_)
    return false;

  return static_cast<SbrContentViewCoreImpl*>(content_view_core_)->GetTabDragAndDropIsInProgress();
}
#endif

float RenderWidgetHostViewAndroid::GetOverdrawBottomHeight() const {
  if (!content_view_core_)
    return 0.f;

  return content_view_core_->GetOverdrawBottomHeightDip();
}

void RenderWidgetHostViewAndroid::selectPopupCloseZero() {
#if defined(S_NATIVE_SUPPORT)//SBROWSER_FORM_NAVIGATION
  if (content_view_core_)
    static_cast<SbrContentViewCoreImpl*>(content_view_core_)->selectPopupCloseZero();
#endif
}
void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& cursor) {
  // There are no cursors on Android.
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

void RenderWidgetHostViewAndroid::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline) {
  // Unused on Android, which uses OnTextInputChanged instead.
}

int RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<int>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::OnTextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  // If an acknowledgement is required for this event, regardless of how we exit
  // from this method, we must acknowledge that we processed the input state
  // change.
  base::ScopedClosureRunner ack_caller;
  if (params.require_ack)
    ack_caller.Reset(base::Bind(&SendImeEventAck, host_));

  if (!IsShowing()
#if defined(S_PLM_P140809_00188)
    || GetNativeImeAdapter()<0
#endif
  	){
  	LOG(INFO) << "RenderWidgetHostViewAndroid::OnTextInputStateChanged  GetNativeImeAdapter = "<< GetNativeImeAdapter();
    return;
  }

#if defined(S_NATIVE_SUPPORT)//SBROWSER_FORM_NAVIGATION
  static_cast<SbrContentViewCoreImpl*>(content_view_core_)->UpdateImeAdapter(
    GetNativeImeAdapter(),
    static_cast<int>(params.type),
    params.value, params.selection_start, params.selection_end,
    params.composition_start, params.composition_end,
    params.show_ime_if_needed,
    params.require_ack,
    params.advanced_ime_options);
#else
  content_view_core_->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type),
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed, params.require_ack);
#endif
}

#if defined(SBROWSER_UI_COMPOSITOR_SET_BACKGROUND_COLOR)
void RenderWidgetHostViewAndroid::SetBackgroundColor() {
    if (delegated_renderer_layer_.get() && using_delegated_renderer_) {
      delegated_renderer_layer_->SetBackgroundColor(cached_background_color_);
    }
}
#endif

void RenderWidgetHostViewAndroid::OnDidChangeBodyBackgroundColor(
    SkColor color) {
  if (cached_background_color_ == color)
    return;

  cached_background_color_ = color;
  if (content_view_core_)
    content_view_core_->OnBackgroundColorChanged(color);

#if defined(SBROWSER_UI_COMPOSITOR_SET_BACKGROUND_COLOR)
    SetBackgroundColor();
#endif
}

void RenderWidgetHostViewAndroid::SendBeginFrame(
    const cc::BeginFrameArgs& args) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::SendBeginFrame");
  if (!host_)
    return;

  if (flush_input_requested_) {
    flush_input_requested_ = false;
    host_->FlushInput();
    content_view_core_->RemoveBeginFrameSubscriber();
  }

  host_->Send(new ViewMsg_BeginFrame(host_->GetRoutingID(), args));
}

void RenderWidgetHostViewAndroid::OnSetNeedsBeginFrame(
    bool enabled) {
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::OnSetNeedsBeginFrame",
               "enabled", enabled);
  // ContentViewCoreImpl handles multiple subscribers to the BeginFrame, so
  // we have to make sure calls to ContentViewCoreImpl's
  // {Add,Remove}BeginFrameSubscriber are balanced, even if
  // RenderWidgetHostViewAndroid's may not be.
  if (content_view_core_ && needs_begin_frame_ != enabled) {
    if (enabled)
      content_view_core_->AddBeginFrameSubscriber();
    else
      content_view_core_->RemoveBeginFrameSubscriber();
    needs_begin_frame_ = enabled;
  }
}

void RenderWidgetHostViewAndroid::OnStartContentIntent(
    const GURL& content_url) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url);
}

void RenderWidgetHostViewAndroid::OnSmartClipDataExtracted(
    const base::string16& result, const base::string16& innerHTML) {
  // Custom serialization over IPC isn't allowed normally for security reasons.
  // Since this feature is only used in (single-process) WebView, there are no
  // security issues. Enforce that it's only called in single process mode.
#ifndef S_NATIVE_SUPPORT
  // FIXME: Will fail for SBrowser as it is multiprocess.
  // To be fixed by sending vector instead of custom serialization
  // Open Source issue
  CHECK(RenderProcessHost::run_renderer_in_process());
#endif
  if (content_view_core_)
    content_view_core_->OnSmartClipDataExtracted(result, innerHTML);
}

void RenderWidgetHostViewAndroid::OnUpdateFocusedInputInfo(
    const gfx::Rect& bounds, bool is_multi_line_input, bool is_content_richly_editable) {
#ifdef S_NATIVE_SUPPORT
  if (content_view_core_) {
    static_cast<SbrContentViewCoreImpl*>(content_view_core_)->OnUpdateFocusedInputInfo(
      bounds, is_multi_line_input, is_content_richly_editable);
  }
#endif  
}
// MULTI-SELECTION >>
#if defined(SBROWSER_MULTI_SELECTION)
void RenderWidgetHostViewAndroid::OnSelectedMarkupWithStartContentRect (
    const base::string16& markup, const gfx::Rect& selection_start_content_rect) {
#ifdef S_NATIVE_SUPPORT
  if (content_view_core_) {
    static_cast<SbrContentViewCoreImpl*>(content_view_core_)->OnSelectedMarkupWithStartContentRect(
      markup, selection_start_content_rect);
  }
#endif
}
#endif
// MULTI-SELECTION <<

#if defined(SBROWSER_HIDE_URLBAR_HYBRID)
void RenderWidgetHostViewAndroid::OnRendererInitializeComplete(){
  if (content_view_core_)
      static_cast<SbrContentViewCoreImpl*>(content_view_core_)->OnRendererInitializeComplete();
}

void RenderWidgetHostViewAndroid::SetTopControlsHeight(int top_controls_height) {
  host_->Send(new ViewMsg_SetTopControlsHeight(host_->GetRoutingID(), top_controls_height));
}
#endif

#if defined(S_SET_SCROLL_TYPE)
void RenderWidgetHostViewAndroid::SetScrollType(int type) {
  host_->Send(new ViewMsg_SetScrollType(host_->GetRoutingID(), type));
}
#endif

#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
void RenderWidgetHostViewAndroid::OnScrollEnd(bool scroll_ignored) {
  if (content_view_core_) {
    SbrUIResourceLayerManager *ui_resource_mgr = static_cast<SbrContentViewCoreImpl*>(
                                                 content_view_core_)->GetUIResourceLayerManager();
    if(ui_resource_mgr){
      ui_resource_mgr->onScrollEnd(scroll_ignored);
    }
  }
}

void RenderWidgetHostViewAndroid::DidViewPortSizeChanged(gfx::Size size) {
  if (content_view_core_) {
    SbrUIResourceLayerManager *ui_resource_mgr = static_cast<SbrContentViewCoreImpl*>(
                                                 content_view_core_)->GetUIResourceLayerManager();
    if(ui_resource_mgr){
      gfx::SizeF sizef = size;
      ui_resource_mgr->DidViewPortSizeChanged(sizef);
    }
  }
}

#endif

#if defined(SBROWSER_HIDE_URLBAR_EOP)
void RenderWidgetHostViewAndroid::OnUpdateEndOfPageState( bool eop_state ) {
  if (content_view_core_)
      static_cast<SbrContentViewCoreImpl*>(content_view_core_)->OnUpdateEndOfPageState(eop_state);
}
#endif

void RenderWidgetHostViewAndroid::ImeCancelComposition() {
  ime_adapter_android_.CancelComposition();
}

void RenderWidgetHostViewAndroid::FocusedNodeChanged(bool is_editable_node,bool is_select_node,
                                                     long node_id) {
  ime_adapter_android_.FocusedNodeChanged(is_editable_node,is_select_node);
#ifdef S_NATIVE_SUPPORT
  if (content_view_core_) {
    static_cast<SbrContentViewCoreImpl*>(content_view_core_)->FocusedNodeChanged(
      is_editable_node,is_select_node, node_id);
  }
#endif
}

void RenderWidgetHostViewAndroid::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects,
    const std::vector<ui::LatencyInfo>& latency_info) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::RenderProcessGone(
    base::TerminationStatus status, int error_code) {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  RemoveLayers();
  SetContentViewCore(NULL);

  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;

  delete this;
}

void RenderWidgetHostViewAndroid::SetTooltipText(
    const base::string16& tooltip_text) {
  // Tooltips don't makes sense on Android.
}

void RenderWidgetHostViewAndroid::SelectionChanged(const base::string16& text,
                                                   size_t offset,
                                                   const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (text.empty() || range.is_empty() || !content_view_core_)
    return;
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  std::string utf8_selection = base::UTF16ToUTF8(text.substr(pos, n));

  content_view_core_->OnSelectionChanged(utf8_selection);
}

void RenderWidgetHostViewAndroid::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (content_view_core_) {
#if defined(S_NATIVE_SUPPORT)
    static_cast<SbrContentViewCoreImpl*>(content_view_core_)->OnSelectionBoundsChanged(params);
#else
    content_view_core_->OnSelectionBoundsChanged(params);
#endif
  }
}

void RenderWidgetHostViewAndroid::ScrollOffsetChanged() {
}

BackingStore* RenderWidgetHostViewAndroid::AllocBackingStore(
    const gfx::Size& size) {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  host_->Send(new ViewMsg_SetBackground(host_->GetRoutingID(), background));
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config bitmap_config) {
  // Only ARGB888 and RGB565 supported as of now.
  bool format_support = ((bitmap_config == SkBitmap::kRGB_565_Config) ||
                         (bitmap_config == SkBitmap::kARGB_8888_Config));
  if (!format_support) {
    DCHECK(format_support);
    callback.Run(false, SkBitmap());
    return;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (!using_synchronous_compositor_ && !IsSurfaceAvailableForCopy()) {
    callback.Run(false, SkBitmap());
    return;
  }
  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;
  bool check_rgb565_support = gl_helper->CanUseRgb565Readback();
  if ((bitmap_config == SkBitmap::kRGB_565_Config) &&
      !check_rgb565_support) {
    LOG(ERROR) << "Readbackformat rgb565  not supported";
    callback.Run(false, SkBitmap());
    return;
  }
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  gfx::Size dst_size_in_pixel =
      ConvertRectToPixel(device_scale_factor, gfx::Rect(dst_size)).size();
  gfx::Rect src_subrect_in_pixel =
      ConvertRectToPixel(device_scale_factor, src_subrect);

  if (using_synchronous_compositor_) {
    SynchronousCopyContents(src_subrect_in_pixel, dst_size_in_pixel, callback,
                            bitmap_config);
    UMA_HISTOGRAM_TIMES("Compositing.CopyFromSurfaceTimeSynchronous",
                        base::TimeTicks::Now() - start_time);
    return;
  }
  scoped_ptr<cc::CopyOutputRequest> request;
  if ((src_subrect_in_pixel.size() == dst_size_in_pixel) &&
      (bitmap_config == SkBitmap::kARGB_8888_Config)) {
      request = cc::CopyOutputRequest::CreateBitmapRequest(base::Bind(
          &RenderWidgetHostViewAndroid::PrepareBitmapCopyOutputResult,
          dst_size_in_pixel,
          bitmap_config,
          start_time,
          callback));
  } else {
      scoped_ptr<SkBitmap> bitmap;
      request = cc::CopyOutputRequest::CreateRequest(base::Bind(
          &RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult,
          dst_size_in_pixel,
          bitmap_config,
          start_time,
          Passed(&bitmap),
          callback));
  }
  request->set_area(src_subrect_in_pixel);
  layer_->RequestCopyOfOutput(request.Pass());
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool RenderWidgetHostViewAndroid::CanCopyToVideoFrame() const {
  return false;
}

void RenderWidgetHostViewAndroid::ShowDisambiguationPopup(
    const gfx::Rect& target_rect, const SkBitmap& zoomed_bitmap) {
  if (!content_view_core_)
    return;

  content_view_core_->ShowDisambiguationPopup(target_rect, zoomed_bitmap);
}

scoped_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAndroid::CreateSyntheticGestureTarget() {
  return scoped_ptr<SyntheticGestureTarget>(new SyntheticGestureTargetAndroid(
      host_, content_view_core_->CreateTouchEventSynthesizer()));
}

void RenderWidgetHostViewAndroid::OnAcceleratedCompositingStateChange() {
}

void RenderWidgetHostViewAndroid::SendDelegatedFrameAck(
    uint32 output_surface_id) {
  cc::CompositorFrameAck ack;
  if (resource_collection_.get())
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(host_->GetRoutingID(),
                                                   output_surface_id,
                                                   host_->GetProcess()->GetID(),
                                                   ack);
}

void RenderWidgetHostViewAndroid::SendReturnedDelegatedResources(
    uint32 output_surface_id) {
  DCHECK(resource_collection_);

  cc::CompositorFrameAck ack;
  resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  DCHECK(!ack.resources.empty());

  RenderWidgetHostImpl::SendReclaimCompositorResources(
      host_->GetRoutingID(),
      output_surface_id,
      host_->GetProcess()->GetID(),
      ack);
}

void RenderWidgetHostViewAndroid::UnusedResourcesAreAvailable() {
  if (ack_callbacks_.size())
    return;
  SendReturnedDelegatedResources(last_output_surface_id_);
}

void RenderWidgetHostViewAndroid::DestroyDelegatedContent() {
  RemoveLayers();
  frame_provider_ = NULL;
  delegated_renderer_layer_ = NULL;
  layer_ = NULL;
}

void RenderWidgetHostViewAndroid::SwapDelegatedFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::DelegatedFrameData> frame_data) {
  bool has_content = !texture_size_in_layer_.IsEmpty();

  if (output_surface_id != last_output_surface_id_) {
    // Drop the cc::DelegatedFrameResourceCollection so that we will not return
    // any resources from the old output surface with the new output surface id.
    if (resource_collection_.get()) {
      if (resource_collection_->LoseAllResources())
        SendReturnedDelegatedResources(last_output_surface_id_);

      resource_collection_->SetClient(NULL);
      resource_collection_ = NULL;
    }
    DestroyDelegatedContent();

    last_output_surface_id_ = output_surface_id;
  }

  if (!has_content) {
    DestroyDelegatedContent();
  } else {
    if (!resource_collection_.get()) {
      resource_collection_ = new cc::DelegatedFrameResourceCollection;
      resource_collection_->SetClient(this);
    }
    if (!frame_provider_ ||
        texture_size_in_layer_ != frame_provider_->frame_size()) {
      RemoveLayers();
      frame_provider_ = new cc::DelegatedFrameProvider(
          resource_collection_.get(), frame_data.Pass());
      delegated_renderer_layer_ =
          cc::DelegatedRendererLayer::Create(frame_provider_);
      layer_ = delegated_renderer_layer_;
      AttachLayers();
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
    if (content_view_core_) {
      SbrUIResourceLayerManager *ui_resource_mgr = static_cast<SbrContentViewCoreImpl*>(
                                                   content_view_core_)->GetUIResourceLayerManager();
      if(ui_resource_mgr){
        if(ui_resource_mgr->isAttached()){
          ui_resource_mgr->Detach();
        }
        ui_resource_mgr->Attach();
      }
    }
#endif
    } else {
      frame_provider_->SetFrameData(frame_data.Pass());
    }
  }

  if (delegated_renderer_layer_.get()) {
    delegated_renderer_layer_->SetDisplaySize(texture_size_in_layer_);
    delegated_renderer_layer_->SetIsDrawable(true);
    delegated_renderer_layer_->SetContentsOpaque(true);
    delegated_renderer_layer_->SetBounds(content_size_in_layer_);
    delegated_renderer_layer_->SetNeedsDisplay();

#if defined(SBROWSER_HIDE_URLBAR_HYBRID)
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
    if (content_view_core_) {
      SbrUIResourceLayerManager *ui_resource_mgr = static_cast<SbrContentViewCoreImpl*>(
                                                   content_view_core_)->GetUIResourceLayerManager();
      //Adjust delegated render layer when Bitmap Composition is enabled.
      if(ui_resource_mgr){
        delegated_renderer_layer_->SetPosition(gfx::PointF(current_content_offset_.x(), current_content_offset_.y()));  
      }
      //Adjust the root layer when Bitmap composition is disabled.
      else if (delegated_renderer_layer_->layer_tree_host()) {
        cc::Layer* root_layer = delegated_renderer_layer_->layer_tree_host()->root_layer();
        if (root_layer)
          root_layer->SetPosition(gfx::PointF(current_content_offset_.x(), current_content_offset_.y()));
      }
    }
#else
    //Adjust the root layer when Bitmap composition is disabled.
    if (delegated_renderer_layer_->layer_tree_host()) {
      cc::Layer* root_layer = delegated_renderer_layer_->layer_tree_host()->root_layer();
      if (root_layer)
        root_layer->SetPosition(gfx::PointF(current_content_offset_.x(), current_content_offset_.y()));
    }
#endif
#endif	
  }

  base::Closure ack_callback =
      base::Bind(&RenderWidgetHostViewAndroid::SendDelegatedFrameAck,
                 weak_ptr_factory_.GetWeakPtr(),
                 output_surface_id);

  if (host_->is_hidden())
    ack_callback.Run();
  else
    ack_callbacks_.push(ack_callback);
}

void RenderWidgetHostViewAndroid::ComputeContentsSize(
    const cc::CompositorFrameMetadata& frame_metadata) {
  // Calculate the content size.  This should be 0 if the texture_size is 0.
  gfx::Vector2dF offset;
  if (texture_size_in_layer_.GetArea() > 0)
    offset = frame_metadata.location_bar_content_translation;
  offset.set_y(offset.y() + frame_metadata.overdraw_bottom_height);
  offset.Scale(frame_metadata.device_scale_factor);
  content_size_in_layer_ =
      gfx::Size(texture_size_in_layer_.width() - offset.x(),
                texture_size_in_layer_.height() - offset.y());
  overscroll_effect_->UpdateDisplayParameters(
      CreateOverscrollDisplayParameters(frame_metadata));
  
#if defined(SBROWSER_HIDE_URLBAR_HYBRID)
  current_content_offset_= frame_metadata.location_bar_content_translation;
  current_content_offset_.Scale(frame_metadata.device_scale_factor);
#endif
}

void RenderWidgetHostViewAndroid::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  // Always let ContentViewCore know about the new frame first, so it can decide
  // to schedule a Draw immediately when it sees the texture layer invalidation.
  UpdateContentViewCoreFrameMetadata(frame->metadata);

  if (layer_ && layer_->layer_tree_host()) {
    for (size_t i = 0; i < frame->metadata.latency_info.size(); i++) {
      scoped_ptr<cc::SwapPromise> swap_promise(
          new cc::LatencyInfoSwapPromise(frame->metadata.latency_info[i]));
      layer_->layer_tree_host()->QueueSwapPromise(swap_promise.Pass());
    }
  }

  if (frame->delegated_frame_data) {
    DCHECK(using_delegated_renderer_);

    DCHECK(frame->delegated_frame_data);
    DCHECK(!frame->delegated_frame_data->render_pass_list.empty());

    cc::RenderPass* root_pass =
        frame->delegated_frame_data->render_pass_list.back();
    texture_size_in_layer_ = root_pass->output_rect.size();
    ComputeContentsSize(frame->metadata);

    SwapDelegatedFrame(output_surface_id, frame->delegated_frame_data.Pass());
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
    if (content_view_core_) {
      SbrUIResourceLayerManager *ui_resource_mgr = static_cast<SbrContentViewCoreImpl*>(
                                                   content_view_core_)->GetUIResourceLayerManager();
      if(ui_resource_mgr){

        ui_resource_mgr->SetPageScaleFactor(frame->metadata.page_scale_factor);

        ui_resource_mgr->SetTopControlsOffset(frame->metadata.location_bar_offset.y());
        ui_resource_mgr->UpdateUIResourceLayers();
        ui_resource_mgr->UpdateUIResourceWidgets();
      }
    }
#endif
    frame_evictor_->SwappedFrame(!host_->is_hidden());
    return;
  }

  DCHECK(!using_delegated_renderer_);

  if (!frame->gl_frame_data || frame->gl_frame_data->mailbox.IsZero())
    return;

  if (output_surface_id != last_output_surface_id_) {
    current_mailbox_ = gpu::Mailbox();
    last_output_surface_id_ = kUndefinedOutputSurfaceId;
  }

  base::Closure callback = base::Bind(&InsertSyncPointAndAckForCompositor,
                                      host_->GetProcess()->GetID(),
                                      output_surface_id,
                                      host_->GetRoutingID(),
                                      current_mailbox_,
                                      texture_size_in_layer_);
  ImageTransportFactoryAndroid::GetInstance()->WaitSyncPoint(
      frame->gl_frame_data->sync_point);

  texture_size_in_layer_ = frame->gl_frame_data->size;
  ComputeContentsSize(frame->metadata);

  BuffersSwapped(frame->gl_frame_data->mailbox, output_surface_id, callback);
  frame_evictor_->SwappedFrame(!host_->is_hidden());
}

void RenderWidgetHostViewAndroid::SynchronousFrameMetadata(
    const cc::CompositorFrameMetadata& frame_metadata) {
  // This is a subset of OnSwapCompositorFrame() used in the synchronous
  // compositor flow.
  UpdateContentViewCoreFrameMetadata(frame_metadata);
  ComputeContentsSize(frame_metadata);

  // DevTools ScreenCast support for Android WebView.
  if (DevToolsAgentHost::HasFor(RenderViewHost::From(GetRenderWidgetHost()))) {
    scoped_refptr<DevToolsAgentHost> dtah =
        DevToolsAgentHost::GetOrCreateFor(
            RenderViewHost::From(GetRenderWidgetHost()));
    // Unblock the compositor.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RenderViewDevToolsAgentHost::SynchronousSwapCompositorFrame,
                   static_cast<RenderViewDevToolsAgentHost*>(dtah.get()),
                   frame_metadata));
  }
}

void RenderWidgetHostViewAndroid::SetOverlayVideoMode(bool enabled) {
  if(layer_)
      layer_->SetContentsOpaque(!enabled);
}

void RenderWidgetHostViewAndroid::SynchronousCopyContents(
    const gfx::Rect& src_subrect_in_pixel,
    const gfx::Size& dst_size_in_pixel,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config config) {
  SynchronousCompositor* compositor =
      SynchronousCompositorImpl::FromID(host_->GetProcess()->GetID(),
                                        host_->GetRoutingID());
  if (!compositor) {
    callback.Run(false, SkBitmap());
    return;
  }

  SkBitmap bitmap;
  bitmap.setConfig(config,
                   dst_size_in_pixel.width(),
                   dst_size_in_pixel.height());
  bitmap.allocPixels();
  SkCanvas canvas(bitmap);
  canvas.scale(
      (float)dst_size_in_pixel.width() / (float)src_subrect_in_pixel.width(),
      (float)dst_size_in_pixel.height() / (float)src_subrect_in_pixel.height());
  compositor->DemandDrawSw(&canvas);
  callback.Run(true, bitmap);
}

void RenderWidgetHostViewAndroid::UpdateContentViewCoreFrameMetadata(
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (content_view_core_) {
    // All offsets and sizes are in CSS pixels.
    content_view_core_->UpdateFrameInfo(
        frame_metadata.root_scroll_offset,
        frame_metadata.page_scale_factor,
        gfx::Vector2dF(frame_metadata.min_page_scale_factor,
                       frame_metadata.max_page_scale_factor),
        frame_metadata.root_layer_size,
        frame_metadata.viewport_size,
        frame_metadata.location_bar_offset,
        frame_metadata.location_bar_content_translation,
        frame_metadata.overdraw_bottom_height);
  }
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceInitialized(int host_id,
                                                                int route_id) {
  accelerated_surface_route_id_ = route_id;
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  NOTREACHED() << "Need --composite-to-mailbox or --enable-delegated-renderer";
}

void RenderWidgetHostViewAndroid::BuffersSwapped(
    const gpu::Mailbox& mailbox,
    uint32_t output_surface_id,
    const base::Closure& ack_callback) {
  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();

  if (!texture_id_in_layer_) {
    texture_id_in_layer_ = factory->CreateTexture();
    texture_layer_->SetTextureId(texture_id_in_layer_);
    texture_layer_->SetIsDrawable(true);
    texture_layer_->SetContentsOpaque(true);
  }

  ImageTransportFactoryAndroid::GetInstance()->AcquireTexture(
      texture_id_in_layer_, mailbox.name);

  ResetClipping();

  current_mailbox_ = mailbox;
  last_output_surface_id_ = output_surface_id;

  if (host_->is_hidden())
    ack_callback.Run();
  else
    ack_callbacks_.push(ack_callback);
}

void RenderWidgetHostViewAndroid::AttachLayers() {
  if (!content_view_core_)
    return;
  if (!layer_.get())
    return;

  content_view_core_->AttachLayer(layer_);
  if (overscroll_effect_enabled_)
    overscroll_effect_->Enable();
  layer_->SetHideLayerAndSubtree(!is_showing_);
}

void RenderWidgetHostViewAndroid::RemoveLayers() {
  if (!content_view_core_)
    return;
  if (!layer_.get())
    return;

  content_view_core_->RemoveLayer(layer_);
  overscroll_effect_->Disable();
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
    if (content_view_core_) {
      SbrUIResourceLayerManager *ui_resource_mgr = static_cast<SbrContentViewCoreImpl*>(
                                                   content_view_core_)->GetUIResourceLayerManager();
      if(ui_resource_mgr && ui_resource_mgr->isAttached()){
        ui_resource_mgr->Detach();
      }
    }
#endif
}

bool RenderWidgetHostViewAndroid::Animate(base::TimeTicks frame_time) {
  return overscroll_effect_->Animate(frame_time);
}

void RenderWidgetHostViewAndroid::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceSuspend() {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceRelease() {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::EvictDelegatedFrame() {
  if (texture_id_in_layer_) {
    texture_layer_->SetTextureId(0);
    texture_layer_->SetIsDrawable(false);
    ImageTransportFactoryAndroid::GetInstance()->DeleteTexture(
        texture_id_in_layer_);
    texture_id_in_layer_ = 0;
    current_mailbox_ = gpu::Mailbox();
    last_output_surface_id_ = kUndefinedOutputSurfaceId;
  }
  if (delegated_renderer_layer_.get())
    DestroyDelegatedContent();
  frame_evictor_->DiscardedFrame();
}

bool RenderWidgetHostViewAndroid::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  NOTREACHED();
  return false;
}

void RenderWidgetHostViewAndroid::GetScreenInfo(blink::WebScreenInfo* result) {
  // ScreenInfo isn't tied to the widget on Android. Always return the default.
  RenderWidgetHostViewBase::GetDefaultScreenInfo(result);
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

gfx::GLSurfaceHandle RenderWidgetHostViewAndroid::GetCompositingSurface() {
  gfx::GLSurfaceHandle handle =
      gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_TRANSPORT);
  if (CompositorImpl::IsInitialized()) {
    handle.parent_client_id =
        ImageTransportFactoryAndroid::GetInstance()->GetChannelID();
  }
  return handle;
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  if (content_view_core_)
    content_view_core_->ConfirmTouchEvent(ack_result);
  
#if defined(S_NATIVE_SUPPORT)//additional action on touch ack
  if (content_view_core_) {
    static_cast<SbrContentViewCoreImpl*>(content_view_core_)->ConfirmTouchEvent(ack_result);
  }
#endif
}

void RenderWidgetHostViewAndroid::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::UnhandledWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  // intentionally empty, like RenderWidgetHostViewViews
}

void RenderWidgetHostViewAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (event.type == blink::WebInputEvent::GestureScrollEnd ||
      event.type == blink::WebInputEvent::GestureFlingStart) {
    OnOverscrolled(gfx::Vector2dF(), gfx::Vector2dF(), gfx::Vector2dF(), gfx::PointF());
  }
  if (content_view_core_)
    content_view_core_->OnGestureEventAck(event, ack_result);
}

InputEventAckState RenderWidgetHostViewAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (content_view_core_ &&
      content_view_core_->FilterInputEvent(input_event))
    return INPUT_EVENT_ACK_STATE_CONSUMED;

  if (!host_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  if (input_event.type == blink::WebInputEvent::GestureTapDown ||
      input_event.type == blink::WebInputEvent::TouchStart) {
    GpuDataManagerImpl* gpu_data = GpuDataManagerImpl::GetInstance();
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim && gpu_data && accelerated_surface_route_id_ &&
        gpu_data->IsDriverBugWorkaroundActive(gpu::WAKE_UP_GPU_BEFORE_DRAWING))
      shim->Send(
          new AcceleratedSurfaceMsg_WakeUpGpu(accelerated_surface_route_id_));
  }

  SynchronousCompositorImpl* compositor =
      SynchronousCompositorImpl::FromID(host_->GetProcess()->GetID(),
                                          host_->GetRoutingID());
  if (compositor)
    return compositor->HandleInputEvent(input_event);
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostViewAndroid::OnSetNeedsFlushInput() {
  if (flush_input_requested_ || !content_view_core_)
    return;
  TRACE_EVENT0("input", "RenderWidgetHostViewAndroid::OnSetNeedsFlushInput");
  flush_input_requested_ = true;
  content_view_core_->AddBeginFrameSubscriber();
}

void RenderWidgetHostViewAndroid::CreateBrowserAccessibilityManagerIfNeeded() {
  if (!host_ || host_->accessibility_mode() != AccessibilityModeComplete)
    return;

  if (!GetBrowserAccessibilityManager()) {
    base::android::ScopedJavaLocalRef<jobject> obj;
    if (content_view_core_)
      obj = content_view_core_->GetJavaObject();
    SetBrowserAccessibilityManager(
        new BrowserAccessibilityManagerAndroid(
            obj, BrowserAccessibilityManagerAndroid::GetEmptyDocument(), this));
  }
}

void RenderWidgetHostViewAndroid::SetAccessibilityFocus(int acc_obj_id) {
  if (!host_)
    return;

  host_->AccessibilitySetFocus(acc_obj_id);
}

void RenderWidgetHostViewAndroid::AccessibilityDoDefaultAction(int acc_obj_id) {
  if (!host_)
    return;

  host_->AccessibilityDoDefaultAction(acc_obj_id);
}

void RenderWidgetHostViewAndroid::AccessibilityScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  if (!host_)
    return;

  host_->AccessibilityScrollToMakeVisible(acc_obj_id, subfocus);
}

void RenderWidgetHostViewAndroid::AccessibilityScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  if (!host_)
    return;

  host_->AccessibilityScrollToPoint(acc_obj_id, point);
}

void RenderWidgetHostViewAndroid::AccessibilitySetTextSelection(
    int acc_obj_id, int start_offset, int end_offset) {
  if (!host_)
    return;

  host_->AccessibilitySetTextSelection(
      acc_obj_id, start_offset, end_offset);
}

gfx::Point RenderWidgetHostViewAndroid::GetLastTouchEventLocation() const {
  NOTIMPLEMENTED();
  // Only used on Win8
  return gfx::Point();
}

void RenderWidgetHostViewAndroid::FatalAccessibilityTreeError() {
  if (!host_)
    return;

  host_->FatalAccessibilityTreeError();
  SetBrowserAccessibilityManager(NULL);
}

bool RenderWidgetHostViewAndroid::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::UnlockMouse() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const NativeWebKeyboardEvent& event) {
  if (host_)
    host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewAndroid::SendTouchEvent(
    const blink::WebTouchEvent& event) {
  if (host_)
    host_->ForwardTouchEventWithLatencyInfo(event, CreateLatencyInfo(event));
}

void RenderWidgetHostViewAndroid::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  if (host_)
    host_->ForwardMouseEvent(event);
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (host_)
    host_->ForwardWheelEvent(event);
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_effect_enabled_)
   overscroll_effect_->Enable();

  if (host_)
    host_->ForwardGestureEventWithLatencyInfo(event, CreateLatencyInfo(event));
}

void RenderWidgetHostViewAndroid::SelectRange(const gfx::Point& start,
                                              const gfx::Point& end
#if defined(S_MULTISELECTION_BOUNDS)
		,bool isLastTouchPoint,bool isFirstTouchPoint
#endif
)
{  
  if (host_) {
    #if defined(S_MULTISELECTION_BOUNDS)
      host_->SelectRange(start, end, isLastTouchPoint,isFirstTouchPoint);
    #else
      host_->SelectRange(start, end);
    #endif
  }
}

void RenderWidgetHostViewAndroid::GetSelectionVisibilityStatus() {
  if (host_)
    host_->Send(new ViewMsg_GetSelectionVisibilityStatus(host_->GetRoutingID()));
}

void RenderWidgetHostViewAndroid::CheckBelongToSelection(int x, int y) {
  if (host_)
    host_->Send(new ViewMsg_CheckBelongToSelection(host_->GetRoutingID(), x, y));
}

void RenderWidgetHostViewAndroid::GetSelectionBitmap() {
  if (host_)
    host_->Send(new ViewMsg_GetSelectionBitmap(host_->GetRoutingID()));
}

void RenderWidgetHostViewAndroid::SelectClosestWord(int x, int y) {
  if (host_)
    host_->Send(new ViewMsg_SelectClosestWord(host_->GetRoutingID(), x, y));
}

void RenderWidgetHostViewAndroid::ClearTextSelection() {
  if (host_)
    host_->Send(new ViewMsg_ClearTextSelection(host_->GetRoutingID()));
}

void RenderWidgetHostViewAndroid::SelectLinkText(const gfx::Point& point) {
  if (host_)
    host_->Send(new ViewMsg_SelectLinkText(host_->GetRoutingID(), point));
}

void RenderWidgetHostViewAndroid::GetTouchedFixedElementHeight(int x,  int y){
	 if (host_)
   	 host_->Send(new ViewMsg_GetTouchedFixedElementHeight(host_->GetRoutingID(), x, y));
}

void RenderWidgetHostViewAndroid::GetBitmapFromCachedResource(
    const std::string& image_url) {
  if (host_)
    host_->Send(new ViewMsg_GetBitmapFromCachedResource(
        host_->GetRoutingID(), image_url));
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host_)
    host_->MoveCaret(point);
}

void RenderWidgetHostViewAndroid::RequestContentClipping(
    const gfx::Rect& clipping,
    const gfx::Size& content_size) {
  // A focused view provides its own clipping.
  if (HasFocus())
    return;

  ClipContents(clipping, content_size);
}

void RenderWidgetHostViewAndroid::RecognizeArticle(int mode) {
  if (host_)
    host_->Send(new ViewMsg_RecognizeArticle(host_->GetRoutingID(), mode));
}

void RenderWidgetHostViewAndroid::OnRecognizeArticleResult(std::string reader_result_str) {
#if defined(S_NATIVE_SUPPORT)
  if (content_view_core_)
    static_cast<SbrContentViewCoreImpl*>(content_view_core_)->
        OnRecognizeArticleResult(reader_result_str);
#endif
}

void RenderWidgetHostViewAndroid::ResetClipping() {
  ClipContents(gfx::Rect(gfx::Point(), content_size_in_layer_),
               content_size_in_layer_);
}

void RenderWidgetHostViewAndroid::ClipContents(const gfx::Rect& clipping,
                                               const gfx::Size& content_size) {
  if (!texture_id_in_layer_ || content_size_in_layer_.IsEmpty())
    return;

  gfx::Size clipped_content(content_size_in_layer_);
  clipped_content.SetToMin(clipping.size());
  texture_layer_->SetBounds(clipped_content);
  texture_layer_->SetNeedsDisplay();

  if (texture_size_in_layer_.IsEmpty()) {
    texture_layer_->SetUV(gfx::PointF(), gfx::PointF());
    return;
  }

  gfx::PointF offset(
      clipping.x() + content_size_in_layer_.width() - content_size.width(),
      clipping.y() + content_size_in_layer_.height() - content_size.height());
  offset.SetToMax(gfx::PointF());

  gfx::Vector2dF uv_scale(1.f / texture_size_in_layer_.width(),
                          1.f / texture_size_in_layer_.height());
  texture_layer_->SetUV(
      gfx::PointF(offset.x() * uv_scale.x(),
                  offset.y() * uv_scale.y()),
      gfx::PointF((offset.x() + clipped_content.width()) * uv_scale.x(),
                  (offset.y() + clipped_content.height()) * uv_scale.y()));
}

SkColor RenderWidgetHostViewAndroid::GetCachedBackgroundColor() const {
  return cached_background_color_;
}

void RenderWidgetHostViewAndroid::OnOverscrolled(
    gfx::Vector2dF accumulated_overscroll,
    gfx::Vector2dF latest_overscroll_delta,
    gfx::Vector2dF current_fling_velocity,
    gfx::PointF causal_event_viewport_point) {
  if (!content_view_core_ || !layer_ || !is_showing_)
    return;

  cc::Layer *root_layer = NULL;
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
  SbrUIResourceLayerManager *ui_resource_mgr = NULL;
  if (content_view_core_) 
    ui_resource_mgr = static_cast<SbrContentViewCoreImpl*>(content_view_core_)->GetUIResourceLayerManager();
  if(ui_resource_mgr)
    root_layer = delegated_renderer_layer_.get();
  else
#endif
    root_layer = content_view_core_->GetLayer();
	
  const float device_scale_factor = content_view_core_->GetDpiScale();

  if (overscroll_effect_->OnOverscrolled(
      root_layer,
      base::TimeTicks::Now(),
      gfx::ScaleVector2d(accumulated_overscroll, device_scale_factor),
      gfx::ScaleVector2d(latest_overscroll_delta, device_scale_factor),
      gfx::ScaleVector2d(current_fling_velocity, device_scale_factor),
      gfx::ScaleVector2d(causal_event_viewport_point.OffsetFromOrigin(),
                         device_scale_factor))) {
    content_view_core_->SetNeedsAnimate();
  }
}

void RenderWidgetHostViewAndroid::DidStopFlinging() {
  if (content_view_core_)
    content_view_core_->DidStopFlinging();
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  RunAckCallbacks();

  RemoveLayers();
  if (content_view_core_ && content_view_core_->GetWindowAndroid() && !using_synchronous_compositor_)
    content_view_core_->GetWindowAndroid()->RemoveObserver(this);

  content_view_core_ = content_view_core;

  if (GetBrowserAccessibilityManager()) {
    base::android::ScopedJavaLocalRef<jobject> obj;
    if (content_view_core_)
      obj = content_view_core_->GetJavaObject();
    GetBrowserAccessibilityManager()->ToBrowserAccessibilityManagerAndroid()->
        SetContentViewCore(obj);
  }

  AttachLayers();
  if (content_view_core_ && content_view_core_->GetWindowAndroid() && !using_synchronous_compositor_)
    content_view_core_->GetWindowAndroid()->AddObserver(this);

  if (!content_view_core_)
    overscroll_effect_.reset();
  else if (overscroll_effect_enabled_ && !overscroll_effect_)
    overscroll_effect_ = OverscrollGlow::Create(overscroll_effect_enabled_);
}

void RenderWidgetHostViewAndroid::RunAckCallbacks() {
  while (!ack_callbacks_.empty()) {
    ack_callbacks_.front().Run();
    ack_callbacks_.pop();
  }
}

void RenderWidgetHostViewAndroid::OnCompositingDidCommit() {
  RunAckCallbacks();
}

void RenderWidgetHostViewAndroid::OnDetachCompositor() {
  DCHECK(content_view_core_);
  DCHECK(!using_synchronous_compositor_);
  RunAckCallbacks();
}

void RenderWidgetHostViewAndroid::OnLostResources() {
  if (texture_layer_.get())
    texture_layer_->SetIsDrawable(false);
  if (delegated_renderer_layer_.get())
    DestroyDelegatedContent();
  texture_id_in_layer_ = 0;
  RunAckCallbacks();
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceToSkBitmap(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config bitmap_config,
    scoped_ptr<SkBitmap> bitmap) {
  // Only ARGB888 and RGB565 supported as of now.
  bool format_support = ((bitmap_config == SkBitmap::kRGB_565_Config) ||
                         (bitmap_config == SkBitmap::kARGB_8888_Config));
  if (!format_support) {
    DCHECK(format_support);
    callback.Run(false, SkBitmap());
    return;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (!using_synchronous_compositor_ && !IsSurfaceAvailableForCopy()) {
    callback.Run(false, SkBitmap());
    return;
  }
  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;
  bool check_rgb565_support = gl_helper->CanUseRgb565Readback();
  if ((bitmap_config == SkBitmap::kRGB_565_Config) &&
      !check_rgb565_support) {
    LOG(ERROR) << "Readbackformat rgb565  not supported";
    callback.Run(false, SkBitmap());
    return;
  }
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  gfx::Size dst_size_in_pixel =
      ConvertRectToPixel(device_scale_factor, gfx::Rect(dst_size)).size();
  gfx::Rect src_subrect_in_pixel =
      ConvertRectToPixel(device_scale_factor, src_subrect);

  if (using_synchronous_compositor_) {
    SynchronousCopyContents(src_subrect_in_pixel, dst_size_in_pixel, callback,
                            bitmap_config);
    UMA_HISTOGRAM_TIMES("Compositing.CopyFromSurfaceTimeSynchronous",
                        base::TimeTicks::Now() - start_time);
    return;
  }
  scoped_ptr<cc::CopyOutputRequest> request;
  if ((src_subrect_in_pixel.size() == dst_size_in_pixel) &&
      (bitmap_config == SkBitmap::kARGB_8888_Config)) {
      request = cc::CopyOutputRequest::CreateBitmapRequest(base::Bind(
          &RenderWidgetHostViewAndroid::PrepareBitmapCopyOutputResult,
          dst_size_in_pixel,
          bitmap_config,
          start_time,
          callback));
  } else {
      request = cc::CopyOutputRequest::CreateRequest(base::Bind(
          &RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult,
          dst_size_in_pixel,
          bitmap_config,
          start_time,
          Passed(&bitmap),
          callback));
  }
  request->set_area(src_subrect_in_pixel);
  layer_->RequestCopyOfOutput(request.Pass());
}

// static
void RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkBitmap::Config bitmap_config,
    const base::TimeTicks& start_time,
    scoped_ptr<SkBitmap> bitmap,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));

  if (!result->HasTexture() || result->IsEmpty() || result->size().IsEmpty())
    return;
  if (!bitmap.get()) {
    bitmap.reset(new SkBitmap);
    bitmap->setConfig(bitmap_config,
                      dst_size_in_pixel.width(),
                      dst_size_in_pixel.height(),
                      0, kOpaque_SkAlphaType);
    if (!bitmap->allocPixels())
      return;
  }

  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8* pixels = static_cast<uint8*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(),
      texture_mailbox.sync_point(),
      result->size(),
      gfx::Rect(result->size()),
      dst_size_in_pixel,
      pixels,
      bitmap_config,
      base::Bind(&CopyFromCompositingSurfaceFinished,
                 callback,
                 base::Passed(&release_callback),
                 base::Passed(&bitmap),
                 start_time,
                 base::Passed(&bitmap_pixels_lock)));
}

// static
void RenderWidgetHostViewAndroid::PrepareBitmapCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkBitmap::Config config,
    const base::TimeTicks& start_time,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (config != SkBitmap::kARGB_8888_Config) {
    NOTIMPLEMENTED();
    callback.Run(false, SkBitmap());
    return;
  }
  DCHECK(result->HasBitmap());
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));

  if (!result->HasBitmap() || result->IsEmpty() || result->size().IsEmpty())
    return;

  scoped_ptr<SkBitmap> source = result->TakeBitmap();
  DCHECK(source);
  if (!source)
    return;

  DCHECK_EQ(source->width(), dst_size_in_pixel.width());
  DCHECK_EQ(source->height(), dst_size_in_pixel.height());

  ignore_result(scoped_callback_runner.Release());
  UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                      base::TimeTicks::Now() - start_time);

  callback.Run(true, *source);
}

bool RenderWidgetHostViewAndroid::CompositeAndReadback(void *pixels,
                                                  const gfx::Rect& rect) {
  if (!delegated_renderer_layer_.get() || !using_delegated_renderer_)
    return false;

  return delegated_renderer_layer_->CompositeAndReadback(pixels,
                                                         rect);
}

void RenderWidgetHostViewAndroid::GetBitmapFromRenderer(
    const gfx::Rect& src_subrect,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    float page_scale_factor) {
  if (!host_) {
    callback.Run(false, SkBitmap());
    return;
  }
  host_->GetBitmapFromRenderer(src_subrect, callback, page_scale_factor);
}

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(
    blink::WebScreenInfo* results) {
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  results->rect = display.bounds();
  // TODO(husky): Remove any system controls from availableRect.
  results->availableRect = display.work_area();
  results->deviceScaleFactor = display.device_scale_factor();
  gfx::DeviceDisplayInfo info;
  results->depth = info.GetBitsPerPixel();
  results->depthPerComponent = info.GetBitsPerComponent();
  results->isMonochrome = (results->depthPerComponent == 0);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView*
RenderWidgetHostView::CreateViewForWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  return new RenderWidgetHostViewAndroid(rwhi, NULL);
}

void RenderWidgetHostViewAndroid::OnSSRMModeCallback(int SSRMCaller, int count) {
#if defined(S_NATIVE_SUPPORT)
  if (content_view_core_) {
   static_cast<SbrContentViewCoreImpl*>(content_view_core_)->OnSSRMModeCallback(SSRMCaller, count);
  }
#endif
}

void RenderWidgetHostViewAndroid::FlushPendingCallbacks() {
  if (!host_) { 
    return;
  }
  host_->FlushPendingCallbacks();
}

} // namespace content
