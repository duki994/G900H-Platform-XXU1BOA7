// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "ash/ash_switches.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/virtual_keyboard_window_controller.h"
#include "ash/host/window_tree_host_factory.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_transformer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "base/time/time.h"
#if defined(USE_X11)
#include "ash/display/output_configurator_animation.h"
#include "chromeos/display/output_configurator.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_types.h"

// Including this at the bottom to avoid other
// potential conflict with chrome headers.
#include <X11/extensions/Xrandr.h>
#undef RootWindow
#endif  // defined(USE_X11)
#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace {

// Primary display stored in global object as it can be
// accessed after Shell is deleted. A separate display instance is created
// during the shutdown instead of always keeping two display instances
// (one here and another one in display_manager) in sync, which is error prone.
int64 primary_display_id = gfx::Display::kInvalidDisplayID;

// Specifies how long the display change should have been disabled
// after each display change operations.
// |kCycleDisplayThrottleTimeoutMs| is set to be longer to avoid
// changing the settings while the system is still configurating
// displays. It will be overriden by |kAfterDisplayChangeThrottleTimeoutMs|
// when the display change happens, so the actual timeout is much shorter.
const int64 kAfterDisplayChangeThrottleTimeoutMs = 500;
const int64 kCycleDisplayThrottleTimeoutMs = 4000;
const int64 kSwapDisplayThrottleTimeoutMs = 500;

internal::DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

void SetDisplayPropertiesOnHostWindow(aura::RootWindow* root,
                                      const gfx::Display& display) {
  internal::DisplayInfo info =
      GetDisplayManager()->GetDisplayInfo(display.id());
#if defined(OS_CHROMEOS) && defined(USE_X11)
  // Native window property (Atom in X11) that specifies the display's
  // rotation, scale factor and if it's internal display.  They are
  // read and used by touchpad/mouse driver directly on X (contact
  // adlr@ for more details on touchpad/mouse driver side). The value
  // of the rotation is one of 0 (normal), 1 (90 degrees clockwise), 2
  // (180 degree) or 3 (270 degrees clockwise).  The value of the
  // scale factor is in percent (100, 140, 200 etc).
  const char kRotationProp[] = "_CHROME_DISPLAY_ROTATION";
  const char kScaleFactorProp[] = "_CHROME_DISPLAY_SCALE_FACTOR";
  const char kInternalProp[] = "_CHROME_DISPLAY_INTERNAL";
  const char kCARDINAL[] = "CARDINAL";
  int xrandr_rotation = RR_Rotate_0;
  switch (info.rotation()) {
    case gfx::Display::ROTATE_0:
      xrandr_rotation = RR_Rotate_0;
      break;
    case gfx::Display::ROTATE_90:
      xrandr_rotation = RR_Rotate_90;
      break;
    case gfx::Display::ROTATE_180:
      xrandr_rotation = RR_Rotate_180;
      break;
    case gfx::Display::ROTATE_270:
      xrandr_rotation = RR_Rotate_270;
      break;
  }

  int internal = display.IsInternal() ? 1 : 0;
  gfx::AcceleratedWidget xwindow = root->host()->GetAcceleratedWidget();
  ui::SetIntProperty(xwindow, kInternalProp, kCARDINAL, internal);
  ui::SetIntProperty(xwindow, kRotationProp, kCARDINAL, xrandr_rotation);
  ui::SetIntProperty(xwindow,
                     kScaleFactorProp,
                     kCARDINAL,
                     100 * display.device_scale_factor());
#endif
  scoped_ptr<aura::RootWindowTransformer> transformer(
      internal::CreateRootWindowTransformerForDisplay(root->window(), display));
  root->host()->SetRootWindowTransformer(transformer.Pass());

  internal::DisplayMode mode;
  if (GetDisplayManager()->GetSelectedModeForDisplayId(display.id(), &mode) &&
      mode.refresh_rate > 0.0f) {
    root->host()->compositor()->vsync_manager()->SetAuthoritativeVSyncInterval(
        base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                          mode.refresh_rate));
  }
}

}  // namespace

