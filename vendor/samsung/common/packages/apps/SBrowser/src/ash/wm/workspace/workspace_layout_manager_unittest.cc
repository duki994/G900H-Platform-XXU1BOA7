// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/display/display_layout.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/window_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

class MaximizeDelegateView : public views::WidgetDelegateView {
 public:
  MaximizeDelegateView(const gfx::Rect& initial_bounds)
      : initial_bounds_(initial_bounds) {
  }
  virtual ~MaximizeDelegateView() {}

  virtual bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE {
    *bounds = initial_bounds_;
    *show_state = ui::SHOW_STATE_MAXIMIZED;
    return true;
  }

 private:
  const gfx::Rect initial_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeDelegateView);
};

class TestShellObserver : public ShellObserver {
 public:
  TestShellObserver() : call_count_(0),
                        is_fullscreen_(false) {
    Shell::GetInstance()->AddShellObserver(this);
  }

  virtual ~TestShellObserver() {
    Shell::GetInstance()->RemoveShellObserver(this);
  }

  virtual void OnFullscreenStateChanged(bool is_fullscreen,
                                        aura::Window* root_window) OVERRIDE {
    call_count_++;
    is_fullscreen_ = is_fullscreen;
  }

  int call_count() const {
    return call_count_;
  }

  bool is_fullscreen() const {
    return is_fullscreen_;
  }

 private:
  int call_count_;
  bool is_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(TestShellObserver);
};

}  // namespace

typedef test::AshTestBase WorkspaceLayoutManagerTest;

// Verifies that a window containing a restore coordinate will be restored to
// to the size prior to minimize, keeping the restore rectangle in tact (if
// there is one).
TEST_F(WorkspaceLayoutManagerTest, RestoreFromMinimizeKeepsRestore) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // This will not be used for un-minimizing window.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(0, 0, 100, 100));
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ("0,0 100x100", window_state->GetRestoreBoundsInScreen().ToString());
  EXPECT_EQ("10,15 25x35", window.get()->bounds().ToString());

  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x300,500x400");
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenUtil::GetSecondaryDisplay());
  EXPECT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());
  window_state->Minimize();
  // This will not be used for un-minimizing window.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(0, 0, 100, 100));
  window_state->Restore();
  EXPECT_EQ("600,0 100x100", window->GetBoundsInScreen().ToString());

  // Make sure the unminimized window moves inside the display when
  // 2nd display is disconnected.
  window_state->Minimize();
  UpdateDisplay("400x300");
  window_state->Restore();
  EXPECT_EQ(Shell::GetPrimaryRootWindow(), window->GetRootWindow());
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
}

TEST_F(WorkspaceLayoutManagerTest, KeepMinimumVisibilityInDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x400,400x500");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  DisplayLayout layout(DisplayLayout::TOP, 0);
  Shell::GetInstance()->display_manager()->
      SetLayoutForCurrentDisplays(layout);
  EXPECT_EQ("0,-500 400x500", root_windows[1]->GetBoundsInScreen().ToString());

  scoped_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, -400, 200, 200)));
  EXPECT_EQ("10,-400 200x200", window1->GetBoundsInScreen().ToString());

  // Make sure the caption is visible.
  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, -600, 200, 200)));
  EXPECT_EQ("10,-500 200x200", window2->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, KeepRestoredWindowInDisplay) {
  if (!SupportsHostWindowResize())
    return;
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Maximized -> Normal transition.
  window_state->Maximize();
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 30, 40));
  window_state->Restore();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-20,0 30x40", window->bounds().ToString());

  // Minimized -> Normal transition.
  window->SetBounds(gfx::Rect(-100, -100, 30, 40));
  window_state->Minimize();
  EXPECT_FALSE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  EXPECT_EQ("-100,-100 30x40", window->bounds().ToString());
  window->Show();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-20,0 30x40", window->bounds().ToString());

  // Fullscreen -> Normal transition.
  window->SetBounds(gfx::Rect(0, 0, 30, 40));  // reset bounds.
  ASSERT_EQ("0,0 30x40", window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(window->bounds(), window->GetRootWindow()->bounds());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 30, 40));
  window_state->Restore();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-20,0 30x40", window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, MaximizeInDisplayToBeRestored) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("300x400,400x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x453", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 30x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(280, 0, 30, 40));
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x453", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("280,0 30x40", window->GetBoundsInScreen().ToString());

  // Restoring widget state.
  scoped_ptr<views::Widget> w1(new views::Widget);
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = new MaximizeDelegateView(gfx::Rect(400, 0, 30, 40));
  params.context = root_windows[0];
  w1->Init(params);
  w1->Show();
  EXPECT_TRUE(w1->IsMaximized());
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("300,0 400x453", w1->GetWindowBoundsInScreen().ToString());
  w1->Restore();
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("400,0 30x40", w1->GetWindowBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, FullscreenInDisplayToBeRestored) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("300x400,400x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 30x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(280, 0, 30, 40));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("280,0 30x40", window->GetBoundsInScreen().ToString());
}

