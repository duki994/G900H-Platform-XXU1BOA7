// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/screen.h"

using std::vector;

typedef ui::EventDispatchDetails DispatchDetails;

namespace aura {

namespace {

const char kRootWindowForAcceleratedWidget[] =
    "__AURA_ROOT_WINDOW_ACCELERATED_WIDGET__";

// Returns true if |target| has a non-client (frame) component at |location|,
// in window coordinates.
bool IsNonClientLocation(Window* target, const gfx::Point& location) {
  if (!target->delegate())
    return false;
  int hit_test_code = target->delegate()->GetNonClientComponent(location);
  return hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE;
}

Window* ConsumerToWindow(ui::GestureConsumer* consumer) {
  return consumer ? static_cast<Window*>(consumer) : NULL;
}

void SetLastMouseLocation(const Window* root_window,
                          const gfx::Point& location_in_root) {
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(root_window);
  if (client) {
    gfx::Point location_in_screen = location_in_root;
    client->ConvertPointToScreen(root_window, &location_in_screen);
    Env::GetInstance()->set_last_mouse_location(location_in_screen);
  } else {
    Env::GetInstance()->set_last_mouse_location(location_in_root);
  }
}

WindowTreeHost* CreateHost(RootWindow* root_window,
                           const RootWindow::CreateParams& params) {
  WindowTreeHost* host = params.host ?
      params.host : WindowTreeHost::Create(params.initial_bounds);
  host->set_delegate(root_window);
  return host;
}

bool IsEventCandidateForHold(const ui::Event& event) {
  if (event.type() == ui::ET_TOUCH_MOVED)
    return true;
  if (event.type() == ui::ET_MOUSE_DRAGGED)
    return true;
  if (event.IsMouseEvent() && (event.flags() & ui::EF_IS_SYNTHESIZED))
    return true;
  return false;
}

}  // namespace

RootWindow::CreateParams::CreateParams(const gfx::Rect& a_initial_bounds)
    : initial_bounds(a_initial_bounds),
      host(NULL) {
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, public:

RootWindow::RootWindow(const CreateParams& params)
    : window_(new Window(NULL)),
      host_(CreateHost(this, params)),
      touch_ids_down_(0),
      mouse_pressed_handler_(NULL),
      mouse_moved_handler_(NULL),
      event_dispatch_target_(NULL),
      old_dispatch_target_(NULL),
      synthesize_mouse_move_(false),
      move_hold_count_(0),
      dispatching_held_event_(false),
      repost_event_factory_(this),
      held_event_factory_(this) {
  window()->set_dispatcher(this);
  window()->SetName("RootWindow");
  window()->SetEventTargeter(
      scoped_ptr<ui::EventTargeter>(new WindowTargeter()));

  prop_.reset(new ui::ViewProp(host_->GetAcceleratedWidget(),
                               kRootWindowForAcceleratedWidget,
                               this));
  ui::GestureRecognizer::Get()->AddGestureEventHelper(this);
}

RootWindow::~RootWindow() {
  TRACE_EVENT0("shutdown", "RootWindow::Destructor");

  ui::GestureRecognizer::Get()->RemoveGestureEventHelper(this);

  // An observer may have been added by an animation on the RootWindow.
  window()->layer()->GetAnimator()->RemoveObserver(this);

  // Destroy child windows while we're still valid. This is also done by
  // ~Window, but by that time any calls to virtual methods overriden here (such
  // as GetRootWindow()) result in Window's implementation. By destroying here
  // we ensure GetRootWindow() still returns this.
  window()->RemoveOrDestroyChildren();

  // Destroying/removing child windows may try to access |host_| (eg.
  // GetAcceleratedWidget())
  host_.reset(NULL);

  window()->set_dispatcher(NULL);
}

// static
RootWindow* RootWindow::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return reinterpret_cast<RootWindow*>(
      ui::ViewProp::GetValue(widget, kRootWindowForAcceleratedWidget));
}

void RootWindow::PrepareForShutdown() {
  host_->PrepareForShutdown();
  // discard synthesize event request as well.
  synthesize_mouse_move_ = false;
}

void RootWindow::RepostEvent(const ui::LocatedEvent& event) {
  DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_GESTURE_TAP_DOWN);
  // We allow for only one outstanding repostable event. This is used
  // in exiting context menus.  A dropped repost request is allowed.
  if (event.type() == ui::ET_MOUSE_PRESSED) {
    held_repostable_event_.reset(
        new ui::MouseEvent(
            static_cast<const ui::MouseEvent&>(event),
            static_cast<aura::Window*>(event.target()),
            window()));
    base::MessageLoop::current()->PostNonNestableTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&RootWindow::DispatchHeldEvents),
                   repost_event_factory_.GetWeakPtr()));
  } else {
    DCHECK(event.type() == ui::ET_GESTURE_TAP_DOWN);
    held_repostable_event_.reset();
    // TODO(rbyers): Reposing of gestures is tricky to get
    // right, so it's not yet supported.  crbug.com/170987.
  }
}

