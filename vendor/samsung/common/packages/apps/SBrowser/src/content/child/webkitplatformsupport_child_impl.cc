// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webkitplatformsupport_child_impl.h"

#include "base/memory/discardable_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/child/web_discardable_memory_impl.h"
#include "third_party/WebKit/public/platform/WebWaitableEvent.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "webkit/child/fling_curve_configuration.h"
#include "webkit/child/webthread_impl.h"
#include "webkit/child/worker_task_runner.h"

#if defined(OS_ANDROID)
#include "webkit/child/fling_animator_impl_android.h"
#endif

using blink::WebFallbackThemeEngine;
using blink::WebThemeEngine;

namespace content {

namespace {

class WebWaitableEventImpl : public blink::WebWaitableEvent {
 public:
  WebWaitableEventImpl() : impl_(new base::WaitableEvent(false, false)) {}
  virtual ~WebWaitableEventImpl() {}

  virtual void wait() { impl_->Wait(); }
  virtual void signal() { impl_->Signal(); }

  base::WaitableEvent* impl() {
    return impl_.get();
  }

 private:
  scoped_ptr<base::WaitableEvent> impl_;
  DISALLOW_COPY_AND_ASSIGN(WebWaitableEventImpl);
};

}  // namespace

WebKitPlatformSupportChildImpl::WebKitPlatformSupportChildImpl()
    : current_thread_slot_(&DestroyCurrentThread),
      fling_curve_configuration_(new webkit_glue::FlingCurveConfiguration) {}

WebKitPlatformSupportChildImpl::~WebKitPlatformSupportChildImpl() {}

WebThemeEngine* WebKitPlatformSupportChildImpl::themeEngine() {
  return &native_theme_engine_;
}

WebFallbackThemeEngine* WebKitPlatformSupportChildImpl::fallbackThemeEngine() {
  return &fallback_theme_engine_;
}

void WebKitPlatformSupportChildImpl::SetFlingCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen) {
  fling_curve_configuration_->SetCurveParameters(new_touchpad, new_touchscreen);
}

blink::WebGestureCurve*
WebKitPlatformSupportChildImpl::createFlingAnimationCurve(
    int device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
#if defined(OS_ANDROID)
  return webkit_glue::FlingAnimatorImpl::CreateAndroidGestureCurve(
      velocity, cumulative_scroll);
#endif

  if (device_source == blink::WebGestureEvent::Touchscreen)
    return fling_curve_configuration_->CreateForTouchScreen(velocity,
                                                            cumulative_scroll);

  return fling_curve_configuration_->CreateForTouchPad(velocity,
                                                       cumulative_scroll);
}

blink::WebThread* WebKitPlatformSupportChildImpl::createThread(
    const char* name) {
  return new webkit_glue::WebThreadImpl(name);
}

blink::WebThread* WebKitPlatformSupportChildImpl::currentThread() {
  webkit_glue::WebThreadImplForMessageLoop* thread =
      static_cast<webkit_glue::WebThreadImplForMessageLoop*>(
          current_thread_slot_.Get());
  if (thread)
    return (thread);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      base::MessageLoopProxy::current();
  if (!message_loop.get())
    return NULL;

  thread = new webkit_glue::WebThreadImplForMessageLoop(message_loop.get());
  current_thread_slot_.Set(thread);
  return thread;
}

blink::WebWaitableEvent* WebKitPlatformSupportChildImpl::createWaitableEvent() {
  return new WebWaitableEventImpl();
}

blink::WebWaitableEvent* WebKitPlatformSupportChildImpl::waitMultipleEvents(
    const blink::WebVector<blink::WebWaitableEvent*>& web_events) {
  base::WaitableEvent** events = new base::WaitableEvent*[web_events.size()];
  for (size_t i = 0; i < web_events.size(); ++i)
    events[i] = static_cast<WebWaitableEventImpl*>(web_events[i])->impl();
  size_t idx = base::WaitableEvent::WaitMany(events, web_events.size());
  DCHECK_LT(idx, web_events.size());
  return web_events[idx];
}

void WebKitPlatformSupportChildImpl::didStartWorkerRunLoop(
    const blink::WebWorkerRunLoop& runLoop) {
  webkit_glue::WorkerTaskRunner* worker_task_runner =
      webkit_glue::WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStarted(runLoop);
}

void WebKitPlatformSupportChildImpl::didStopWorkerRunLoop(
    const blink::WebWorkerRunLoop& runLoop) {
  webkit_glue::WorkerTaskRunner* worker_task_runner =
      webkit_glue::WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStopped(runLoop);
}

blink::WebDiscardableMemory*
WebKitPlatformSupportChildImpl::allocateAndLockDiscardableMemory(size_t bytes) {
  base::DiscardableMemoryType type =
      base::DiscardableMemory::GetPreferredType();
  if (type == base::DISCARDABLE_MEMORY_TYPE_EMULATED)
    return NULL;
  return content::WebDiscardableMemoryImpl::CreateLockedMemory(bytes).release();
}

// static
void WebKitPlatformSupportChildImpl::DestroyCurrentThread(void* thread) {
  webkit_glue::WebThreadImplForMessageLoop* impl =
      static_cast<webkit_glue::WebThreadImplForMessageLoop*>(thread);
  delete impl;
}

}  // namespace content