// WindowObserver implementation used by DontClobberRestoreBoundsWindowObserver.
// This code mirrors what BrowserFrameAsh does. In particular when this code
// sees the window was maximized it changes the bounds of a secondary
// window. The secondary window mirrors the status window.
class DontClobberRestoreBoundsWindowObserver : public aura::WindowObserver {
 public:
  DontClobberRestoreBoundsWindowObserver() : window_(NULL) {}

  void set_window(aura::Window* window) { window_ = window; }

  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    if (!window_)
      return;

    if (wm::GetWindowState(window)->IsMaximized()) {
      aura::Window* w = window_;
      window_ = NULL;

      gfx::Rect shelf_bounds(Shell::GetPrimaryRootWindowController()->
                             GetShelfLayoutManager()->GetIdealBounds());
      const gfx::Rect& window_bounds(w->bounds());
      w->SetBounds(gfx::Rect(window_bounds.x(), shelf_bounds.y() - 1,
                             window_bounds.width(), window_bounds.height()));
    }
  }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DontClobberRestoreBoundsWindowObserver);
};

// Creates a window, maximized the window and from within the maximized
// notification sets the bounds of a window to overlap the shelf. Verifies this
// doesn't effect the restore bounds.
TEST_F(WorkspaceLayoutManagerTest, DontClobberRestoreBounds) {
  DontClobberRestoreBoundsWindowObserver window_observer;
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(10, 20, 30, 40));
  // NOTE: for this test to exercise the failure the observer needs to be added
  // before the parent set. This mimics what BrowserFrameAsh does.
  window->AddObserver(&window_observer);
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->Activate();

  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 30, 40)));
  views::corewm::AddTransientChild(window.get(), window2.get());
  window2->Show();

  window_observer.set_window(window2.get());
  window_state->Maximize();
  EXPECT_EQ("10,20 30x40",
            window_state->GetRestoreBoundsInScreen().ToString());
  window->RemoveObserver(&window_observer);
}

// Verifies when a window is maximized all descendant windows have a size.
TEST_F(WorkspaceLayoutManagerTest, ChildBoundsResetOnMaximize) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 30, 40)));
  window->Show();
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->Activate();
  scoped_ptr<aura::Window> child_window(
      aura::test::CreateTestWindowWithBounds(gfx::Rect(5, 6, 7, 8),
                                             window.get()));
  child_window->Show();
  window_state->Maximize();
  EXPECT_EQ("5,6 7x8", child_window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, WindowShouldBeOnScreenWhenAdded) {
  // Normal window bounds shouldn't be changed.
  gfx::Rect window_bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds, window->bounds());

  // If the window is out of the workspace, it would be moved on screen.
  gfx::Rect root_window_bounds =
      Shell::GetInstance()->GetPrimaryRootWindow()->bounds();
  window_bounds.Offset(root_window_bounds.width(), root_window_bounds.height());
  ASSERT_FALSE(window_bounds.Intersects(root_window_bounds));
  scoped_ptr<aura::Window> out_window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds.size(), out_window->bounds().size());
  gfx::Rect bounds = out_window->bounds();
  bounds.Intersect(root_window_bounds);

  // 30% of the window edge must be visible.
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  aura::Window* parent = out_window->parent();
  parent->RemoveChild(out_window.get());
  out_window->SetBounds(gfx::Rect(-200, -200, 200, 200));
  // UserHasChangedWindowPositionOrSize flag shouldn't turn off this behavior.
  wm::GetWindowState(window.get())->set_bounds_changed_by_user(true);
  parent->AddChild(out_window.get());
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  // Make sure we always make more than 1/3 of the window edge visible even
  // if the initial bounds intersects with display.
  window_bounds.SetRect(-150, -150, 200, 200);
  bounds = window_bounds;
  bounds.Intersect(root_window_bounds);

  // Make sure that the initial bounds' visible area is less than 26%
  // so that the auto adjustment logic kicks in.
  ASSERT_LT(bounds.width(), out_window->bounds().width() * 0.26);
  ASSERT_LT(bounds.height(), out_window->bounds().height() * 0.26);
  ASSERT_TRUE(window_bounds.Intersects(root_window_bounds));

  scoped_ptr<aura::Window> partially_out_window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds.size(), partially_out_window->bounds().size());
  bounds = partially_out_window->bounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  // Make sure the window whose 30% width/height is bigger than display
  // will be placed correctly.
  window_bounds.SetRect(-1900, -1900, 3000, 3000);
  scoped_ptr<aura::Window> window_bigger_than_display(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_GE(root_window_bounds.width(),
            window_bigger_than_display->bounds().width());
  EXPECT_GE(root_window_bounds.height(),
            window_bigger_than_display->bounds().height());

  bounds = window_bigger_than_display->bounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);
}

