// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace_controller_test_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_move_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/window_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {
namespace internal {

class WorkspaceEventHandlerTest : public test::AshTestBase {
 public:
  WorkspaceEventHandlerTest() {}
  virtual ~WorkspaceEventHandlerTest() {}

 protected:
  aura::Window* CreateTestWindow(aura::WindowDelegate* delegate,
                                 const gfx::Rect& bounds) {
    aura::Window* window = new aura::Window(delegate);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(aura::WINDOW_LAYER_TEXTURED);
    ParentWindowInPrimaryRootWindow(window);
    window->SetBounds(bounds);
    window->Show();
    return window;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandlerTest);
};

// Keeps track of the properties changed of a particular window.
class WindowPropertyObserver : public aura::WindowObserver {
 public:
  explicit WindowPropertyObserver(aura::Window* window)
      : window_(window) {
    window->AddObserver(this);
  }

  virtual ~WindowPropertyObserver() {
    window_->RemoveObserver(this);
  }

  bool DidPropertyChange(const void* property) const {
    return std::find(properties_changed_.begin(),
                     properties_changed_.end(),
                     property) != properties_changed_.end();
  }

 private:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    properties_changed_.push_back(key);
  }

  aura::Window* window_;
  std::vector<const void*> properties_changed_;

  DISALLOW_COPY_AND_ASSIGN(WindowPropertyObserver);
};

TEST_F(WorkspaceEventHandlerTest, DoubleClickSingleAxisResizeEdge) {
  // Double clicking the vertical resize edge of a window should maximize it
  // vertically.
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, restored_bounds));

  wm::ActivateWindow(window.get());

  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());

  // Double-click the top resize edge.
  wd.set_window_component(HTTOP);
  // On X a double click actually generates a drag between each press/release.
  // Explicitly trigger this path since we had bugs in dealing with it
  // correctly.
  generator.PressLeftButton();
  generator.ReleaseLeftButton();
  generator.set_flags(ui::EF_IS_DOUBLE_CLICK);
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_location(), 1);
  generator.ReleaseLeftButton();
  gfx::Rect bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.x(), bounds_in_screen.x());
  EXPECT_EQ(restored_bounds.width(), bounds_in_screen.width());
  EXPECT_EQ(work_area.y(), bounds_in_screen.y());
  EXPECT_EQ(work_area.height(), bounds_in_screen.height());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  // Single-axis maximization is not considered real maximization.
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator.DoubleClickLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.ToString(), bounds_in_screen.ToString());
  // Note that it should not even be restored at this point, it should have
  // also cleared the restore rectangle.
  EXPECT_FALSE(window_state->HasRestoreBounds());

  // Double clicking the left resize edge should maximize horizontally.
  wd.set_window_component(HTLEFT);
  generator.DoubleClickLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.y(), bounds_in_screen.y());
  EXPECT_EQ(restored_bounds.height(), bounds_in_screen.height());
  EXPECT_EQ(work_area.x(), bounds_in_screen.x());
  EXPECT_EQ(work_area.width(), bounds_in_screen.width());
  // Single-axis maximization is not considered real maximization.
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator.DoubleClickLeftButton();
  EXPECT_EQ(restored_bounds.ToString(), window->GetBoundsInScreen().ToString());

#if defined(OS_WIN)
  // Multi display test does not run on Win8 bot. crbug.com/247427.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return;
