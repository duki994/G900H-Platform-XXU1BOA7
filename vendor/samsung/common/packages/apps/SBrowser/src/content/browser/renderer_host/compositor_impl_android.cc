// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <map>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/layer_tree_host.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/base/android/window_android.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/frame_time.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace gfx {
class JavaBitmap;
}

namespace {

// Used for drawing directly to the screen. Bypasses resizing and swaps.
class DirectOutputSurface : public cc::OutputSurface {
 public:
  DirectOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context_provider)
      : cc::OutputSurface(context_provider) {
    capabilities_.adjust_deadline_for_parent = false;
  }

  virtual void Reshape(const gfx::Size& size, float scale_factor) OVERRIDE {
    surface_size_ = size;
  }
  virtual void SwapBuffers(cc::CompositorFrame*) OVERRIDE {
    context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  }
};

// Used to override capabilities_.adjust_deadline_for_parent to false
class OutputSurfaceWithoutParent : public cc::OutputSurface {
 public:
  OutputSurfaceWithoutParent(const scoped_refptr<
      content::ContextProviderCommandBuffer>& context_provider)
      : cc::OutputSurface(context_provider) {
    capabilities_.adjust_deadline_for_parent = false;
  }

  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE {
    content::ContextProviderCommandBuffer* provider_command_buffer =
        static_cast<content::ContextProviderCommandBuffer*>(
            context_provider_.get());
    content::CommandBufferProxyImpl* command_buffer_proxy =
        provider_command_buffer->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    command_buffer_proxy->SetLatencyInfo(frame->metadata.latency_info);

    OutputSurface::SwapBuffers(frame);
  }
};

class TransientUIResource : public cc::ScopedUIResource {
 public:
  static scoped_ptr<TransientUIResource> Create(
      cc::LayerTreeHost* host,
      const cc::UIResourceBitmap& bitmap) {
    return make_scoped_ptr(new TransientUIResource(host, bitmap));
  }

  virtual cc::UIResourceBitmap GetBitmap(cc::UIResourceId uid,
                                         bool resource_lost) OVERRIDE {
    if (!retrieved_) {
      cc::UIResourceBitmap old_bitmap(bitmap_);

      // Return a place holder for all following calls to GetBitmap.
      SkBitmap tiny_bitmap;
      SkCanvas canvas(tiny_bitmap);
      tiny_bitmap.setConfig(
          SkBitmap::kARGB_8888_Config, 1, 1, 0, kOpaque_SkAlphaType);
      tiny_bitmap.allocPixels();
      canvas.drawColor(SK_ColorWHITE);
      tiny_bitmap.setImmutable();

      // Release our reference of the true bitmap.
      bitmap_ = cc::UIResourceBitmap(tiny_bitmap);

      retrieved_ = true;
      return old_bitmap;
    }
    return bitmap_;
  }

 protected:
  TransientUIResource(cc::LayerTreeHost* host,
                      const cc::UIResourceBitmap& bitmap)
      : cc::ScopedUIResource(host, bitmap), retrieved_(false) {}

 private:
  bool retrieved_;
};

static bool g_initialized = false;

} // anonymous namespace

namespace content {

typedef std::map<int, base::android::ScopedJavaGlobalRef<jobject> >
    SurfaceMap;
static base::LazyInstance<SurfaceMap>
    g_surface_map = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::Lock> g_surface_map_lock;

// static
Compositor* Compositor::Create(CompositorClient* client,
                               gfx::NativeWindow root_window) {
  return client ? new CompositorImpl(client, root_window) : NULL;
}

// static
void Compositor::Initialize() {
  DCHECK(!CompositorImpl::IsInitialized());
  g_initialized = true;
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

// static
jobject CompositorImpl::GetSurface(int surface_id) {
  base::AutoLock lock(g_surface_map_lock.Get());
  SurfaceMap* surfaces = g_surface_map.Pointer();
  SurfaceMap::iterator it = surfaces->find(surface_id);
  jobject jsurface = it == surfaces->end() ? NULL : it->second.obj();

  LOG_IF(WARNING, !jsurface) << "No surface for surface id " << surface_id;
  return jsurface;
}

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : root_layer_(cc::Layer::Create()),
      has_transparent_background_(false),
      device_scale_factor_(1),
      window_(NULL),
      surface_id_(0),
      client_(client),
      root_window_(root_window)
#if defined (S_PLM_P140621_01532)
      ,weak_factory_(this)
#endif
	  {
  DCHECK(client);
  DCHECK(root_window);
  ImageTransportFactoryAndroid::AddObserver(this);
  root_window->AttachCompositor();
}

CompositorImpl::~CompositorImpl() {
  root_window_->DetachCompositor();
  ImageTransportFactoryAndroid::RemoveObserver(this);
  // Clean-up any surface references.
  SetSurface(NULL);
}

void CompositorImpl::Composite() {
#if defined (S_PLM_P140621_01532)
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  if (!factory->GetGpuChannel() || factory->GetGpuChannel()->IsLost()) {
    CauseForGpuLaunch cause =
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
    factory->EstablishGpuChannel(
        cause,
        base::Bind(&CompositorImpl::OnGpuChannelEstablished,
                   weak_factory_.GetWeakPtr()));
    return;
  }
#endif
  if (host_)
    host_->Composite(gfx::FrameTime::Now());
}
#if defined (S_PLM_P140621_01532)
void CompositorImpl::OnGpuChannelEstablished() {
  ScheduleComposite();
}
#endif
void CompositorImpl::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  root_layer_->RemoveAllChildren();
  root_layer_->AddChild(root_layer);
}

void CompositorImpl::SetWindowSurface(ANativeWindow* window) {
  GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

  if (window_) {
    tracker->RemoveSurface(surface_id_);
    ANativeWindow_release(window_);
    window_ = NULL;
    surface_id_ = 0;
    SetVisible(false);
  }

  if (window) {
    window_ = window;
    ANativeWindow_acquire(window);
    surface_id_ = tracker->AddSurfaceForNativeWidget(window);
    tracker->SetSurfaceHandle(
        surface_id_,
        gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_DIRECT));
    SetVisible(true);
  }
}