// Verifies the size of a window is enforced to be smaller than the work area.
TEST_F(WorkspaceLayoutManagerTest, SizeToWorkArea) {
  // Normal window bounds shouldn't be changed.
  gfx::Size work_area(
      Shell::GetScreen()->GetPrimaryDisplay().work_area().size());
  const gfx::Rect window_bounds(
      100, 101, work_area.width() + 1, work_area.height() + 2);
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
      window->bounds().ToString());

  // Directly setting the bounds triggers a slightly different code path. Verify
  // that too.
  window->SetBounds(window_bounds);
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
      window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, NotifyFullscreenChanges) {
  TestShellObserver observer;
  scoped_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  wm::WindowState* window_state1 = wm::GetWindowState(window1.get());
  wm::WindowState* window_state2 = wm::GetWindowState(window2.get());
  window_state2->Activate();

  window_state2->ToggleFullscreen();
  EXPECT_EQ(1, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  // When window1 moves to the front the fullscreen state should change.
  window_state1->Activate();
  EXPECT_EQ(2, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());

  // It should change back if window2 becomes active again.
  window_state2->Activate();
  EXPECT_EQ(3, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  window_state2->ToggleFullscreen();
  EXPECT_EQ(4, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());

  window_state2->ToggleFullscreen();
  EXPECT_EQ(5, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  // Closing the window should change the fullscreen state.
  window2.reset();
  EXPECT_EQ(6, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());
}

// Following tests were originally written for BaseLayoutManager.

namespace {

class WorkspaceLayoutManagerSoloTest : public test::AshTestBase {
 public:
  WorkspaceLayoutManagerSoloTest() {}
  virtual ~WorkspaceLayoutManagerSoloTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    UpdateDisplay("800x600");
    aura::Window* default_container = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        internal::kShellWindowId_DefaultContainer);
    default_container->SetLayoutManager(new internal::WorkspaceLayoutManager(
        Shell::GetPrimaryRootWindow()));
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithBounds(bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerSoloTest);
};

}  // namespace

// Tests normal->maximize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Maximize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  // Maximized window fills the work area, not the whole display.
  EXPECT_EQ(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Tests normal->minimize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Minimize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  // Note: Currently minimize doesn't do anything except set the state.
  // See crbug.com/104571.
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// A WindowDelegate which sets the focus when the window
// becomes visible.
class FocusDelegate : public aura::test::TestWindowDelegate {
 public:
  FocusDelegate()
      : window_(NULL),
        show_state_(ui::SHOW_STATE_END) {
  }
  virtual ~FocusDelegate() {}

  void set_window(aura::Window* window) { window_ = window; }

  // aura::test::TestWindowDelegate overrides:
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {
    if (window_) {
      if (visible)
        window_->Focus();
      show_state_ = window_->GetProperty(aura::client::kShowStateKey);
    }
  }

  ui::WindowShowState GetShowStateAndReset() {
    ui::WindowShowState ret = show_state_;
    show_state_ = ui::SHOW_STATE_END;
    return ret;
  }

 private:
  aura::Window* window_;
  ui::WindowShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(FocusDelegate);
};

// Make sure that the window's show state is correct in
// |WindowDelegate::OnWindowTargetVisibilityChanged|, and setting
// focus in this callback doesn't cause DCHECK error.  See
// crbug.com/168383.
TEST_F(WorkspaceLayoutManagerSoloTest, FocusDuringUnminimize) {
  FocusDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(100, 100, 100, 100)));
  delegate.set_window(window.get());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED, delegate.GetShowStateAndReset());
  window->Show();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, delegate.GetShowStateAndReset());
}