namespace internal {

// A utility class to store/restore focused/active window
// when the display configuration has changed.
class FocusActivationStore {
 public:
  FocusActivationStore()
      : activation_client_(NULL),
        capture_client_(NULL),
        focus_client_(NULL),
        focused_(NULL),
        active_(NULL) {
  }

  void Store(bool clear_focus) {
    if (!activation_client_) {
      aura::Window* root = Shell::GetPrimaryRootWindow();
      activation_client_ = aura::client::GetActivationClient(root);
      capture_client_ = aura::client::GetCaptureClient(root);
      focus_client_ = aura::client::GetFocusClient(root);
    }
    focused_ = focus_client_->GetFocusedWindow();
    if (focused_)
      tracker_.Add(focused_);
    active_ = activation_client_->GetActiveWindow();
    if (active_ && focused_ != active_)
      tracker_.Add(active_);

    // Deactivate the window to close menu / bubble windows.
    if (clear_focus)
      activation_client_->DeactivateWindow(active_);

    // Release capture if any.
    capture_client_->SetCapture(NULL);
    // Clear the focused window if any. This is necessary because a
    // window may be deleted when losing focus (fullscreen flash for
    // example).  If the focused window is still alive after move, it'll
    // be re-focused below.
    if (clear_focus)
      focus_client_->FocusWindow(NULL);
  }

  void Restore() {
    // Restore focused or active window if it's still alive.
    if (focused_ && tracker_.Contains(focused_)) {
      focus_client_->FocusWindow(focused_);
    } else if (active_ && tracker_.Contains(active_)) {
      activation_client_->ActivateWindow(active_);
    }
    if (focused_)
      tracker_.Remove(focused_);
    if (active_)
      tracker_.Remove(active_);
    focused_ = NULL;
    active_ = NULL;
  }

 private:
  aura::client::ActivationClient* activation_client_;
  aura::client::CaptureClient* capture_client_;
  aura::client::FocusClient* focus_client_;
  aura::WindowTracker tracker_;
  aura::Window* focused_;
  aura::Window* active_;

  DISALLOW_COPY_AND_ASSIGN(FocusActivationStore);
};

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// DisplayChangeLimiter

DisplayController::DisplayChangeLimiter::DisplayChangeLimiter()
    : throttle_timeout_(base::Time::Now()) {
}

void DisplayController::DisplayChangeLimiter::SetThrottleTimeout(
    int64 throttle_ms) {
  throttle_timeout_ =
      base::Time::Now() + base::TimeDelta::FromMilliseconds(throttle_ms);
}

bool DisplayController::DisplayChangeLimiter::IsThrottled() const {
  return base::Time::Now() < throttle_timeout_;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayController

DisplayController::DisplayController()
    : primary_root_window_for_replace_(NULL),
      focus_activation_store_(new internal::FocusActivationStore()),
      cursor_window_controller_(new internal::CursorWindowController()),
      mirror_window_controller_(new internal::MirrorWindowController()),
      virtual_keyboard_window_controller_(
          new internal::VirtualKeyboardWindowController) {
#if defined(OS_CHROMEOS)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshDisableDisplayChangeLimiter) &&
      base::SysInfo::IsRunningOnChromeOS())
    limiter_.reset(new DisplayChangeLimiter);
#endif
  // Reset primary display to make sure that tests don't use
  // stale display info from previous tests.
  primary_display_id = gfx::Display::kInvalidDisplayID;
}

DisplayController::~DisplayController() {
}

void DisplayController::Start() {
  Shell::GetScreen()->AddObserver(this);
  Shell::GetInstance()->display_manager()->set_delegate(this);

  if (Shell::GetInstance()->delegate()->IsFirstRunAfterBoot()) {
    // Update the display pref with the initial power state.
    FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanging());
    FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanged());
  }
}

