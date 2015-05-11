// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_widget_test.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/size.h"
#include "ui/surface/transport_dib.h"

namespace content {

const int RenderWidgetTest::kNumBytesPerPixel = 4;
const int RenderWidgetTest::kLargeWidth = 1024;
const int RenderWidgetTest::kLargeHeight = 768;
const int RenderWidgetTest::kSmallWidth = 600;
const int RenderWidgetTest::kSmallHeight = 450;
const int RenderWidgetTest::kTextPositionX = 800;
const int RenderWidgetTest::kTextPositionY = 600;
const uint32 RenderWidgetTest::kRedARGB = 0xFFFF0000;

RenderWidgetTest::RenderWidgetTest() {}

void RenderWidgetTest::TestOnResize() {
  RenderWidget* widget = static_cast<RenderViewImpl*>(view_);

  // The initial bounds is empty, so setting it to the same thing should do
  // nothing.
  ViewMsg_Resize_Params resize_params;
  resize_params.screen_info = blink::WebScreenInfo();
  resize_params.new_size = gfx::Size();
  resize_params.physical_backing_size = gfx::Size();
  resize_params.overdraw_bottom_height = 0.f;
  resize_params.resizer_rect = gfx::Rect();
  resize_params.is_fullscreen = false;
  widget->OnResize(resize_params);
  EXPECT_FALSE(widget->next_paint_is_resize_ack());

  // Setting empty physical backing size should not send the ack.
  resize_params.new_size = gfx::Size(10, 10);
  widget->OnResize(resize_params);
  EXPECT_FALSE(widget->next_paint_is_resize_ack());

  // Setting the bounds to a "real" rect should send the ack.
  render_thread_->sink().ClearMessages();
  gfx::Size size(100, 100);
  resize_params.new_size = size;
  resize_params.physical_backing_size = size;
  widget->OnResize(resize_params);
  EXPECT_TRUE(widget->next_paint_is_resize_ack());
  widget->DoDeferredUpdate();
  ProcessPendingMessages();

  const ViewHostMsg_UpdateRect* msg =
      static_cast<const ViewHostMsg_UpdateRect*>(
          render_thread_->sink().GetUniqueMessageMatching(
              ViewHostMsg_UpdateRect::ID));
  ASSERT_TRUE(msg);
  ViewHostMsg_UpdateRect::Schema::Param update_rect_params;
  EXPECT_TRUE(ViewHostMsg_UpdateRect::Read(msg, &update_rect_params));
  EXPECT_TRUE(ViewHostMsg_UpdateRect_Flags::is_resize_ack(
      update_rect_params.a.flags));
  EXPECT_EQ(size,
      update_rect_params.a.view_size);
  render_thread_->sink().ClearMessages();

  // Setting the same size again should not send the ack.
  widget->OnResize(resize_params);
  EXPECT_FALSE(widget->next_paint_is_resize_ack());

  // Resetting the rect to empty should not send the ack.
  resize_params.new_size = gfx::Size();
  resize_params.physical_backing_size = gfx::Size();
  widget->OnResize(resize_params);
  EXPECT_FALSE(widget->next_paint_is_resize_ack());
}

}  // namespace content
