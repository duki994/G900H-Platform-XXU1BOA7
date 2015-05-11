// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_in_process_context_provider.h"

#include "base/lazy_instance.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {

// static
scoped_ptr<gpu::GLInProcessContext> CreateTestInProcessContext() {
  const bool is_offscreen = true;
  const bool share_resources = true;
  gpu::GLInProcessContextAttribs attribs;
  attribs.alpha_size = 8;
  attribs.blue_size = 8;
  attribs.green_size = 8;
  attribs.red_size = 8;
  attribs.depth_size = 24;
  attribs.stencil_size = 8;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  scoped_ptr<gpu::GLInProcessContext> context = make_scoped_ptr(
      gpu::GLInProcessContext::CreateContext(is_offscreen,
                                             gfx::AcceleratedWidget(),
                                             gfx::Size(1, 1),
                                             share_resources,
                                             attribs,
                                             gpu_preference));
  DCHECK(context);
  return context.Pass();
}

TestInProcessContextProvider::TestInProcessContextProvider()
    : context_(CreateTestInProcessContext()) {}

TestInProcessContextProvider::~TestInProcessContextProvider() {
  if (gr_context_)
    gr_context_->contextDestroyed();
}

bool TestInProcessContextProvider::BindToCurrentThread() { return true; }

gpu::gles2::GLES2Interface* TestInProcessContextProvider::ContextGL() {
  return context_->GetImplementation();
}

gpu::ContextSupport* TestInProcessContextProvider::ContextSupport() {
  return context_->GetImplementation();
}

namespace {

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() { ::gles2::Initialize(); }

  ~GLES2Initializer() { ::gles2::Terminate(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

static base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

static void BindGrContextCallback(const GrGLInterface* interface) {
  TestInProcessContextProvider* context_provider =
      reinterpret_cast<TestInProcessContextProvider*>(interface->fCallbackData);

  // Make sure the gles2 library is initialized first on exactly one thread.
  g_gles2_initializer.Get();
  gles2::SetGLContext(context_provider->ContextGL());
}

class GrContext* TestInProcessContextProvider::GrContext() {
  if (gr_context_)
    return gr_context_.get();

  skia::RefPtr<GrGLInterface> interface =
      skia::AdoptRef(skia_bindings::CreateCommandBufferSkiaGLBinding());
  interface->fCallback = BindGrContextCallback;
  interface->fCallbackData = reinterpret_cast<GrGLInterfaceCallbackData>(this);

  gr_context_ = skia::AdoptRef(GrContext::Create(
      kOpenGL_GrBackend, reinterpret_cast<GrBackendContext>(interface.get())));

  return gr_context_.get();
}

ContextProvider::Capabilities
TestInProcessContextProvider::ContextCapabilities() {
  return ContextProvider::Capabilities();
}

bool TestInProcessContextProvider::IsContextLost() { return false; }

void TestInProcessContextProvider::VerifyContexts() {}

bool TestInProcessContextProvider::DestroyedOnMainThread() { return false; }

void TestInProcessContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {}

void TestInProcessContextProvider::SetMemoryPolicyChangedCallback(
    const MemoryPolicyChangedCallback& memory_policy_changed_callback) {}

}  // namespace cc