void DisplayController::Shutdown() {
  // Unset the display manager's delegate here because
  // DisplayManager outlives DisplayController.
  Shell::GetInstance()->display_manager()->set_delegate(NULL);

  cursor_window_controller_.reset();
  mirror_window_controller_.reset();
  virtual_keyboard_window_controller_.reset();

  Shell::GetScreen()->RemoveObserver(this);
  // Delete all root window controllers, which deletes root window
  // from the last so that the primary root window gets deleted last.
  for (std::map<int64, aura::Window*>::const_reverse_iterator it =
           root_windows_.rbegin(); it != root_windows_.rend(); ++it) {
    internal::RootWindowController* controller =
        internal::GetRootWindowController(it->second);
    DCHECK(controller);
    delete controller;
  }
}

void DisplayController::InitPrimaryDisplay() {
  const gfx::Display& primary_candidate =
      GetDisplayManager()->GetPrimaryDisplayCandidate();
  primary_display_id = primary_candidate.id();
  AddRootWindowForDisplay(primary_candidate);
}

void DisplayController::InitSecondaryDisplays() {
  internal::DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display& display = display_manager->GetDisplayAt(i);
    if (primary_display_id != display.id()) {
      aura::RootWindow* root = AddRootWindowForDisplay(display);
      internal::RootWindowController::CreateForSecondaryDisplay(root);
    }
  }
  UpdateHostWindowNames();
}

void DisplayController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DisplayController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
int64 DisplayController::GetPrimaryDisplayId() {
  return primary_display_id;
}

aura::Window* DisplayController::GetPrimaryRootWindow() {
  DCHECK(!root_windows_.empty());
  return root_windows_[primary_display_id];
}

aura::Window* DisplayController::GetRootWindowForDisplayId(int64 id) {
  return root_windows_[id];
}

void DisplayController::CloseChildWindows() {
  for (std::map<int64, aura::Window*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    aura::Window* root_window = it->second;
    internal::RootWindowController* controller =
        internal::GetRootWindowController(root_window);
    if (controller) {
      controller->CloseChildWindows();
    } else {
      while (!root_window->children().empty()) {
        aura::Window* child = root_window->children()[0];
        delete child;
      }
    }
  }
}

aura::Window::Windows DisplayController::GetAllRootWindows() {
  aura::Window::Windows windows;
  for (std::map<int64, aura::Window*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    DCHECK(it->second);
    if (internal::GetRootWindowController(it->second))
      windows.push_back(it->second);
  }
  return windows;
}

gfx::Insets DisplayController::GetOverscanInsets(int64 display_id) const {
  return GetDisplayManager()->GetOverscanInsets(display_id);
}

void DisplayController::SetOverscanInsets(int64 display_id,
                                          const gfx::Insets& insets_in_dip) {
  GetDisplayManager()->SetOverscanInsets(display_id, insets_in_dip);
}

std::vector<internal::RootWindowController*>
DisplayController::GetAllRootWindowControllers() {
  std::vector<internal::RootWindowController*> controllers;
  for (std::map<int64, aura::Window*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    internal::RootWindowController* controller =
        internal::GetRootWindowController(it->second);
    if (controller)
      controllers.push_back(controller);
  }
  return controllers;
}

void DisplayController::ToggleMirrorMode() {
  internal::DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->num_connected_displays() <= 1)
    return;

  if (limiter_) {
    if  (limiter_->IsThrottled())
      return;
    limiter_->SetThrottleTimeout(kCycleDisplayThrottleTimeoutMs);
  }
#if defined(OS_CHROMEOS) && defined(USE_X11)
  Shell* shell = Shell::GetInstance();
  internal::OutputConfiguratorAnimation* animation =
      shell->output_configurator_animation();
  animation->StartFadeOutAnimation(base::Bind(
      base::IgnoreResult(&internal::DisplayManager::SetMirrorMode),
      base::Unretained(display_manager),
      !display_manager->IsMirrored()));
