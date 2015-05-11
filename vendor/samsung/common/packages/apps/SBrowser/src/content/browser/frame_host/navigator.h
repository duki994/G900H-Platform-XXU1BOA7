// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"

class GURL;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;

namespace base {
class TimeTicks;
}

namespace content {

class NavigationControllerImpl;
class NavigationEntryImpl;
class NavigatorDelegate;
class RenderFrameHostImpl;

// Implementations of this interface are responsible for performing navigations
// in a node of the FrameTree. Its lifetime is bound to all FrameTreeNode
// objects that are using it and will be released once all nodes that use it are
// freed. The Navigator is bound to a single frame tree and cannot be used by
// multiple instances of FrameTree.
// TODO(nasko): Move all navigation methods, such as didStartProvisionalLoad
// from WebContentsImpl to this interface.
class CONTENT_EXPORT Navigator : public base::RefCounted<Navigator> {
 public:
  // The RenderFrameHostImpl started a provisional load.
  virtual void DidStartProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                                       int64 frame_id,
                                       int64 parent_frame_id,
                                       bool main_frame,
                                       const GURL& url) {};

  // The RenderFrameHostImpl has failed a provisional load.
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {};

  // The RenderFrameHostImpl has failed to load the document.
  virtual void DidFailLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      int64 frame_id,
      const GURL& url,
      bool is_main_frame,
      int error_code,
      const base::string16& error_description) {}

  // The RenderFrameHostImpl processed a redirect during a provisional load.
  //
  // TODO(creis): Remove this method and have the pre-rendering code listen to
  // WebContentsObserver::DidGetRedirectForResourceRequest instead.
  // See http://crbug.com/78512.
  virtual void DidRedirectProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      int32 page_id,
      const GURL& source_url,
      const GURL& target_url) {}

  // The RenderFrameHostImpl has committed a navigation.
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {}

  // Causes the Navigator to navigate in the right render frame to |entry|,
  // which must be already part of the entries in the navigation controller.
  // This does not change the NavigationController state.
  virtual bool NavigateToEntry(
      RenderFrameHostImpl* render_frame_host,
      const NavigationEntryImpl& entry,
      NavigationController::ReloadType reload_type);

  // Called by the NavigationController to cause the Navigator to navigate
  // to the current pending entry. The NavigationController should be called
  // back with RendererDidNavigate on success or DiscardPendingEntry on failure.
  // The callbacks can be inside of this function, or at some future time.
  //
  // The entry has a PageID of -1 if newly created (corresponding to navigation
  // to a new URL).
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  //
  // TODO(nasko): Remove this method from the interface, since Navigator and
  // NavigationController know about each other. This will be possible once
  // initialization of Navigator and NavigationController is properly done.
  virtual bool NavigateToPendingEntry(
      RenderFrameHostImpl* render_frame_host,
      NavigationController::ReloadType reload_type);

  virtual base::TimeTicks GetCurrentLoadStart();

 protected:
  friend class base::RefCounted<Navigator>;
  virtual ~Navigator() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