void CompositorImpl::SetSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_surface(env, surface);

  // First, cleanup any existing surface references.
  if (surface_id_) {
    DCHECK(g_surface_map.Get().find(surface_id_) !=
           g_surface_map.Get().end());
    base::AutoLock lock(g_surface_map_lock.Get());
    g_surface_map.Get().erase(surface_id_);
  }
  SetWindowSurface(NULL);

  // Now, set the new surface if we have one.
  ANativeWindow* window = NULL;
  if (surface)
    window = ANativeWindow_fromSurface(env, surface);
  if (window) {
    SetWindowSurface(window);
    ANativeWindow_release(window);
    {
      base::AutoLock lock(g_surface_map_lock.Get());
      g_surface_map.Get().insert(std::make_pair(surface_id_, j_surface));
    }
  }
}

void CompositorImpl::SetVisible(bool visible) {
  if (!visible) {
    ui_resource_map_.clear();
    host_.reset();
    client_->UIResourcesAreInvalid();
  } else if (!host_) {
    cc::LayerTreeSettings settings;
    settings.refresh_rate = 60.0;
    settings.impl_side_painting = false;
    settings.allow_antialiasing = false;
    settings.calculate_top_controls_position = false;
    settings.top_controls_height = 0.f;
    settings.use_memory_management = false;
    settings.highp_threshold_min = 2048;

    host_ = cc::LayerTreeHost::CreateSingleThreaded(this, this, NULL, settings);
    host_->SetRootLayer(root_layer_);

    host_->SetVisible(true);
    host_->SetLayerTreeHostClientReady();
    host_->SetViewportSize(size_);
    host_->set_has_transparent_background(has_transparent_background_);
    host_->SetDeviceScaleFactor(device_scale_factor_);
    // Need to recreate the UI resources because a new LayerTreeHost has been
    // created.
    client_->DidLoseUIResources();
  }
}

void CompositorImpl::setDeviceScaleFactor(float factor) {
  device_scale_factor_ = factor;
  if (host_)
    host_->SetDeviceScaleFactor(factor);
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_)
    host_->SetViewportSize(size);
  root_layer_->SetBounds(size);
}

void CompositorImpl::SetHasTransparentBackground(bool flag) {
  has_transparent_background_ = flag;
  if (host_)
    host_->set_has_transparent_background(flag);
}

bool CompositorImpl::CompositeAndReadback(void *pixels, const gfx::Rect& rect) {
  if (host_)
    return host_->CompositeAndReadback(pixels, rect);
  else
    return false;
}

cc::UIResourceId CompositorImpl::GenerateUIResourceFromUIResourceBitmap(
    const cc::UIResourceBitmap& bitmap,
    bool is_transient) {
  if (!host_)
    return 0;

  cc::UIResourceId id = 0;
  scoped_ptr<cc::UIResourceClient> resource;
  if (is_transient) {
    scoped_ptr<TransientUIResource> transient_resource =
        TransientUIResource::Create(host_.get(), bitmap);
    id = transient_resource->id();
    resource = transient_resource.Pass();
  } else {
    scoped_ptr<cc::ScopedUIResource> scoped_resource =
        cc::ScopedUIResource::Create(host_.get(), bitmap);
    id = scoped_resource->id();
    resource = scoped_resource.Pass();
  }

  ui_resource_map_.set(id, resource.Pass());
  return id;
}

cc::UIResourceId CompositorImpl::GenerateUIResource(const SkBitmap& bitmap,
                                                    bool is_transient) {
  return GenerateUIResourceFromUIResourceBitmap(cc::UIResourceBitmap(bitmap),
                                                is_transient);
}