#endif
}

void DisplayController::SwapPrimaryDisplay() {
  if (limiter_) {
    if  (limiter_->IsThrottled())
      return;
    limiter_->SetThrottleTimeout(kSwapDisplayThrottleTimeoutMs);
  }

  if (Shell::GetScreen()->GetNumDisplays() > 1) {
#if defined(OS_CHROMEOS) && defined(USE_X11)
    internal::OutputConfiguratorAnimation* animation =
        Shell::GetInstance()->output_configurator_animation();
    if (animation) {
      animation->StartFadeOutAnimation(base::Bind(
          &DisplayController::OnFadeOutForSwapDisplayFinished,
          base::Unretained(this)));
    } else {
      SetPrimaryDisplay(ScreenUtil::GetSecondaryDisplay());
    }
#else
    SetPrimaryDisplay(ScreenUtil::GetSecondaryDisplay());
#endif
  }
}

void DisplayController::SetPrimaryDisplayId(int64 id) {
  DCHECK_NE(gfx::Display::kInvalidDisplayID, id);
  if (id == gfx::Display::kInvalidDisplayID || primary_display_id == id)
    return;

  const gfx::Display& display = GetDisplayManager()->GetDisplayForId(id);
  if (display.is_valid())
    SetPrimaryDisplay(display);
}

void DisplayController::SetPrimaryDisplay(
    const gfx::Display& new_primary_display) {
  internal::DisplayManager* display_manager = GetDisplayManager();
  DCHECK(new_primary_display.is_valid());
  DCHECK(display_manager->IsActiveDisplay(new_primary_display));

  if (!new_primary_display.is_valid() ||
      !display_manager->IsActiveDisplay(new_primary_display)) {
    LOG(ERROR) << "Invalid or non-existent display is requested:"
               << new_primary_display.ToString();
    return;
  }

  if (primary_display_id == new_primary_display.id() ||
      root_windows_.size() < 2) {
    return;
  }

  aura::Window* non_primary_root = root_windows_[new_primary_display.id()];
  LOG_IF(ERROR, !non_primary_root)
      << "Unknown display is requested in SetPrimaryDisplay: id="
      << new_primary_display.id();
  if (!non_primary_root)
    return;

  gfx::Display old_primary_display = Shell::GetScreen()->GetPrimaryDisplay();

  // Swap root windows between current and new primary display.
  aura::Window* primary_root = root_windows_[primary_display_id];
  DCHECK(primary_root);
  DCHECK_NE(primary_root, non_primary_root);

  root_windows_[new_primary_display.id()] = primary_root;
  internal::GetRootWindowSettings(primary_root)->display_id =
      new_primary_display.id();

  root_windows_[old_primary_display.id()] = non_primary_root;
  internal::GetRootWindowSettings(non_primary_root)->display_id =
      old_primary_display.id();

  primary_display_id = new_primary_display.id();
  GetDisplayManager()->layout_store()->UpdatePrimaryDisplayId(
      display_manager->GetCurrentDisplayIdPair(), primary_display_id);

  UpdateWorkAreaOfDisplayNearestWindow(
      primary_root, old_primary_display.GetWorkAreaInsets());
  UpdateWorkAreaOfDisplayNearestWindow(
      non_primary_root, new_primary_display.GetWorkAreaInsets());

  // Update the dispay manager with new display info.
  std::vector<internal::DisplayInfo> display_info_list;
  display_info_list.push_back(display_manager->GetDisplayInfo(
      primary_display_id));
  display_info_list.push_back(display_manager->GetDisplayInfo(
      ScreenUtil::GetSecondaryDisplay().id()));
  GetDisplayManager()->set_force_bounds_changed(true);
  GetDisplayManager()->UpdateDisplays(display_info_list);
  GetDisplayManager()->set_force_bounds_changed(false);
}

