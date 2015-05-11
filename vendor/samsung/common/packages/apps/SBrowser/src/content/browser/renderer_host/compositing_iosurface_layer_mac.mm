// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_layer_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/gl.h>

#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gpu_switching_manager.h"

@implementation CompositingIOSurfaceLayer

@synthesize context = context_;

- (id)initWithRenderWidgetHostViewMac:(content::RenderWidgetHostViewMac*)r {
  if (self = [super init]) {
    renderWidgetHostView_ = r;
    context_ = content::CompositingIOSurfaceContext::Get(
        content::CompositingIOSurfaceContext::kOffscreenContextWindowNumber);
    DCHECK(context_);
    needsDisplay_ = NO;

    ScopedCAActionDisabler disabler;
    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setContentsGravity:kCAGravityTopLeft];
    [self setFrame:NSRectToCGRect(
        [renderWidgetHostView_->cocoa_view() bounds])];
    [self setNeedsDisplay];
    [self updateScaleFactor];
  }
  return self;
}

- (void)updateScaleFactor {
  if (!renderWidgetHostView_ ||
      ![self respondsToSelector:(@selector(contentsScale))] ||
      ![self respondsToSelector:(@selector(setContentsScale:))])
    return;

  float current_scale_factor = [self contentsScale];
  float new_scale_factor = current_scale_factor;
  if (renderWidgetHostView_->compositing_iosurface_) {
    new_scale_factor =
        renderWidgetHostView_->compositing_iosurface_->scale_factor();
  }

  if (new_scale_factor == current_scale_factor)
    return;

  ScopedCAActionDisabler disabler;
  [self setContentsScale:new_scale_factor];
}

- (void)disableCompositing{
  ScopedCAActionDisabler disabler;
  [self removeFromSuperlayer];
  renderWidgetHostView_ = nil;
}

- (void)gotNewFrame {
  if (![self isAsynchronous]) {
    [self setNeedsDisplay];
    [self setAsynchronous:YES];
  } else {
    needsDisplay_ = YES;
  }
}

- (void)timerSinceGotNewFrameFired {
  if (![self isAsynchronous])
    return;

  [self setAsynchronous:NO];
  if (needsDisplay_)
    [self setNeedsDisplay];
}

// The remaining methods implement the CAOpenGLLayer interface.

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  if (!context_)
    return [super copyCGLPixelFormatForDisplayMask:mask];
  return CGLRetainPixelFormat(CGLGetPixelFormat(context_->cgl_context()));
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (!context_)
    return [super copyCGLContextForPixelFormat:pixelFormat];
  return CGLRetainContext(context_->cgl_context());
}

- (void)setNeedsDisplay {
  needsDisplay_ = YES;
  [super setNeedsDisplay];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  return needsDisplay_;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "CompositingIOSurfaceLayer::drawInCGLContext");

  if (!context_ ||
      (context_ && context_->cgl_context() != glContext) ||
      !renderWidgetHostView_ ||
      !renderWidgetHostView_->compositing_iosurface_) {
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }

  // Cache a copy of renderWidgetHostView_ because it may be reset if
  // a software frame is received in GetBackingStore.
  content::RenderWidgetHostViewMac* cached_view = renderWidgetHostView_;

  // If a resize is in progress then GetBackingStore request a frame of the
  // current window size and block until a frame of the right size comes in.
  // This makes the window content not lag behind the resize (at the cost of
  // blocking on the browser's main thread).
  if (cached_view->render_widget_host_) {
    // Note that GetBackingStore can potentially spawn a nested run loop, which
    // may change the current GL context, or, because the GL contexts are
    // shared, may change the currently-bound FBO. Ensure that, when the run
    // loop returns, the original GL context remain current, and the original
    // FBO remain bound.
    // TODO(ccameron): This is far too fragile a mechanism to rely on. Find
    // a way to avoid doing this.
    GLuint previous_framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,
                  reinterpret_cast<GLint*>(&previous_framebuffer));
    {
      gfx::ScopedCGLSetCurrentContext scoped_set_current_context(NULL);
      cached_view->about_to_validate_and_paint_ = true;
      (void)cached_view->render_widget_host_->GetBackingStore(true);
      cached_view->about_to_validate_and_paint_ = false;
    }
    CHECK_EQ(CGLGetCurrentContext(), glContext)
        << "original GL context failed to re-bind after nested run loop, "
        << "browser crash is imminent.";
    glBindFramebuffer(GL_FRAMEBUFFER, previous_framebuffer);
  }

  // If a transition to software mode has occurred, this layer should be
  // removed from the heirarchy now, so don't draw anything.
  if (!renderWidgetHostView_)
    return;

  gfx::Rect window_rect([self frame]);
  float window_scale_factor = 1.f;
  if ([self respondsToSelector:(@selector(contentsScale))])
    window_scale_factor = [self contentsScale];

  if (!renderWidgetHostView_->compositing_iosurface_->DrawIOSurface(
        context_,
        window_rect,
        window_scale_factor,
        false)) {
    renderWidgetHostView_->GotAcceleratedCompositingError();
    return;
  }

  needsDisplay_ = NO;
  renderWidgetHostView_->SendPendingLatencyInfoToHost();
}

@end