cc::UIResourceId CompositorImpl::GenerateCompressedUIResource(
    const gfx::Size& size,
    void* pixels,
    bool is_transient) {
  DCHECK_LT(0, size.width());
  DCHECK_LT(0, size.height());
  DCHECK_EQ(0, size.width() % 4);
  DCHECK_EQ(0, size.height() % 4);

  size_t data_size = size.width() * size.height() / 2;
  SkImageInfo info = {size.width(), size.height() / 2, kAlpha_8_SkColorType,
                      kPremul_SkAlphaType};
  skia::RefPtr<SkMallocPixelRef> etc1_pixel_ref =
      skia::AdoptRef(SkMallocPixelRef::NewAllocate(info, 0, 0));
  memcpy(etc1_pixel_ref->getAddr(), pixels, data_size);
  etc1_pixel_ref->setImmutable();
  return GenerateUIResourceFromUIResourceBitmap(
      cc::UIResourceBitmap(etc1_pixel_ref, size), is_transient);
}

void CompositorImpl::DeleteUIResource(cc::UIResourceId resource_id) {
  UIResourceMap::iterator it = ui_resource_map_.find(resource_id);
  if (it != ui_resource_map_.end())
    ui_resource_map_.erase(it);
}

static scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
CreateGpuProcessViewContext(
#if defined (S_PLM_P140621_01532)
    const scoped_refptr<GpuChannelHost>& gpu_channel_host,
#endif	
    const blink::WebGraphicsContext3D::Attributes attributes,
    int surface_id) {
#if !defined (S_PLM_P140621_01532)	
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      factory->EstablishGpuChannelSync(cause));
  if (!gpu_channel_host)
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
#else
  DCHECK(gpu_channel_host);
#endif
  GURL url("chrome://gpu/Compositor::createContext3D");
  static const size_t kBytesPerPixel = 4;
  gfx::DeviceDisplayInfo display_info;
  size_t full_screen_texture_size_in_bytes =
      display_info.GetDisplayHeight() *
      display_info.GetDisplayWidth() *
      kBytesPerPixel;
  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
  limits.command_buffer_size = 64 * 1024;
  limits.start_transfer_buffer_size = 64 * 1024;
  limits.min_transfer_buffer_size = 64 * 1024;
  limits.max_transfer_buffer_size = std::min(
      3 * full_screen_texture_size_in_bytes, kDefaultMaxTransferBufferSize);
  limits.mapped_memory_reclaim_limit = 2 * 1024 * 1024;
  #if defined(S_PLM_P140603_03145)
  bool bind_generates_resource = false;
  bool lose_context_when_out_of_memory = true;
  #endif
  return make_scoped_ptr(
      new WebGraphicsContext3DCommandBufferImpl(surface_id,
                                                url,
                                                gpu_channel_host.get(),
                                                attributes,
                                                #if defined(S_PLM_P140603_03145)
                                                bind_generates_resource,
                                                lose_context_when_out_of_memory,
                                                #else
                                                false,
                                                #endif
                                                limits));
}

scoped_ptr<cc::OutputSurface> CompositorImpl::CreateOutputSurface(
    bool fallback) {
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  attrs.noAutomaticFlushes = true;

  DCHECK(window_);
  DCHECK(surface_id_);
#if !defined (S_PLM_P140621_01532)
  scoped_refptr<ContextProviderCommandBuffer> context_provider =
      ContextProviderCommandBuffer::Create(
          CreateGpuProcessViewContext(attrs, surface_id_), "BrowserCompositor");
  if (!context_provider.get()) {
    LOG(ERROR) << "Failed to create 3D context for compositor.";
    return scoped_ptr<cc::OutputSurface>();
  }
#else  
  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  scoped_refptr<GpuChannelHost> gpu_channel_host = factory->GetGpuChannel();
  if (gpu_channel_host && !gpu_channel_host->IsLost()) {
    context_provider = ContextProviderCommandBuffer::Create(
        CreateGpuProcessViewContext(gpu_channel_host, attrs, surface_id_),
        "BrowserCompositor");
  }
#endif
  return scoped_ptr<cc::OutputSurface>(
      new OutputSurfaceWithoutParent(context_provider));
}

void CompositorImpl::OnLostResources() {
  client_->DidLoseResources();
}

scoped_refptr<cc::ContextProvider> CompositorImpl::OffscreenContextProvider() {
  // There is no support for offscreen contexts, or compositor filters that
  // would require them in this compositor instance. If they are needed,
  // then implement a context provider that provides contexts from
  // ImageTransportSurfaceAndroid.
  return NULL;
}

void CompositorImpl::DidCompleteSwapBuffers() {
  client_->OnSwapBuffersCompleted();
}

void CompositorImpl::ScheduleComposite() {
  client_->ScheduleComposite();
}

void CompositorImpl::ScheduleAnimation() {
  ScheduleComposite();
}

void CompositorImpl::DidPostSwapBuffers() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidPostSwapBuffers");
  client_->OnSwapBuffersPosted();
}

void CompositorImpl::DidAbortSwapBuffers() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidAbortSwapBuffers");
  client_->OnSwapBuffersCompleted();
}

void CompositorImpl::DidCommit() {
  root_window_->OnCompositingDidCommit();
}

} // namespace content