void DisplayController::EnsurePointerInDisplays() {
  // If the mouse is currently on a display in native location,
  // use the same native location. Otherwise find the display closest
  // to the current cursor location in screen coordinates.

  gfx::Point point_in_screen = Shell::GetScreen()->GetCursorScreenPoint();
  gfx::Point target_location_in_native;
  int64 closest_distance_squared = -1;
  internal::DisplayManager* display_manager = GetDisplayManager();

  aura::Window* dst_root_window = NULL;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display& display = display_manager->GetDisplayAt(i);
    const internal::DisplayInfo display_info =
        display_manager->GetDisplayInfo(display.id());
    aura::Window* root_window = GetRootWindowForDisplayId(display.id());
    if (display_info.bounds_in_native().Contains(
            cursor_location_in_native_coords_for_restore_)) {
      dst_root_window = root_window;
      target_location_in_native = cursor_location_in_native_coords_for_restore_;
      break;
    }
    gfx::Point center = display.bounds().CenterPoint();
    // Use the distance squared from the center of the dislay. This is not
    // exactly "closest" display, but good enough to pick one
    // appropriate (and there are at most two displays).
    // We don't care about actual distance, only relative to other displays, so
    // using the LengthSquared() is cheaper than Length().

    int64 distance_squared = (center - point_in_screen).LengthSquared();
    if (closest_distance_squared < 0 ||
        closest_distance_squared > distance_squared) {
      aura::Window* root_window = GetRootWindowForDisplayId(display.id());
      aura::client::ScreenPositionClient* client =
          aura::client::GetScreenPositionClient(root_window);
      client->ConvertPointFromScreen(root_window, &center);
      root_window->GetDispatcher()->host()->ConvertPointToNativeScreen(&center);
      dst_root_window = root_window;
      target_location_in_native = center;
      closest_distance_squared = distance_squared;
    }
  }
  dst_root_window->GetDispatcher()->host()->ConvertPointFromNativeScreen(
      &target_location_in_native);
  dst_root_window->MoveCursorTo(target_location_in_native);
}

bool DisplayController::UpdateWorkAreaOfDisplayNearestWindow(
    const aura::Window* window,
    const gfx::Insets& insets) {
  const aura::Window* root_window = window->GetRootWindow();
  int64 id = internal::GetRootWindowSettings(root_window)->display_id;
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != gfx::Display::kInvalidDisplayID);
  return GetDisplayManager()->UpdateWorkAreaOfDisplay(id, insets);
}

void DisplayController::OnDisplayBoundsChanged(const gfx::Display& display) {
  const internal::DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display.id());
  DCHECK(!display_info.bounds_in_native().IsEmpty());
  aura::WindowEventDispatcher* dispatcher =
      root_windows_[display.id()]->GetDispatcher();
  dispatcher->host()->SetBounds(display_info.bounds_in_native());
  SetDisplayPropertiesOnHostWindow(dispatcher, display);
}

void DisplayController::OnDisplayAdded(const gfx::Display& display) {
  if (primary_root_window_for_replace_) {
    DCHECK(root_windows_.empty());
    primary_display_id = display.id();
    root_windows_[display.id()] = primary_root_window_for_replace_;
    internal::GetRootWindowSettings(primary_root_window_for_replace_)->
        display_id = display.id();
    primary_root_window_for_replace_ = NULL;
    const internal::DisplayInfo& display_info =
        GetDisplayManager()->GetDisplayInfo(display.id());
    aura::WindowEventDispatcher* dispatcher =
        root_windows_[display.id()]->GetDispatcher();
    dispatcher->host()->SetBounds(display_info.bounds_in_native());
    SetDisplayPropertiesOnHostWindow(dispatcher, display);
  } else {
    if (primary_display_id == gfx::Display::kInvalidDisplayID)
      primary_display_id = display.id();
    DCHECK(!root_windows_.empty());
    aura::RootWindow* root = AddRootWindowForDisplay(display);
    internal::RootWindowController::CreateForSecondaryDisplay(root);
  }
}

