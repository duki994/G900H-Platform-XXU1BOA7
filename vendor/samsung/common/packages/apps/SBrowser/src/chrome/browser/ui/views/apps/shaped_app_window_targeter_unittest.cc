// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"

#include "chrome/browser/ui/views/apps/native_app_window_views.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/wm/public/easy_resize_window_targeter.h"

class ShapedAppWindowTargeterTest : public aura::test::AuraTestBase {
 public:
  ShapedAppWindowTargeterTest()
      : web_view_(NULL) {
  }

  virtual ~ShapedAppWindowTargeterTest() {}

  views::Widget* widget() { return widget_.get(); }

  apps::NativeAppWindow* app_window() { return &app_window_; }
  NativeAppWindowViews* app_window_views() { return &app_window_; }

 protected:
  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    widget_.reset(new views::Widget);
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.remove_standard_frame = true;
    params.bounds = gfx::Rect(30, 30, 100, 100);
    params.context = root_window();
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(params);

    app_window_.web_view_ = &web_view_;
    app_window_.window_ = widget_.get();

    widget_->Show();
  }

  virtual void TearDown() OVERRIDE {
    widget_.reset();
    aura::test::AuraTestBase::TearDown();
  }

 private:
  views::WebView web_view_;
  scoped_ptr<views::Widget> widget_;
  NativeAppWindowViews app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShapedAppWindowTargeterTest);
};

TEST_F(ShapedAppWindowTargeterTest, HitTestBasic) {
  aura::Window* window = widget()->GetNativeWindow();
  {
    // Without any custom shapes, the event should be targeted correctly to the
    // window.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(40, 40), gfx::Point(40, 40),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }

  scoped_ptr<SkRegion> region(new SkRegion);
  region->op(SkIRect::MakeXYWH(40, 0, 20, 100), SkRegion::kUnion_Op);
  region->op(SkIRect::MakeXYWH(0, 40, 100, 20), SkRegion::kUnion_Op);
  app_window()->UpdateShape(region.Pass());
  {
    // With the custom shape, the events that don't fall within the custom shape
    // will go through to the root window.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(40, 40), gfx::Point(40, 40),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());

    // But events within the shape will still reach the window.
    ui::MouseEvent move2(ui::ET_MOUSE_MOVED,
                         gfx::Point(80, 80), gfx::Point(80, 80),
                         ui::EF_NONE, ui::EF_NONE);
    details = dispatcher()->OnEventFromSource(&move2);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move2.target());
  }
}

TEST_F(ShapedAppWindowTargeterTest, HitTestOnlyForShapedWindow) {
  // Install a window-targeter on the root window that allows a window to
  // receive events outside of its bounds. Verify that this window-targeter is
  // active unless the window has a custom shape.
  gfx::Insets inset(-30, -30, -30, -30);
  root_window()->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
      new wm::EasyResizeWindowTargeter(root_window(), inset, inset)));

  aura::Window* window = widget()->GetNativeWindow();
  {
    // Without any custom shapes, an event within the window bounds should be
    // targeted correctly to the window.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(40, 40), gfx::Point(40, 40),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }
  {
    // Without any custom shapes, an event that falls just outside the window
    // bounds should also be targeted correctly to the window, because of the
    // targeter installed on the root-window.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(10, 10), gfx::Point(10, 10),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }

  scoped_ptr<SkRegion> region(new SkRegion);
  region->op(SkIRect::MakeXYWH(40, 0, 20, 100), SkRegion::kUnion_Op);
  region->op(SkIRect::MakeXYWH(0, 40, 100, 20), SkRegion::kUnion_Op);
  app_window()->UpdateShape(region.Pass());
  {
    // With the custom shape, the events that don't fall within the custom shape
    // will go through to the root window.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(10, 10), gfx::Point(10, 10),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }

  // Remove the custom shape. This should restore the behaviour of targeting the
  // app window for events just outside its bounds.
  app_window()->UpdateShape(scoped_ptr<SkRegion>());
  {
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(10, 10), gfx::Point(10, 10),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }
}

// Tests targeting of events on a window with an EasyResizeWindowTargeter
// installed on its container.
TEST_F(ShapedAppWindowTargeterTest, ResizeInsetsWithinBounds) {
  aura::Window* window = widget()->GetNativeWindow();
  {
    // An event in the center of the window should always have
    // |window| as its target.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(80, 80), gfx::Point(80, 80),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }
  {
    // Without an EasyResizeTargeter on the container, an event
    // inside the window and within 5px of an edge should have
    // |window| as its target.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(32, 37), gfx::Point(32, 37),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }

  // The EasyResizeTargeter specifies an inset of 5px within the window.
  app_window_views()->InstallEasyResizeTargeterOnContainer();

  {
    // An event in the center of the window should always have
    // |window| as its target.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(80, 80), gfx::Point(80, 80),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window, move.target());
  }
  {
    // With an EasyResizeTargeter on the container, an event
    // inside the window and within 5px of an edge should have
    // root_window() as its target.
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(32, 37), gfx::Point(32, 37),
                        ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = dispatcher()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }
}