WindowTreeHostDelegate* RootWindow::AsWindowTreeHostDelegate() {
  return this;
}

void RootWindow::OnMouseEventsEnableStateChanged(bool enabled) {
  // Send entered / exited so that visual state can be updated to match
  // mouse events state.
  PostMouseMoveEventAfterWindowChange();
  // TODO(mazda): Add code to disable mouse events when |enabled| == false.
}

Window* RootWindow::GetGestureTarget(ui::GestureEvent* event) {
  Window* target = NULL;
  if (!event->IsEndingEvent()) {
    // The window that received the start event (e.g. scroll begin) needs to
    // receive the end event (e.g. scroll end).
    target = client::GetCaptureWindow(window());
  }
  if (!target) {
    target = ConsumerToWindow(
        ui::GestureRecognizer::Get()->GetTargetForGestureEvent(*event));
  }

  return target;
}

void RootWindow::DispatchGestureEvent(ui::GestureEvent* event) {
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;

  Window* target = GetGestureTarget(event);
  if (target) {
    event->ConvertLocationToTarget(window(), target);
    DispatchDetails details = DispatchEvent(target, event);
    if (details.dispatcher_destroyed)
      return;
  }
}

void RootWindow::OnWindowDestroying(Window* window) {
  DispatchMouseExitToHidingWindow(window);
  if (window->IsVisible() &&
      window->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }

  // Hiding the window releases capture which can implicitly destroy the window
  // so the window may no longer be valid after this call.
  OnWindowHidden(window, WINDOW_DESTROYED);
}

void RootWindow::OnWindowBoundsChanged(Window* window,
                                       bool contained_mouse_point) {
  if (contained_mouse_point ||
      (window->IsVisible() &&
       window->ContainsPointInRoot(GetLastMouseLocationInRoot()))) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::DispatchMouseExitToHidingWindow(Window* window) {
  // The mouse capture is intentionally ignored. Think that a mouse enters
  // to a window, the window sets the capture, the mouse exits the window,
  // and then it releases the capture. In that case OnMouseExited won't
  // be called. So it is natural not to emit OnMouseExited even though
  // |window| is the capture window.
  gfx::Point last_mouse_location = GetLastMouseLocationInRoot();
  if (window->Contains(mouse_moved_handler_) &&
      window->ContainsPointInRoot(last_mouse_location))
    DispatchMouseExitAtPoint(last_mouse_location);
}

void RootWindow::DispatchMouseExitAtPoint(const gfx::Point& point) {
  ui::MouseEvent event(ui::ET_MOUSE_EXITED, point, point, ui::EF_NONE,
                       ui::EF_NONE);
  DispatchDetails details =
      DispatchMouseEnterOrExit(event, ui::ET_MOUSE_EXITED);
  if (details.dispatcher_destroyed)
    return;
}

void RootWindow::OnWindowVisibilityChanged(Window* window, bool is_visible) {
  if (window->ContainsPointInRoot(GetLastMouseLocationInRoot()))
    PostMouseMoveEventAfterWindowChange();

  // Hiding the window releases capture which can implicitly destroy the window
  // so the window may no longer be valid after this call.
  if (!is_visible)
    OnWindowHidden(window, WINDOW_HIDDEN);
}

void RootWindow::OnWindowTransformed(Window* window, bool contained_mouse) {
  if (contained_mouse ||
      (window->IsVisible() &&
       window->ContainsPointInRoot(GetLastMouseLocationInRoot()))) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnKeyboardMappingChanged() {
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnKeyboardMappingChanged(this));
}

void RootWindow::OnWindowTreeHostCloseRequested() {
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnWindowTreeHostCloseRequested(this));
}