void DisplayController::OnDisplayRemoved(const gfx::Display& display) {
  aura::Window* root_to_delete = root_windows_[display.id()];
  DCHECK(root_to_delete) << display.ToString();

  // Display for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  root_windows_.erase(display.id());

  // When the primary root window's display is removed, move the primary
  // root to the other display.
  if (primary_display_id == display.id()) {
    // Temporarily store the primary root window in
    // |primary_root_window_for_replace_| when replacing the display.
    if (root_windows_.size() == 0) {
      primary_display_id = gfx::Display::kInvalidDisplayID;
      primary_root_window_for_replace_ = root_to_delete;
      return;
    }
    DCHECK_EQ(1U, root_windows_.size());
    primary_display_id = ScreenUtil::GetSecondaryDisplay().id();
    aura::Window* primary_root = root_to_delete;

    // Delete the other root instead.
    root_to_delete = root_windows_[primary_display_id];
    internal::GetRootWindowSettings(root_to_delete)->display_id = display.id();

    // Setup primary root.
    root_windows_[primary_display_id] = primary_root;
    internal::GetRootWindowSettings(primary_root)->display_id =
        primary_display_id;

    OnDisplayBoundsChanged(
        GetDisplayManager()->GetDisplayForId(primary_display_id));
  }
  internal::RootWindowController* controller =
      internal::GetRootWindowController(root_to_delete);
  DCHECK(controller);
  controller->MoveWindowsTo(GetPrimaryRootWindow());
  // Delete most of root window related objects, but don't delete
  // root window itself yet because the stack may be using it.
  controller->Shutdown();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, controller);
}

void DisplayController::OnWindowTreeHostResized(const aura::RootWindow* root) {
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      const_cast<aura::Window*>(root->window()));

  internal::DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->UpdateDisplayBounds(
          display.id(),
          root->host()->GetBounds())) {
    mirror_window_controller_->UpdateWindow();
    cursor_window_controller_->UpdateContainer();
  }
}

void DisplayController::CreateOrUpdateNonDesktopDisplay(
    const internal::DisplayInfo& info) {
  switch (GetDisplayManager()->second_display_mode()) {
    case internal::DisplayManager::MIRRORING:
      mirror_window_controller_->UpdateWindow(info);
      cursor_window_controller_->UpdateContainer();
      virtual_keyboard_window_controller_->Close();
      break;
    case internal::DisplayManager::VIRTUAL_KEYBOARD:
      mirror_window_controller_->Close();
      cursor_window_controller_->UpdateContainer();
      virtual_keyboard_window_controller_->UpdateWindow(info);
      break;
    case internal::DisplayManager::EXTENDED:
      NOTREACHED();
  }
}

void DisplayController::CloseNonDesktopDisplay() {
  mirror_window_controller_->Close();
  cursor_window_controller_->UpdateContainer();
  virtual_keyboard_window_controller_->Close();
}

void DisplayController::PreDisplayConfigurationChange(bool clear_focus) {
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanging());
  focus_activation_store_->Store(clear_focus);
  gfx::Screen* screen = Shell::GetScreen();
  gfx::Point point_in_screen = screen->GetCursorScreenPoint();
  gfx::Display display = screen->GetDisplayNearestPoint(point_in_screen);
  aura::Window* root_window = GetRootWindowForDisplayId(display.id());

  aura::client::ScreenPositionClient* client =
      aura::client::GetScreenPositionClient(root_window);
  client->ConvertPointFromScreen(root_window, &point_in_screen);
  root_window->GetDispatcher()->host()->ConvertPointToNativeScreen(
      &point_in_screen);
  cursor_location_in_native_coords_for_restore_ = point_in_screen;
}

