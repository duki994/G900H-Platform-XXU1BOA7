// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/web_contents_view_port.h"

namespace content {

class WebContents;
class WebContentsImpl;
class BrowserPluginGuest;

class CONTENT_EXPORT WebContentsViewGuest
    : public WebContentsViewPort,
      public RenderViewHostDelegateView {
 public:
  // The corresponding WebContentsImpl is passed in the constructor, and manages
  // our lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  // WebContentsViewGuest always has a backing platform dependent view,
  // |platform_view|.
  WebContentsViewGuest(WebContentsImpl* web_contents,
                       BrowserPluginGuest* guest,
                       scoped_ptr<WebContentsViewPort> platform_view,
                       RenderViewHostDelegateView* platform_view_delegate_view);
  virtual ~WebContentsViewGuest();

  WebContents* web_contents();

  void OnGuestInitialized(WebContentsView* parent_view);

  // WebContentsView implementation --------------------------------------------

  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual DropData* GetDropData() const OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  void SavePageFileName(
      const base::FilePath::StringType& pure_file_name) OVERRIDE;
  virtual void OnReceiveBitmapFromCache(const SkBitmap& bitmap) OVERRIDE;
#if defined(S_SCROLL_EVENT)
  virtual void OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect)OVERRIDE;
#endif
#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
  virtual void ShowAutoLoginFailureMsg() OVERRIDE;
#endif


#if defined(OS_MACOSX)
  virtual void SetAllowOverlappingViews(bool overlapping) OVERRIDE;
  virtual bool GetAllowOverlappingViews() const OVERRIDE;
  virtual void SetOverlayView(WebContentsView* overlay,
                              const gfx::Point& offset) OVERRIDE;
  virtual void RemoveOverlayView() OVERRIDE;
#endif

  // WebContentsViewPort implementation ----------------------------------------
  virtual void CreateView(const gfx::Size& initial_size,
                          gfx::NativeView context) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual void SetPageTitle(const base::string16& title) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void RenderViewSwappedIn(RenderViewHost* host) OVERRIDE;
  virtual void SetOverscrollControllerEnabled(bool enabled) OVERRIDE;
#if defined(OS_MACOSX)
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
#endif

  // Backend implementation of RenderViewHostDelegateView.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) OVERRIDE;

  //SBROWSER_FORM_NAVIGATION
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<MenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection,
                             int advanced_ime_options = 0) OVERRIDE;
  virtual void StartDragging(const DropData& drop_data,
                             blink::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) OVERRIDE;
  virtual void UpdateDragCursor(blink::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

  virtual void SelectedMarkup(const base::string16& markup) OVERRIDE;
  virtual void SetSelectionVisibility(bool isVisible) OVERRIDE;
  virtual void UpdateSelectionRect(const gfx::Rect& selectionRect) OVERRIDE;
  virtual void PointOnRegion(bool isOnRegion) OVERRIDE;
  virtual void SelectedBitmap(const SkBitmap& bitmap) OVERRIDE;
  virtual void OnOpenUrlInNewTab(const base::string16& url) OVERRIDE;
  virtual void SetLongPressSelectionPoint(int x, int y) OVERRIDE;
  virtual void UpdateTouchedFixedElementHeight(int height) OVERRIDE;
#if defined(S_INTUITIVE_HOVER)
  virtual void OnHoverHitTestResult(int contentType) OVERRIDE;
#endif

 private:
  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;
  BrowserPluginGuest* guest_;
  // The platform dependent view backing this WebContentsView.
  // Calls to this WebContentsViewGuest are forwarded to |platform_view_|.
  scoped_ptr<WebContentsViewPort> platform_view_;
  gfx::Size size_;

  // Delegate view for guest's platform view.
  RenderViewHostDelegateView* platform_view_delegate_view_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