// Tests maximized window size during root window resize.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  gfx::Rect initial_work_area_bounds =
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get());
  EXPECT_EQ(initial_work_area_bounds.ToString(), window->bounds().ToString());
  // Enlarge the root window.  We should still match the work area size.
  UpdateDisplay("900x700");
  EXPECT_EQ(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  EXPECT_NE(
      initial_work_area_bounds.ToString(),
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString());
}

// Tests normal->fullscreen->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Fullscreen) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  // Fullscreen window fills the whole display.
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Tests fullscreen window size during root window resize.
TEST_F(WorkspaceLayoutManagerSoloTest, FullscreenRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  // Fullscreen window fills the whole display.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
  // Enlarge the root window.  We should still match the display size.
  UpdateDisplay("800x600");
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
}

// Tests that when the screen gets smaller the windows aren't bigger than
// the screen.
TEST_F(WorkspaceLayoutManagerSoloTest, RootWindowResizeShrinksWindows) {
  scoped_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 500, 400)));
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  // Invariant: Window is smaller than work area.
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window narrower than our window.
  UpdateDisplay("300x400");
  work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window shorter than our window.
  UpdateDisplay("300x200");
  work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Enlarging the root window does not change the window bounds.
  gfx::Rect old_bounds = window->bounds();
  UpdateDisplay("800x600");
  EXPECT_EQ(old_bounds.width(), window->bounds().width());
  EXPECT_EQ(old_bounds.height(), window->bounds().height());
}

// Tests that a maximized window with too-large restore bounds will be restored
// to smaller than the full work area.
TEST_F(WorkspaceLayoutManagerSoloTest, BoundsWithScreenEdgeVisible) {
  // Create a window with bounds that fill the screen.
  gfx::Rect bounds = Shell::GetScreen()->GetPrimaryDisplay().bounds();
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  // Maximize it, which writes the old bounds to restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  // Restore it.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  // It should have the default maximized window bounds, inset by the grid size.
  int grid_size = internal::WorkspaceWindowResizer::kScreenEdgeInset;
  gfx::Rect max_bounds =
      ash::ScreenUtil::GetMaximizedWindowBoundsInParent(window.get());
  max_bounds.Inset(grid_size, grid_size);
  EXPECT_EQ(max_bounds.ToString(), window->bounds().ToString());
}

// Verifies maximizing sets the restore bounds, and restoring
// restores the bounds.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeSetsRestoreBounds) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(10, 20, 30, 40)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("10,20 30x40", window_state->GetRestoreBoundsInParent().ToString());

  // Restore it, which should restore bounds and reset restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ("10,20 30x40", window->bounds().ToString());
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies maximizing keeps the restore bounds if set.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeResetsRestoreBounds) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInParent(gfx::Rect(10, 11, 12, 13));

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("10,11 12x13", window_state->GetRestoreBoundsInParent().ToString());
}

// Verifies that the restore bounds do not get reset when restoring to a
// maximzied state from a minimized state.
TEST_F(WorkspaceLayoutManagerSoloTest,
       BoundsAfterRestoringToMaximizeFromMinimize) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  // Maximize it, which should reset restore bounds.
  window_state->Maximize();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());
  // Minimize the window. The restore bounds should not change.
  window_state->Minimize();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());

  // Show the window again. The window should be maximized, and the restore
  // bounds should not change.
  window->Show();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());
  EXPECT_TRUE(window_state->IsMaximized());

  window_state->Restore();
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Verify if the window is not resized during screen lock. See: crbug.com/173127
TEST_F(WorkspaceLayoutManagerSoloTest, NotResizeWhenScreenIsLocked) {
  SetCanLockScreen(true);
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  // window with AlwaysOnTop will be managed by BaseLayoutManager.
  window->SetProperty(aura::client::kAlwaysOnTopKey, true);
  window->Show();

  internal::ShelfLayoutManager* shelf =
      internal::ShelfLayoutManager::ForShelf(window.get());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  window->SetBounds(ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()));
  gfx::Rect window_bounds = window->bounds();
  EXPECT_EQ(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window_bounds.ToString());

  Shell::GetInstance()->session_state_delegate()->LockScreen();
  shelf->UpdateVisibilityState();
  EXPECT_NE(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window_bounds.ToString());

  Shell::GetInstance()->session_state_delegate()->UnlockScreen();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(window_bounds.ToString(), window->bounds().ToString());
}

}  // namespace ash