#endif

  // Verify the double clicking the resize edge works on 2nd display too.
  UpdateDisplay("200x200,400x300");
  gfx::Rect work_area2 = ScreenUtil::GetSecondaryDisplay().work_area();
  restored_bounds.SetRect(220,20, 50, 50);
  window->SetBoundsInScreen(restored_bounds, ScreenUtil::GetSecondaryDisplay());
  aura::Window* second_root = Shell::GetAllRootWindows()[1];
  EXPECT_EQ(second_root, window->GetRootWindow());
  aura::test::EventGenerator generator2(second_root, window.get());

  // Y-axis maximization.
  wd.set_window_component(HTTOP);
  generator2.PressLeftButton();
  generator2.ReleaseLeftButton();
  generator2.set_flags(ui::EF_IS_DOUBLE_CLICK);
  generator2.PressLeftButton();
  generator2.MoveMouseTo(generator.current_location(), 1);
  generator2.ReleaseLeftButton();
  generator.DoubleClickLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.x(), bounds_in_screen.x());
  EXPECT_EQ(restored_bounds.width(), bounds_in_screen.width());
  EXPECT_EQ(work_area2.y(), bounds_in_screen.y());
  EXPECT_EQ(work_area2.height(), bounds_in_screen.height());
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator2.DoubleClickLeftButton();
  EXPECT_EQ(restored_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // X-axis maximization.
  wd.set_window_component(HTLEFT);
  generator2.DoubleClickLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.y(), bounds_in_screen.y());
  EXPECT_EQ(restored_bounds.height(), bounds_in_screen.height());
  EXPECT_EQ(work_area2.x(), bounds_in_screen.x());
  EXPECT_EQ(work_area2.width(), bounds_in_screen.width());
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator2.DoubleClickLeftButton();
  EXPECT_EQ(restored_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

// Tests the behavior when double clicking the border of a side snapped window.
TEST_F(WorkspaceEventHandlerTest, DoubleClickSingleAxisWhenSideSnapped) {
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, restored_bounds));

  gfx::Rect work_area_in_screen = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  internal::SnapSizer::SnapWindow(window_state, internal::SnapSizer::LEFT_EDGE);

  gfx::Rect snapped_bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(work_area_in_screen.x(), snapped_bounds_in_screen.x());
  EXPECT_EQ(work_area_in_screen.y(), snapped_bounds_in_screen.y());
  EXPECT_GT(work_area_in_screen.width(), snapped_bounds_in_screen.width());
  EXPECT_EQ(work_area_in_screen.height(), snapped_bounds_in_screen.height());

  // Double clicking the top border should not do anything for side snapped
  // windows. (They already take up the entire workspace height and reverting
  // to the restored bounds would be weird).
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());
  wd.set_window_component(HTTOP);
  generator.DoubleClickLeftButton();
  EXPECT_EQ(wm::SHOW_TYPE_LEFT_SNAPPED, window_state->window_show_type());
  EXPECT_EQ(snapped_bounds_in_screen.ToString(),
            window->GetBoundsInScreen().ToString());

  // Double clicking the right border should exit the side snapped state and
  // make the window take up the entire work area.
  wd.set_window_component(HTRIGHT);
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalShowType());
  EXPECT_EQ(work_area_in_screen.ToString(),
            window->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceEventHandlerTest,
       DoubleClickSingleAxisDoesntResizeVerticalEdgeIfConstrained) {
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, restored_bounds));

  wm::ActivateWindow(window.get());

  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();

  wd.set_maximum_size(gfx::Size(0, 100));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());
  // Double-click the top resize edge.
  wd.set_window_component(HTTOP);
  generator.DoubleClickLeftButton();

  // The size of the window should be unchanged.
  EXPECT_EQ(restored_bounds.y(), window->bounds().y());
  EXPECT_EQ(restored_bounds.height(), window->bounds().height());
}

TEST_F(WorkspaceEventHandlerTest,
       DoubleClickSingleAxisDoesntResizeHorizontalEdgeIfConstrained) {
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, restored_bounds));

  wm::ActivateWindow(window.get());

  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();

  wd.set_maximum_size(gfx::Size(100, 0));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());
  // Double-click the top resize edge.
  wd.set_window_component(HTRIGHT);
  generator.DoubleClickLeftButton();

  // The size of the window should be unchanged.
  EXPECT_EQ(restored_bounds.x(), window->bounds().x());
  EXPECT_EQ(restored_bounds.width(), window->bounds().width());
}

TEST_F(WorkspaceEventHandlerTest,
       DoubleClickOrTapWithModalChildDoesntMaximize) {
  aura::test::TestWindowDelegate wd1;
  aura::test::TestWindowDelegate wd2;
  scoped_ptr<aura::Window> window(
      CreateTestWindow(&wd1, gfx::Rect(10, 20, 30, 40)));
  scoped_ptr<aura::Window> child(
      CreateTestWindow(&wd2, gfx::Rect(0, 0, 1, 1)));
  window->SetProperty(aura::client::kCanMaximizeKey, true);
  wd1.set_window_component(HTCAPTION);

  child->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  views::corewm::AddTransientChild(window.get(), child.get());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator generator(root, window.get());
  generator.DoubleClickLeftButton();
  EXPECT_EQ("10,20 30x40", window->bounds().ToString());
  EXPECT_FALSE(window_state->IsMaximized());

  generator.GestureTapAt(gfx::Point(25, 25));
  generator.GestureTapAt(gfx::Point(25, 25));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("10,20 30x40", window->bounds().ToString());
  EXPECT_FALSE(window_state->IsMaximized());
}