void RootWindow::AddRootWindowObserver(RootWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void RootWindow::RemoveRootWindowObserver(RootWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RootWindow::ProcessedTouchEvent(ui::TouchEvent* event,
                                     Window* window,
                                     ui::EventResult result) {
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(ui::GestureRecognizer::Get()->
      ProcessTouchEventForGesture(*event, result, window));
  DispatchDetails details = ProcessGestures(gestures.get());
  if (details.dispatcher_destroyed)
    return;
}

void RootWindow::HoldPointerMoves() {
  if (!move_hold_count_)
    held_event_factory_.InvalidateWeakPtrs();
  ++move_hold_count_;
  TRACE_EVENT_ASYNC_BEGIN0("ui", "RootWindow::HoldPointerMoves", this);
}

void RootWindow::ReleasePointerMoves() {
  --move_hold_count_;
  DCHECK_GE(move_hold_count_, 0);
  if (!move_hold_count_ && held_move_event_) {
    // We don't want to call DispatchHeldEvents directly, because this might be
    // called from a deep stack while another event, in which case dispatching
    // another one may not be safe/expected.  Instead we post a task, that we
    // may cancel if HoldPointerMoves is called again before it executes.
    base::MessageLoop::current()->PostNonNestableTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&RootWindow::DispatchHeldEvents),
                   held_event_factory_.GetWeakPtr()));
  }
  TRACE_EVENT_ASYNC_END0("ui", "RootWindow::HoldPointerMoves", this);
}

