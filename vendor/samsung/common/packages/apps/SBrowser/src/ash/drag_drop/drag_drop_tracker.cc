// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_tracker.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// An activation delegate which disables activating the drag and drop window.
class CaptureWindowActivationDelegate
    : public aura::client::ActivationDelegate {
 public:
  CaptureWindowActivationDelegate() {}
  virtual ~CaptureWindowActivationDelegate() {}

  // aura::client::ActivationDelegate overrides:
  virtual bool ShouldActivate() const OVERRIDE {
    return false;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(CaptureWindowActivationDelegate);
};

// Creates a window for capturing drag events.
aura::Window* CreateCaptureWindow(aura::Window* context_root,
                                  aura::WindowDelegate* delegate) {
  static CaptureWindowActivationDelegate* activation_delegate_instance = NULL;
  if (!activation_delegate_instance)
    activation_delegate_instance = new CaptureWindowActivationDelegate;
  aura::Window* window = new aura::Window(delegate);
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  aura::client::ParentWindowWithContext(window, context_root, gfx::Rect());
  aura::client::SetActivationDelegate(window, activation_delegate_instance);
  window->Show();
  DCHECK(window->bounds().size().IsEmpty());
  return window;
}

}  // namespace

DragDropTracker::DragDropTracker(aura::Window* context_root,
                                 aura::WindowDelegate* delegate)
    : capture_window_(CreateCaptureWindow(context_root, delegate)) {
}

DragDropTracker::~DragDropTracker()  {
  capture_window_->ReleaseCapture();
}

void DragDropTracker::TakeCapture() {
  capture_window_->SetCapture();
}

aura::Window* DragDropTracker::GetTarget(const ui::LocatedEvent& event) {
  DCHECK(capture_window_.get());
  gfx::Point location_in_screen = event.location();
  wm::ConvertPointToScreen(capture_window_.get(),
                           &location_in_screen);
  aura::Window* root_window_at_point =
      wm::GetRootWindowAt(location_in_screen);
  gfx::Point location_in_root = location_in_screen;
  wm::ConvertPointFromScreen(root_window_at_point, &location_in_root);
  return root_window_at_point->GetEventHandlerForPoint(location_in_root);
}

ui::LocatedEvent* DragDropTracker::ConvertEvent(
    aura::Window* target,
    const ui::LocatedEvent& event) {
  DCHECK(capture_window_.get());
  gfx::Point target_location = event.location();
  aura::Window::ConvertPointToTarget(capture_window_.get(), target,
                                     &target_location);
  gfx::Point location_in_screen = event.location();
  ash::wm::ConvertPointToScreen(capture_window_.get(), &location_in_screen);
  gfx::Point target_root_location = event.root_location();
  aura::Window::ConvertPointToTarget(
      capture_window_->GetRootWindow(),
      ash::wm::GetRootWindowAt(location_in_screen),
      &target_root_location);
  return new ui::MouseEvent(event.type(),
                            target_location,
                            target_root_location,
                            event.flags(),
                            static_cast<const ui::MouseEvent&>(event).
                                changed_button_flags());
}

}  // namespace internal
}  // namespace ash