// Test the behavior as a result of double clicking the window header.
TEST_F(WorkspaceEventHandlerTest, DoubleClickCaptionTogglesMaximize) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(
      CreateTestWindow(&wd, gfx::Rect(1, 2, 30, 40)));
  window->SetProperty(aura::client::kCanMaximizeKey, true);

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  gfx::Rect restore_bounds = window->bounds();
  gfx::Rect work_area_in_parent = ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      window.get());

  EXPECT_FALSE(window_state->IsMaximized());

  // 1) Double clicking a normal window should maximize.
  wd.set_window_component(HTCAPTION);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator generator(root, window.get());
  generator.DoubleClickLeftButton();
  EXPECT_NE(restore_bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(window_state->IsMaximized());

  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalShowType());
  EXPECT_EQ(restore_bounds.ToString(), window->bounds().ToString());

  // 2) Double clicking a horizontally maximized window should maximize.
  wd.set_window_component(HTLEFT);
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalShowType());
  EXPECT_EQ(work_area_in_parent.x(), window->bounds().x());
  EXPECT_EQ(restore_bounds.y(), window->bounds().y());
  EXPECT_EQ(work_area_in_parent.width(), window->bounds().width());
  EXPECT_EQ(restore_bounds.height(), window->bounds().height());

  wd.set_window_component(HTCAPTION);
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsMaximized());

  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalShowType());
  EXPECT_EQ(restore_bounds.ToString(), window->bounds().ToString());

  // 3) Double clicking a snapped window should maximize.
  internal::SnapSizer::SnapWindow(window_state, internal::SnapSizer::LEFT_EDGE);
  EXPECT_TRUE(window_state->IsSnapped());
  generator.MoveMouseTo(window->GetBoundsInRootWindow().CenterPoint());
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsMaximized());

  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalShowType());
  EXPECT_EQ(restore_bounds.ToString(), window->bounds().ToString());
}

// Test that double clicking the middle button on the window header does not
// toggle the maximized state.
TEST_F(WorkspaceEventHandlerTest,
       DoubleClickMiddleButtonDoesNotToggleMaximize) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(
      CreateTestWindow(&wd, gfx::Rect(1, 2, 30, 40)));
  window->SetProperty(aura::client::kCanMaximizeKey, true);
  wd.set_window_component(HTCAPTION);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator generator(root, window.get());

  WindowPropertyObserver observer(window.get());
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, generator.current_location(),
                       generator.current_location(),
                       ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK,
                       ui::EF_MIDDLE_MOUSE_BUTTON);
  aura::WindowEventDispatcher* dispatcher = root->GetDispatcher();
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, generator.current_location(),
                         generator.current_location(),
                         ui::EF_IS_DOUBLE_CLICK,
                         ui::EF_MIDDLE_MOUSE_BUTTON);
  details = dispatcher->OnEventFromSource(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);

  EXPECT_FALSE(wm::GetWindowState(window.get())->IsMaximized());
  EXPECT_EQ("1,2 30x40", window->bounds().ToString());
  EXPECT_FALSE(observer.DidPropertyChange(aura::client::kShowStateKey));
}

TEST_F(WorkspaceEventHandlerTest, DoubleTapCaptionTogglesMaximize) {
  aura::test::TestWindowDelegate wd;
  gfx::Rect bounds(10, 20, 30, 40);
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, bounds));
  window->SetProperty(aura::client::kCanMaximizeKey, true);
  wd.set_window_component(HTCAPTION);

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());
  generator.GestureTapAt(gfx::Point(25, 25));
  generator.GestureTapAt(gfx::Point(25, 25));
  RunAllPendingInMessageLoop();
  EXPECT_NE(bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(window_state->IsMaximized());

  generator.GestureTapAt(gfx::Point(5, 5));
  generator.GestureTapAt(gfx::Point(10, 10));

  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Verifies deleting the window while dragging doesn't crash.
TEST_F(WorkspaceEventHandlerTest, DeleteWhenDragging) {
  // Create a large window in the background. This is necessary so that when we
  // delete |window| WorkspaceEventHandler is still the active event handler.
  aura::test::TestWindowDelegate wd2;
  scoped_ptr<aura::Window> window2(
      CreateTestWindow(&wd2, gfx::Rect(0, 0, 500, 500)));

  aura::test::TestWindowDelegate wd;
  const gfx::Rect bounds(10, 20, 30, 40);
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, bounds));
  wd.set_window_component(HTCAPTION);
  aura::test::EventGenerator generator(window->GetRootWindow());
  generator.MoveMouseToCenterOf(window.get());
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_location() + gfx::Vector2d(50, 50));
  DCHECK_NE(bounds.origin().ToString(), window->bounds().origin().ToString());
  window.reset();
  generator.MoveMouseTo(generator.current_location() + gfx::Vector2d(50, 50));
}

// Verifies deleting the window while in a run loop doesn't crash.
TEST_F(WorkspaceEventHandlerTest, DeleteWhileInRunLoop) {
  aura::test::TestWindowDelegate wd;
  const gfx::Rect bounds(10, 20, 30, 40);
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, bounds));
  wd.set_window_component(HTCAPTION);

  ASSERT_TRUE(aura::client::GetWindowMoveClient(window->GetRootWindow()));
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, window.get());
  aura::client::GetWindowMoveClient(window->GetRootWindow())
      ->RunMoveLoop(window.release(),
                    gfx::Vector2d(),
                    aura::client::WINDOW_MOVE_SOURCE_MOUSE);
}

}  // namespace internal
}  // namespace ash