void DisplayController::PostDisplayConfigurationChange() {
  if (limiter_)
    limiter_->SetThrottleTimeout(kAfterDisplayChangeThrottleTimeoutMs);

  focus_activation_store_->Restore();

  internal::DisplayManager* display_manager = GetDisplayManager();
  internal::DisplayLayoutStore* layout_store = display_manager->layout_store();
  if (display_manager->num_connected_displays() > 1) {
    DisplayIdPair pair = display_manager->GetCurrentDisplayIdPair();
    layout_store->UpdateMirrorStatus(pair, display_manager->IsMirrored());
    DisplayLayout layout = layout_store->GetRegisteredDisplayLayout(pair);

    if (Shell::GetScreen()->GetNumDisplays() > 1 ) {
      int64 primary_id = layout.primary_id;
      SetPrimaryDisplayId(
          primary_id == gfx::Display::kInvalidDisplayID ?
          pair.first : primary_id);
      // Update the primary_id in case the above call is
      // ignored. Happens when a) default layout's primary id
      // doesn't exist, or b) the primary_id has already been
      // set to the same and didn't update it.
      layout_store->UpdatePrimaryDisplayId(
          pair, Shell::GetScreen()->GetPrimaryDisplay().id());
    }
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanged());
  UpdateHostWindowNames();
  EnsurePointerInDisplays();
}

aura::RootWindow* DisplayController::AddRootWindowForDisplay(
    const gfx::Display& display) {
  static int root_window_count = 0;
  const internal::DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display.id());
  const gfx::Rect& bounds_in_native = display_info.bounds_in_native();
  aura::RootWindow::CreateParams params(bounds_in_native);
  params.host = Shell::GetInstance()->window_tree_host_factory()->
      CreateWindowTreeHost(bounds_in_native);
  aura::RootWindow* root_window = new aura::RootWindow(params);
  root_window->window()->SetName(
      base::StringPrintf("RootWindow-%d", root_window_count++));
  root_window->host()->compositor()->SetBackgroundColor(SK_ColorBLACK);
  // No need to remove RootWindowObserver because
  // the DisplayController object outlives RootWindow objects.
  root_window->AddRootWindowObserver(this);
  internal::InitRootWindowSettings(root_window->window())->display_id =
      display.id();
  root_window->host()->InitHost();

  root_windows_[display.id()] = root_window->window();
  SetDisplayPropertiesOnHostWindow(root_window, display);

#if defined(OS_CHROMEOS)
  static bool force_constrain_pointer_to_root =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshConstrainPointerToRoot);
  if (base::SysInfo::IsRunningOnChromeOS() || force_constrain_pointer_to_root)
    root_window->host()->ConfineCursorToRootWindow();
#endif
  return root_window;
}

void DisplayController::OnFadeOutForSwapDisplayFinished() {
#if defined(OS_CHROMEOS) && defined(USE_X11)
  SetPrimaryDisplay(ScreenUtil::GetSecondaryDisplay());
  Shell::GetInstance()->output_configurator_animation()->StartFadeInAnimation();
#endif
}

void DisplayController::UpdateHostWindowNames() {
#if defined(USE_X11)
  // crbug.com/120229 - set the window title for the primary dislpay
  // to "aura_root_0" so gtalk can find the primary root window to broadcast.
  // TODO(jhorwich) Remove this once Chrome supports window-based broadcasting.
  aura::Window* primary = Shell::GetPrimaryRootWindow();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    std::string name =
        root_windows[i] == primary ? "aura_root_0" : "aura_root_x";
    gfx::AcceleratedWidget xwindow =
        root_windows[i]->GetDispatcher()->host()->GetAcceleratedWidget();
    XStoreName(gfx::GetXDisplay(), xwindow, name.c_str());
  }
#endif
}

}  // namespace ash