gfx::Point RootWindow::GetLastMouseLocationInRoot() const {
  gfx::Point location = Env::GetInstance()->last_mouse_location();
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(window());
  if (client)
    client->ConvertPointFromScreen(window(), &location);
  return location;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

void RootWindow::TransformEventForDeviceScaleFactor(ui::LocatedEvent* event) {
  event->UpdateForRootTransform(host()->GetInverseRootTransform());
}

ui::EventDispatchDetails RootWindow::DispatchMouseEnterOrExit(
    const ui::MouseEvent& event,
    ui::EventType type) {
  if (event.type() != ui::ET_MOUSE_CAPTURE_CHANGED &&
      !(event.flags() & ui::EF_IS_SYNTHESIZED)) {
    SetLastMouseLocation(window(), event.root_location());
  }

  if (!mouse_moved_handler_ || !mouse_moved_handler_->delegate())
    return DispatchDetails();

  // |event| may be an event in the process of being dispatched to a target (in
  // which case its locations will be in the event's target's coordinate
  // system), or a synthetic event created in root-window (in which case, the
  // event's target will be NULL, and the event will be in the root-window's
  // coordinate system.
  aura::Window* target = static_cast<Window*>(event.target());
  if (!target)
    target = window();
  ui::MouseEvent translated_event(event,
                                  target,
                                  mouse_moved_handler_,
                                  type,
                                  event.flags() | ui::EF_IS_SYNTHESIZED);
  return DispatchEvent(mouse_moved_handler_, &translated_event);
}

ui::EventDispatchDetails RootWindow::ProcessGestures(
    ui::GestureRecognizer::Gestures* gestures) {
  DispatchDetails details;
  if (!gestures || gestures->empty())
    return details;

  Window* target = GetGestureTarget(gestures->get().at(0));
  for (size_t i = 0; i < gestures->size(); ++i) {
    ui::GestureEvent* event = gestures->get().at(i);
    event->ConvertLocationToTarget(window(), target);
    details = DispatchEvent(target, event);
    if (details.dispatcher_destroyed || details.target_destroyed)
      break;
  }
  return details;
}

void RootWindow::OnWindowAddedToRootWindow(Window* attached) {
  if (attached->IsVisible() &&
      attached->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowRemovedFromRootWindow(Window* detached,
                                               Window* new_root) {
  DCHECK(aura::client::GetCaptureWindow(window()) != window());

  DispatchMouseExitToHidingWindow(detached);
  if (detached->IsVisible() &&
      detached->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }

  // Hiding the window releases capture which can implicitly destroy the window
  // so the window may no longer be valid after this call.
  OnWindowHidden(detached, new_root ? WINDOW_MOVING : WINDOW_HIDDEN);
}

void RootWindow::OnWindowHidden(Window* invisible, WindowHiddenReason reason) {
  // If the window the mouse was pressed in becomes invisible, it should no
  // longer receive mouse events.
  if (invisible->Contains(mouse_pressed_handler_))
    mouse_pressed_handler_ = NULL;
  if (invisible->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = NULL;

  CleanupGestureState(invisible);

  // Do not clear the capture, and the |event_dispatch_target_| if the
  // window is moving across root windows, because the target itself
  // is actually still visible and clearing them stops further event
  // processing, which can cause unexpected behaviors. See
  // crbug.com/157583
  if (reason != WINDOW_MOVING) {
    Window* capture_window = aura::client::GetCaptureWindow(window());

    if (invisible->Contains(event_dispatch_target_))
      event_dispatch_target_ = NULL;

    if (invisible->Contains(old_dispatch_target_))
      old_dispatch_target_ = NULL;

    // If the ancestor of the capture window is hidden, release the capture.
    // Note that this may delete the window so do not use capture_window
    // after this.
    if (invisible->Contains(capture_window) && invisible != window())
      capture_window->ReleaseCapture();
  }
}

void RootWindow::CleanupGestureState(Window* window) {
  ui::GestureRecognizer::Get()->CancelActiveTouches(window);
  ui::GestureRecognizer::Get()->CleanupStateForConsumer(window);
  const Window::Windows& windows = window->children();
  for (Window::Windows::const_iterator iter = windows.begin();
      iter != windows.end();
      ++iter) {
    CleanupGestureState(*iter);
  }
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, aura::client::CaptureDelegate implementation:

void RootWindow::UpdateCapture(Window* old_capture,
                               Window* new_capture) {
  // |mouse_moved_handler_| may have been set to a Window in a different root
  // (see below). Clear it here to ensure we don't end up referencing a stale
  // Window.
  if (mouse_moved_handler_ && !window()->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = NULL;

  if (old_capture && old_capture->GetRootWindow() == window() &&
      old_capture->delegate()) {
    // Send a capture changed event with bogus location data.
    ui::MouseEvent event(ui::ET_MOUSE_CAPTURE_CHANGED, gfx::Point(),
                         gfx::Point(), 0, 0);

    DispatchDetails details = DispatchEvent(old_capture, &event);
    if (details.dispatcher_destroyed)
      return;

    old_capture->delegate()->OnCaptureLost();
  }

  if (new_capture) {
    // Make all subsequent mouse events go to the capture window. We shouldn't
    // need to send an event here as OnCaptureLost() should take care of that.
    if (mouse_moved_handler_ || Env::GetInstance()->IsMouseButtonDown())
      mouse_moved_handler_ = new_capture;
  } else {
    // Make sure mouse_moved_handler gets updated.
    DispatchDetails details = SynthesizeMouseMoveEvent();
    if (details.dispatcher_destroyed)
      return;
  }
  mouse_pressed_handler_ = NULL;
}

void RootWindow::OnOtherRootGotCapture() {
  mouse_moved_handler_ = NULL;
  mouse_pressed_handler_ = NULL;
}

void RootWindow::SetNativeCapture() {
  host_->SetCapture();
}

void RootWindow::ReleaseNativeCapture() {
  host_->ReleaseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::EventProcessor implementation:
ui::EventTarget* RootWindow::GetRootTarget() {
  return window();
}

void RootWindow::PrepareEventForDispatch(ui::Event* event) {
  if (dispatching_held_event_) {
    // The held events are already in |window()|'s coordinate system. So it is
    // not necessary to apply the transform to convert from the host's
    // coordinate system to |window()|'s coordinate system.
    return;
  }
  if (event->IsMouseEvent() ||
      event->IsScrollEvent() ||
      event->IsTouchEvent() ||
      event->IsGestureEvent()) {
    TransformEventForDeviceScaleFactor(static_cast<ui::LocatedEvent*>(event));
  }
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::EventDispatcherDelegate implementation:

bool RootWindow::CanDispatchToTarget(ui::EventTarget* target) {
  return event_dispatch_target_ == target;
}

ui::EventDispatchDetails RootWindow::PreDispatchEvent(ui::EventTarget* target,
                                                      ui::Event* event) {
  if (!dispatching_held_event_) {
    bool can_be_held = IsEventCandidateForHold(*event);
    if (!move_hold_count_ || !can_be_held) {
      if (can_be_held)
        held_move_event_.reset();
      DispatchDetails details = DispatchHeldEvents();
      if (details.dispatcher_destroyed || details.target_destroyed)
        return details;
    }
  }

  Window* target_window = static_cast<Window*>(target);
  if (event->IsMouseEvent()) {
    PreDispatchMouseEvent(target_window, static_cast<ui::MouseEvent*>(event));
  } else if (event->IsScrollEvent()) {
    PreDispatchLocatedEvent(target_window,
                            static_cast<ui::ScrollEvent*>(event));
  } else if (event->IsTouchEvent()) {
    PreDispatchTouchEvent(target_window, static_cast<ui::TouchEvent*>(event));
  }
  old_dispatch_target_ = event_dispatch_target_;
  event_dispatch_target_ = static_cast<Window*>(target);
  return DispatchDetails();
}

ui::EventDispatchDetails RootWindow::PostDispatchEvent(ui::EventTarget* target,
                                                       const ui::Event& event) {
  DispatchDetails details;
  if (target != event_dispatch_target_)
    details.target_destroyed = true;
  event_dispatch_target_ = old_dispatch_target_;
  old_dispatch_target_ = NULL;
#ifndef NDEBUG
  DCHECK(!event_dispatch_target_ || window()->Contains(event_dispatch_target_));
#endif

  if (event.IsTouchEvent() && !details.target_destroyed) {
    // Do not let 'held' touch events contribute to any gestures.
    if (!held_move_event_ || !held_move_event_->IsTouchEvent()) {
      ui::TouchEvent orig_event(static_cast<const ui::TouchEvent&>(event),
                                static_cast<Window*>(event.target()), window());
      // Get the list of GestureEvents from GestureRecognizer.
      scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
      gestures.reset(ui::GestureRecognizer::Get()->
          ProcessTouchEventForGesture(orig_event, event.result(),
                                      static_cast<Window*>(target)));
      return ProcessGestures(gestures.get());
    }
  }

  return details;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::GestureEventHelper implementation:

bool RootWindow::CanDispatchToConsumer(ui::GestureConsumer* consumer) {
  Window* consumer_window = ConsumerToWindow(consumer);;
  return (consumer_window && consumer_window->GetRootWindow() == window());
}

void RootWindow::DispatchPostponedGestureEvent(ui::GestureEvent* event) {
  DispatchGestureEvent(event);
}

void RootWindow::DispatchCancelTouchEvent(ui::TouchEvent* event) {
  DispatchDetails details = OnEventFromSource(event);
  if (details.dispatcher_destroyed)
    return;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::LayerAnimationObserver implementation:

void RootWindow::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* animation) {
  host()->UpdateRootWindowSize(host_->GetBounds().size());
}

void RootWindow::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* animation) {
}

void RootWindow::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* animation) {
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, WindowTreeHostDelegate implementation:

void RootWindow::OnHostCancelMode() {
  ui::CancelModeEvent event;
  Window* focused_window = client::GetFocusClient(window())->GetFocusedWindow();
  DispatchDetails details =
      DispatchEvent(focused_window ? focused_window : window(), &event);
  if (details.dispatcher_destroyed)
    return;
}

void RootWindow::OnHostActivated() {
  Env::GetInstance()->RootWindowActivated(this);
}

void RootWindow::OnHostLostWindowCapture() {
  Window* capture_window = client::GetCaptureWindow(window());
  if (capture_window && capture_window->GetRootWindow() == window())
    capture_window->ReleaseCapture();
}

void RootWindow::OnHostLostMouseGrab() {
  mouse_pressed_handler_ = NULL;
  mouse_moved_handler_ = NULL;
}

void RootWindow::OnHostMoved(const gfx::Point& origin) {
  TRACE_EVENT1("ui", "RootWindow::OnHostMoved",
               "origin", origin.ToString());

  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnWindowTreeHostMoved(this, origin));
}

void RootWindow::OnHostResized(const gfx::Size& size) {
  TRACE_EVENT1("ui", "RootWindow::OnHostResized",
               "size", size.ToString());

  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnWindowTreeHostResized(this));

  // Constrain the mouse position within the new root Window size.
  gfx::Point point;
  if (host_->QueryMouseLocation(&point)) {
    SetLastMouseLocation(window(),
                         ui::ConvertPointToDIP(window()->layer(), point));
  }
  synthesize_mouse_move_ = false;
}

void RootWindow::OnCursorMovedToRootLocation(const gfx::Point& root_location) {
  SetLastMouseLocation(window(), root_location);
  synthesize_mouse_move_ = false;
}

RootWindow* RootWindow::AsRootWindow() {
  return this;
}

const RootWindow* RootWindow::AsRootWindow() const {
  return this;
}

ui::EventProcessor* RootWindow::GetEventProcessor() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

ui::EventDispatchDetails RootWindow::DispatchHeldEvents() {
  if (!held_repostable_event_ && !held_move_event_)
    return DispatchDetails();

  CHECK(!dispatching_held_event_);
  dispatching_held_event_ = true;

  DispatchDetails dispatch_details;
  if (held_repostable_event_) {
    if (held_repostable_event_->type() == ui::ET_MOUSE_PRESSED) {
      scoped_ptr<ui::MouseEvent> mouse_event(
          static_cast<ui::MouseEvent*>(held_repostable_event_.release()));
      dispatch_details = OnEventFromSource(mouse_event.get());
    } else {
      // TODO(rbyers): GESTURE_TAP_DOWN not yet supported: crbug.com/170987.
      NOTREACHED();
    }
    if (dispatch_details.dispatcher_destroyed)
      return dispatch_details;
  }

  if (held_move_event_) {
    // If a mouse move has been synthesized, the target location is suspect,
    // so drop the held mouse event.
    if (held_move_event_->IsTouchEvent() ||
        (held_move_event_->IsMouseEvent() && !synthesize_mouse_move_)) {
      dispatch_details = OnEventFromSource(held_move_event_.get());
    }
    if (!dispatch_details.dispatcher_destroyed)
      held_move_event_.reset();
  }

  if (!dispatch_details.dispatcher_destroyed)
    dispatching_held_event_ = false;
  return dispatch_details;
}

void RootWindow::PostMouseMoveEventAfterWindowChange() {
  if (synthesize_mouse_move_)
    return;
  synthesize_mouse_move_ = true;
  base::MessageLoop::current()->PostNonNestableTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&RootWindow::SynthesizeMouseMoveEvent),
                 held_event_factory_.GetWeakPtr()));
}

ui::EventDispatchDetails RootWindow::SynthesizeMouseMoveEvent() {
  DispatchDetails details;
  if (!synthesize_mouse_move_)
    return details;
  synthesize_mouse_move_ = false;
  gfx::Point root_mouse_location = GetLastMouseLocationInRoot();
  if (!window()->bounds().Contains(root_mouse_location))
    return details;
  gfx::Point host_mouse_location = root_mouse_location;
  host()->ConvertPointToHost(&host_mouse_location);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED,
                       host_mouse_location,
                       host_mouse_location,
                       ui::EF_IS_SYNTHESIZED,
                       0);
  return OnEventFromSource(&event);
}

void RootWindow::PreDispatchLocatedEvent(Window* target,
                                         ui::LocatedEvent* event) {
  int flags = event->flags();
  if (IsNonClientLocation(target, event->location()))
    flags |= ui::EF_IS_NON_CLIENT;
  event->set_flags(flags);

  if (!dispatching_held_event_ &&
      (event->IsMouseEvent() || event->IsScrollEvent()) &&
      !(event->flags() & ui::EF_IS_SYNTHESIZED)) {
    if (event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)
      SetLastMouseLocation(window(), event->root_location());
    synthesize_mouse_move_ = false;
  }
}

void RootWindow::PreDispatchMouseEvent(Window* target,
                                       ui::MouseEvent* event) {
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client &&
      !cursor_client->IsMouseEventsEnabled() &&
      (event->flags() & ui::EF_IS_SYNTHESIZED)) {
    event->SetHandled();
    return;
  }

  if (IsEventCandidateForHold(*event) && !dispatching_held_event_) {
    if (move_hold_count_) {
      if (!(event->flags() & ui::EF_IS_SYNTHESIZED) &&
          event->type() != ui::ET_MOUSE_CAPTURE_CHANGED) {
        SetLastMouseLocation(window(), event->root_location());
      }
      held_move_event_.reset(new ui::MouseEvent(*event, target, window()));
      event->SetHandled();
      return;
    } else {
      // We may have a held event for a period between the time move_hold_count_
      // fell to 0 and the DispatchHeldEvents executes. Since we're going to
      // dispatch the new event directly below, we can reset the old one.
      held_move_event_.reset();
    }
  }

  const int kMouseButtonFlagMask = ui::EF_LEFT_MOUSE_BUTTON |
                                   ui::EF_MIDDLE_MOUSE_BUTTON |
                                   ui::EF_RIGHT_MOUSE_BUTTON;
  switch (event->type()) {
    case ui::ET_MOUSE_EXITED:
      if (!target || target == window()) {
        DispatchDetails details =
            DispatchMouseEnterOrExit(*event, ui::ET_MOUSE_EXITED);
        if (details.dispatcher_destroyed) {
          event->SetHandled();
          return;
        }
        mouse_moved_handler_ = NULL;
      }
      break;
    case ui::ET_MOUSE_MOVED:
      // Send an exit to the current |mouse_moved_handler_| and an enter to
      // |target|. Take care that both us and |target| aren't destroyed during
      // dispatch.
      if (target != mouse_moved_handler_) {
        aura::Window* old_mouse_moved_handler = mouse_moved_handler_;
        WindowTracker live_window;
        live_window.Add(target);
        DispatchDetails details =
            DispatchMouseEnterOrExit(*event, ui::ET_MOUSE_EXITED);
        if (details.dispatcher_destroyed) {
          event->SetHandled();
          return;
        }
        // If the |mouse_moved_handler_| changes out from under us, assume a
        // nested message loop ran and we don't need to do anything.
        if (mouse_moved_handler_ != old_mouse_moved_handler) {
          event->SetHandled();
          return;
        }
        if (!live_window.Contains(target) || details.target_destroyed) {
          mouse_moved_handler_ = NULL;
          event->SetHandled();
          return;
        }
        live_window.Remove(target);

        mouse_moved_handler_ = target;
        details = DispatchMouseEnterOrExit(*event, ui::ET_MOUSE_ENTERED);
        if (details.dispatcher_destroyed || details.target_destroyed) {
          event->SetHandled();
          return;
        }
      }
      break;
    case ui::ET_MOUSE_PRESSED:
      // Don't set the mouse pressed handler for non client mouse down events.
      // These are only sent by Windows and are not always followed with non
      // client mouse up events which causes subsequent mouse events to be
      // sent to the wrong target.
      if (!(event->flags() & ui::EF_IS_NON_CLIENT) && !mouse_pressed_handler_)
        mouse_pressed_handler_ = target;
      Env::GetInstance()->set_mouse_button_flags(
          event->flags() & kMouseButtonFlagMask);
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_handler_ = NULL;
      Env::GetInstance()->set_mouse_button_flags(event->flags() &
          kMouseButtonFlagMask & ~event->changed_button_flags());
      break;
    default:
      break;
  }

  PreDispatchLocatedEvent(target, event);
}

void RootWindow::PreDispatchTouchEvent(Window* target,
                                       ui::TouchEvent* event) {
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      touch_ids_down_ |= (1 << event->touch_id());
      Env::GetInstance()->set_touch_down(touch_ids_down_ != 0);
      break;

      // Handle ET_TOUCH_CANCELLED only if it has a native event.
    case ui::ET_TOUCH_CANCELLED:
      if (!event->HasNativeEvent())
        break;
      // fallthrough
    case ui::ET_TOUCH_RELEASED:
      touch_ids_down_ = (touch_ids_down_ | (1 << event->touch_id())) ^
            (1 << event->touch_id());
      Env::GetInstance()->set_touch_down(touch_ids_down_ != 0);
      break;

    case ui::ET_TOUCH_MOVED:
      if (move_hold_count_ && !dispatching_held_event_) {
        held_move_event_.reset(new ui::TouchEvent(*event, target, window()));
        event->SetHandled();
        return;
      }
      break;

    default:
      NOTREACHED();
      break;
  }
  PreDispatchLocatedEvent(target, event);
}

}  // namespace aura
