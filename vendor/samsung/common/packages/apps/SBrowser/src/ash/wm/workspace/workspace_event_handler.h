// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_

#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "ui/events/event_handler.h"

namespace ash {
namespace wm {
class WindowState;
}

namespace internal {

class WorkspaceEventHandlerTestHelper;

class WorkspaceEventHandler : public ui::EventHandler {
 public:
  WorkspaceEventHandler();
  virtual ~WorkspaceEventHandler();

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  friend class WorkspaceEventHandlerTestHelper;

  // Determines if |event| corresponds to a double click on either the top or
  // bottom vertical resize edge, and if so toggles the vertical height of the
  // window between its restored state and the full available height of the
  // workspace.
  void HandleVerticalResizeDoubleClick(wm::WindowState* window_state,
                                       ui::MouseEvent* event);

  MultiWindowResizeController multi_window_resize_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
