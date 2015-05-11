// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/default_state.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_types.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/display.h"
#include "ui/views/corewm/window_util.h"

namespace ash {
namespace wm {

namespace {

// A tentative class to set the bounds on the window.
// TODO(oshima): Once all logic is cleaned up, move this to the real layout
// manager with proper friendship.
class BoundsSetter : public aura::LayoutManager {
 public:
  BoundsSetter() {}
  virtual ~BoundsSetter() {}

  // aura::LayoutManager overrides:
  virtual void OnWindowResized() OVERRIDE {}
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnChildWindowVisibilityChanged(
      aura::Window* child, bool visible) OVERRIDE {}
  virtual void SetChildBounds(
      aura::Window* child, const gfx::Rect& requested_bounds) OVERRIDE {}

  void SetBounds(aura::Window* window, const gfx::Rect& bounds) {
    SetChildBoundsDirect(window, bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsSetter);
};

WMEvent WMEventFromShowState(ui::WindowShowState requested_show_state) {
  switch (requested_show_state) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL:
      return NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return MINIMIZE;
    case ui::SHOW_STATE_MAXIMIZED:
      return MAXIMIZE;
    case ui::SHOW_STATE_FULLSCREEN:
      return FULLSCREEN;
    case ui::SHOW_STATE_INACTIVE:
      return SHOW_INACTIVE;
    case ui::SHOW_STATE_DETACHED:
    case ui::SHOW_STATE_END:
      NOTREACHED() << "No WMEvent defined for the show type:"
                   << requested_show_state;
  }
  return NORMAL;
}

}  // namespace

WindowState::WindowState(aura::Window* window)
    : window_(window),
      window_position_managed_(false),
      bounds_changed_by_user_(false),
      panel_attached_(true),
      continue_drag_after_reparent_(false),
      ignored_by_shelf_(false),
      can_consume_system_keys_(false),
      top_row_keys_are_function_keys_(false),
      unminimize_to_restore_bounds_(false),
      hide_shelf_when_fullscreen_(true),
      animate_to_fullscreen_(true),
      minimum_visibility_(false),
      ignore_property_change_(false),
      window_show_type_(ToWindowShowType(GetShowState())),
      current_state_(new DefaultState) {
  window_->AddObserver(this);
#if defined(OS_CHROMEOS)
  // NOTE(pkotwicz): Animating to immersive fullscreen does not look good. When
  // switches::UseImmersiveFullscreenForAllWindows() returns true, most windows
  // can be put into immersive fullscreen. It is not worth the added complexity
  // to only animate to fullscreen if the window is put into immersive
  // fullscreen.
  animate_to_fullscreen_ = !switches::UseImmersiveFullscreenForAllWindows();
#endif
}

WindowState::~WindowState() {
}

bool WindowState::HasDelegate() const {
  return delegate_;
}

void WindowState::SetDelegate(scoped_ptr<WindowStateDelegate> delegate) {
  DCHECK(!delegate_.get());
  delegate_ = delegate.Pass();
}

ui::WindowShowState WindowState::GetShowState() const {
  return window_->GetProperty(aura::client::kShowStateKey);
}

bool WindowState::IsMinimized() const {
  return GetShowState() == ui::SHOW_STATE_MINIMIZED;
}

bool WindowState::IsMaximized() const {
  return GetShowState() == ui::SHOW_STATE_MAXIMIZED;
}

bool WindowState::IsFullscreen() const {
  return GetShowState() == ui::SHOW_STATE_FULLSCREEN;
}

bool WindowState::IsMaximizedOrFullscreen() const {
  ui::WindowShowState show_state(GetShowState());
  return show_state == ui::SHOW_STATE_FULLSCREEN ||
      show_state == ui::SHOW_STATE_MAXIMIZED;
}

bool WindowState::IsNormalShowState() const {
  ui::WindowShowState state = GetShowState();
  return state == ui::SHOW_STATE_NORMAL || state == ui::SHOW_STATE_DEFAULT;
}

bool WindowState::IsNormalShowType() const {
  return window_show_type_ == SHOW_TYPE_NORMAL ||
      window_show_type_ == SHOW_TYPE_DEFAULT;
}

bool WindowState::IsActive() const {
  return IsActiveWindow(window_);
}

bool WindowState::IsDocked() const {
  return window_->parent() &&
      window_->parent()->id() == internal::kShellWindowId_DockedContainer;
}

bool WindowState::IsSnapped() const {
  return window_show_type_ == SHOW_TYPE_LEFT_SNAPPED ||
      window_show_type_ == SHOW_TYPE_RIGHT_SNAPPED;
}

bool WindowState::CanMaximize() const {
  return window_->GetProperty(aura::client::kCanMaximizeKey);
}

bool WindowState::CanMinimize() const {
  internal::RootWindowController* controller =
      internal::RootWindowController::ForWindow(window_);
  if (!controller)
    return false;
  aura::Window* lockscreen = controller->GetContainer(
      internal::kShellWindowId_LockScreenContainersContainer);
  if (lockscreen->Contains(window_))
    return false;

  return true;
}

bool WindowState::CanResize() const {
  return window_->GetProperty(aura::client::kCanResizeKey);
}

bool WindowState::CanActivate() const {
  return views::corewm::CanActivateWindow(window_);
}

bool WindowState::CanSnap() const {
  if (!CanResize() || window_->type() == ui::wm::WINDOW_TYPE_PANEL ||
      views::corewm::GetTransientParent(window_))
    return false;
  // If a window has a maximum size defined, snapping may make it too big.
  return window_->delegate() ? window_->delegate()->GetMaximumSize().IsEmpty() :
                              true;
}

bool WindowState::HasRestoreBounds() const {
  return window_->GetProperty(aura::client::kRestoreBoundsKey) != NULL;
}

void WindowState::Maximize() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void WindowState::SnapLeft(const gfx::Rect& bounds) {
  SnapWindow(SHOW_TYPE_LEFT_SNAPPED, bounds);
}

void WindowState::SnapRight(const gfx::Rect& bounds) {
  SnapWindow(SHOW_TYPE_RIGHT_SNAPPED, bounds);
}

void WindowState::Minimize() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
}

void WindowState::Unminimize() {
  window_->SetProperty(
      aura::client::kShowStateKey,
      window_->GetProperty(aura::client::kRestoreShowStateKey));
  window_->ClearProperty(aura::client::kRestoreShowStateKey);
}

void WindowState::Activate() {
  ActivateWindow(window_);
}

void WindowState::Deactivate() {
  DeactivateWindow(window_);
}

void WindowState::Restore() {
  if (!IsNormalShowType())
    OnWMEvent(NORMAL);
}

void WindowState::ToggleFullscreen() {
  OnWMEvent(TOGGLE_FULLSCREEN);
}

void WindowState::OnWMEvent(WMEvent event) {
  current_state_->OnWMEvent(this, event);
}

void WindowState::SetBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds_in_parent =
      ScreenUtil::ConvertRectFromScreen(window_->parent(),
                                       bounds_in_screen);
  window_->SetBounds(bounds_in_parent);
}

