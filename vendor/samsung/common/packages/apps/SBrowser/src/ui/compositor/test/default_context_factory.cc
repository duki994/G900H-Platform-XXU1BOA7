// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/default_context_factory.h"

#include "cc/output/output_surface.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace ui {

DefaultContextFactory::DefaultContextFactory() {
  DCHECK_NE(gfx::GetGLImplementation(), gfx::kGLImplementationNone);
}

DefaultContextFactory::~DefaultContextFactory() {
}

scoped_ptr<cc::OutputSurface> DefaultContextFactory::CreateOutputSurface(
    Compositor* compositor, bool software_fallback) {
  DCHECK(!software_fallback);
  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.shareResources = true;

  bool lose_context_when_out_of_memory = true;
  using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context3d(
      WebGraphicsContext3DInProcessCommandBufferImpl::CreateViewContext(
          attrs, 
      #if defined(S_PLM_P140603_03145)
      lose_context_when_out_of_memory,
      #endif
    compositor->widget()));
  CHECK(context3d);

  using webkit::gpu::ContextProviderInProcess;
  scoped_refptr<ContextProviderInProcess> context_provider =
      ContextProviderInProcess::Create(context3d.Pass(),
                                       "UICompositor");

  return make_scoped_ptr(new cc::OutputSurface(context_provider));
}

scoped_refptr<Reflector> DefaultContextFactory::CreateReflector(
    Compositor* mirroed_compositor,
    Layer* mirroring_layer) {
  return NULL;
}

void DefaultContextFactory::RemoveReflector(
    scoped_refptr<Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
DefaultContextFactory::OffscreenCompositorContextProvider() {
  if (!offscreen_compositor_contexts_.get() ||
      !offscreen_compositor_contexts_->DestroyedOnMainThread()) {
    #if defined(S_PLM_P140603_03145)
    bool lose_context_when_out_of_memory = true;
     offscreen_compositor_contexts_ = webkit::gpu::ContextProviderInProcess::CreateOffscreen(
      lose_context_when_out_of_memory);
    #else
    offscreen_compositor_contexts_ =
        webkit::gpu::ContextProviderInProcess::CreateOffscreen();
   #endif
  }
  return offscreen_compositor_contexts_;
}

scoped_refptr<cc::ContextProvider>
DefaultContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->DestroyedOnMainThread())
    return shared_main_thread_contexts_;

  if (ui::Compositor::WasInitializedWithThread()) {
    #if defined(S_PLM_P140603_03145)
	  bool lose_context_when_out_of_memory = false;
     shared_main_thread_contexts_ = webkit::gpu::ContextProviderInProcess::CreateOffscreen(
            lose_context_when_out_of_memory);
    #else
    shared_main_thread_contexts_ =
        webkit::gpu::ContextProviderInProcess::CreateOffscreen();
   #endif
  } else {
    shared_main_thread_contexts_ =
        static_cast<webkit::gpu::ContextProviderInProcess*>(
            OffscreenCompositorContextProvider().get());
  }
  if (shared_main_thread_contexts_ &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void DefaultContextFactory::RemoveCompositor(Compositor* compositor) {
}

bool DefaultContextFactory::DoesCreateTestContexts() { return false; }

}  // namespace ui
