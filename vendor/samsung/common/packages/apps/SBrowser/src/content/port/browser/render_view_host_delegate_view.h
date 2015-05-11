// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
#define CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "base/files/file_path.h"

class SkBitmap;

namespace gfx {
class ImageSkia;
class Rect;
class Vector2d;
}

namespace content {
class RenderFrameHost;
struct ContextMenuParams;
struct DropData;
struct MenuItem;

// This class provides a way for the RenderViewHost to reach out to its
// delegate's view. It only needs to be implemented by embedders if they don't
// use the default WebContentsView implementations.
class CONTENT_EXPORT RenderViewHostDelegateView {
 public:
  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) {}

  // Shows a popup menu with the specified items.
  // This method should call RenderViewHost::DidSelectPopupMenuItem[s]() or
  // RenderViewHost::DidCancelPopupMenu() based on the user action.

  //SBROWSER_FORM_NAVIGATION
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<MenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection,
                             int advanced_ime_options = 0) = 0;

  // The user started dragging content of the specified type within the
  // RenderView. Contextual information about the dragged content is supplied
  // by DropData. If the delegate's view cannot start the drag for /any/
  // reason, it must inform the renderer that the drag has ended; otherwise,
  // this results in bugs like http://crbug.com/157134.
  virtual void StartDragging(const DropData& drop_data,
                             blink::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) {}

  // The page wants to update the mouse cursor during a drag & drop operation.
  // |operation| describes the current operation (none, move, copy, link.)
  virtual void UpdateDragCursor(blink::WebDragOperation operation) {}

  // Notification that view for this delegate got the focus.
  virtual void GotFocus() {}

  // Callback to inform the browser that the page is returning the focus to
  // the browser's chrome. If reverse is true, it means the focus was
  // retrieved by doing a Shift-Tab.
  virtual void TakeFocus(bool reverse) {}

#if defined(S_NOTIFY_ROTATE_STATUS)
  virtual void NotifyRotateStatus() {}
#endif

  virtual void SelectedMarkup(const base::string16& markup) {}
  virtual void SetSelectionVisibility(bool isVisible) {}
  virtual void UpdateSelectionRect(const gfx::Rect& selectionRect) {}
  virtual void PointOnRegion(bool isOnRegion) {}
  virtual void SelectedBitmap(const SkBitmap& bitmap) {}

  virtual void SavePageFileName(
      const base::FilePath::StringType& pure_file_name) {}

  virtual void OnReceiveBitmapFromCache(const SkBitmap& bitmap) {}

  virtual void OnOpenUrlInNewTab(const base::string16& url) {}
  //Sent to browser for setting last touch point for long press enter key : start
  virtual void SetLongPressSelectionPoint(int x, int y) {}
  //Sent to browser for setting last touch point for long press enter key : end
  virtual void UpdateTouchedFixedElementHeight(int height) {}
#if defined(S_SCROLL_EVENT)
  virtual void OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect) {}
#endif
#if defined(S_INTUITIVE_HOVER)
  virtual void OnHoverHitTestResult(int contentType) {}
#endif
#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
  virtual void ShowAutoLoginFailureMsg() {}
#endif

 protected:
  virtual ~RenderViewHostDelegateView() {}
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
