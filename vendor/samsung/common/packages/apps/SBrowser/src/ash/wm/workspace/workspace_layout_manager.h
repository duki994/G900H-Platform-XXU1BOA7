// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/wm_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ui {
class Layer;
}

namespace ash {
namespace wm {
class WindowState;
}

namespace internal {

class ShelfLayoutManager;

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager
    : public aura::LayoutManager,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver,
      public ShellObserver,
      public wm::WindowStateObserver {
 public:
  explicit WorkspaceLayoutManager(aura::Window* window);
  virtual ~WorkspaceLayoutManager();

  void SetShelf(internal::ShelfLayoutManager* shelf);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // ash::ShellObserver overrides:
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

  // Overriden from WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // WindowStateObserver overrides:
  virtual void OnPostWindowShowTypeChange(wm::WindowState* window_state,
                                          wm::WindowShowType old_type) OVERRIDE;

 private:
  typedef std::set<aura::Window*> WindowSet;

  enum AdjustWindowReason {
    ADJUST_WINDOW_DISPLAY_SIZE_CHANGED,
    ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED,
  };

  // Adjusts the window's bounds when the display area changes for given
  // window. This happens when the display size, work area insets or
  // the display on which the window exists has changed.
  // If this is called for a display size change (i.e. |reason|
  // is ADJUST_WINDOW_DISPLAY_SIZE_CHANGED), the non-maximized/non-fullscreen
  // windows are readjusted to make sure the window is completely within the
  // display region. Otherwise, it makes sure at least some parts of the window
  // is on display.
  void AdjustAllWindowsBoundsForWorkAreaChange(AdjustWindowReason reason);

  // Adjusts the sizes of the specific window in respond to a screen change or
  // display-area size change.
  void AdjustWindowBoundsForWorkAreaChange(wm::WindowState* window_state,
                                           AdjustWindowReason reason);

  void AdjustWindowBoundsWhenAdded(wm::WindowState* window_state);

  // Updates the visibility state of the shelf.
  void UpdateShelfVisibility();

  // Updates the fullscreen state of the workspace and notifies Shell if it
  // has changed.
  void UpdateFullscreenState();

  // Updates the bounds of the window for a show type change from
  // |old_show_type|.
  void UpdateBoundsFromShowType(wm::WindowState* window_state,
                                wm::WindowShowType old_show_type);

  // If |window_state| is maximized or fullscreen the bounds of the
  // window are set and true is returned. Does nothing otherwise.
  bool SetMaximizedOrFullscreenBounds(wm::WindowState* window_state);

  // Animates the window bounds to |bounds|.
  void SetChildBoundsAnimated(aura::Window* child, const gfx::Rect& bounds);

  internal::ShelfLayoutManager* shelf_;
  aura::Window* window_;
  aura::Window* root_window_;

  // Set of windows we're listening to.
  WindowSet windows_;

  // The work area in the coordinates of |window_|.
  gfx::Rect work_area_in_parent_;

  // True if this workspace is currently in fullscreen mode.
  bool is_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