void WindowState::SaveCurrentBoundsForRestore() {
  gfx::Rect bounds_in_screen =
      ScreenUtil::ConvertRectToScreen(window_->parent(),
                                     window_->bounds());
  SetRestoreBoundsInScreen(bounds_in_screen);
}

gfx::Rect WindowState::GetRestoreBoundsInScreen() const {
  return *window_->GetProperty(aura::client::kRestoreBoundsKey);
}

gfx::Rect WindowState::GetRestoreBoundsInParent() const {
  return ScreenUtil::ConvertRectFromScreen(window_->parent(),
                                          GetRestoreBoundsInScreen());
}

void WindowState::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  window_->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

void WindowState::SetRestoreBoundsInParent(const gfx::Rect& bounds) {
  SetRestoreBoundsInScreen(
      ScreenUtil::ConvertRectToScreen(window_->parent(), bounds));
}

void WindowState::ClearRestoreBounds() {
  window_->ClearProperty(aura::client::kRestoreBoundsKey);
}

void WindowState::SetPreAutoManageWindowBounds(
    const gfx::Rect& bounds) {
  pre_auto_manage_window_bounds_.reset(new gfx::Rect(bounds));
}

void WindowState::AddObserver(WindowStateObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WindowState::RemoveObserver(WindowStateObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WindowState::CreateDragDetails(aura::Window* window,
                                    const gfx::Point& point_in_parent,
                                    int window_component,
                                    aura::client::WindowMoveSource source) {
  drag_details_.reset(
      new DragDetails(window, point_in_parent, window_component, source));
}

void WindowState::DeleteDragDetails() {
  drag_details_.reset();
}

void WindowState::SetAndClearRestoreBounds() {
  DCHECK(HasRestoreBounds());
  SetBoundsInScreen(GetRestoreBoundsInScreen());
  ClearRestoreBounds();
}

void WindowState::AdjustSnappedBounds(gfx::Rect* bounds) {
  if (is_dragged() || !IsSnapped())
    return;
  gfx::Rect maximized_bounds = ScreenUtil::GetMaximizedWindowBoundsInParent(
      window_);
  if (window_show_type() == SHOW_TYPE_LEFT_SNAPPED)
    bounds->set_x(maximized_bounds.x());
  else if (window_show_type() == SHOW_TYPE_RIGHT_SNAPPED)
    bounds->set_x(maximized_bounds.right() - bounds->width());
  bounds->set_y(maximized_bounds.y());
  bounds->set_height(maximized_bounds.height());
}

void WindowState::OnWindowPropertyChanged(aura::Window* window,
                                          const void* key,
                                          intptr_t old) {
  DCHECK_EQ(window, window_);
  if (key == aura::client::kShowStateKey && !ignore_property_change_)
    OnWMEvent(WMEventFromShowState(GetShowState()));
}

void WindowState::SnapWindow(WindowShowType left_or_right,
                             const gfx::Rect& bounds) {
  if (window_show_type_ == left_or_right) {
    window_->SetBounds(bounds);
    return;
  }

  // Compute the bounds that the window will restore to. If the window does not
  // already have restore bounds, it will be restored (when un-snapped) to the
  // last bounds that it had before getting snapped.
  gfx::Rect restore_bounds_in_screen(HasRestoreBounds() ?
      GetRestoreBoundsInScreen() : window_->GetBoundsInScreen());
  // Set the window's restore bounds so that WorkspaceLayoutManager knows
  // which width to use when the snapped window is moved to the edge.
  SetRestoreBoundsInParent(bounds);

  DCHECK(left_or_right == SHOW_TYPE_LEFT_SNAPPED ||
         left_or_right == SHOW_TYPE_RIGHT_SNAPPED);
  OnWMEvent(left_or_right == SHOW_TYPE_LEFT_SNAPPED ?
            SNAP_LEFT : SNAP_RIGHT);

  // TODO(varkha): Ideally the bounds should be changed in a LayoutManager upon
  // observing the WindowShowType change.
  // If the window is a child of kShellWindowId_DockedContainer such as during
  // a drag, the window's bounds are not set in
  // WorkspaceLayoutManager::OnWindowShowTypeChanged(). Set them here. Skip
  // setting the bounds otherwise to avoid stopping the slide animation which
  // was started as a result of OnWindowShowTypeChanged().
  if (IsDocked())
    window_->SetBounds(bounds);
  SetRestoreBoundsInScreen(restore_bounds_in_screen);
}

void WindowState::UpdateWindowShowType(WindowShowType new_window_show_type) {
  ui::WindowShowState new_window_state =
      ToWindowShowState(new_window_show_type);
  if (new_window_state != GetShowState()) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(aura::client::kShowStateKey, new_window_state);
  }
  window_show_type_ = new_window_show_type;
}

void WindowState::NotifyPreShowTypeChange(WindowShowType old_window_show_type) {
  FOR_EACH_OBSERVER(WindowStateObserver, observer_list_,
                    OnPreWindowShowTypeChange(this, old_window_show_type));
}

void WindowState::NotifyPostShowTypeChange(
    WindowShowType old_window_show_type) {
  FOR_EACH_OBSERVER(WindowStateObserver, observer_list_,
                    OnPostWindowShowTypeChange(this, old_window_show_type));
}

void WindowState::SetBoundsDirect(const gfx::Rect& bounds) {
  BoundsSetter().SetBounds(window_, bounds);
}

void WindowState::SetBoundsDirectAnimated(const gfx::Rect& bounds) {
  const int kBoundsChangeSlideDurationMs = 120;

  ui::Layer* layer = window_->layer();
  ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kBoundsChangeSlideDurationMs));
  SetBoundsDirect(bounds);
}

WindowState* GetActiveWindowState() {
  aura::Window* active = GetActiveWindow();
  return active ? GetWindowState(active) : NULL;
}

WindowState* GetWindowState(aura::Window* window) {
  if (!window)
    return NULL;
  WindowState* settings = window->GetProperty(internal::kWindowStateKey);
  if(!settings) {
    settings = new WindowState(window);
    window->SetProperty(internal::kWindowStateKey, settings);
  }
  return settings;
}

const WindowState* GetWindowState(const aura::Window* window) {
  return GetWindowState(const_cast<aura::Window*>(window));
}

}  // namespace wm
}  // namespace ash
