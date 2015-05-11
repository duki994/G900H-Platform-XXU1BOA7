// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/common/content_export.h"

namespace content {

class NavigationControllerImpl;
class NavigatorDelegate;
struct LoadCommittedDetails;

// This class is an implementation of Navigator, responsible for managing
// navigations in regular browser tabs.
class CONTENT_EXPORT NavigatorImpl : public Navigator {
 public:
  NavigatorImpl(NavigationControllerImpl* navigation_controller,
                NavigatorDelegate* delegate);

  // Navigator implementation.
  virtual void DidStartProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                                       int64 frame_id,
                                       int64 parent_frame_id,
                                       bool main_frame,
                                       const GURL& url) OVERRIDE;
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params)
      OVERRIDE;
  virtual void DidFailLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      int64 frame_id,
      const GURL& url,
      bool is_main_frame,
      int error_code,
      const base::string16& error_description) OVERRIDE;
  virtual void DidRedirectProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      int32 page_id,
      const GURL& source_url,
      const GURL& target_url) OVERRIDE;
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params&
          input_params) OVERRIDE;
  virtual bool NavigateToEntry(
      RenderFrameHostImpl* render_frame_host,
      const NavigationEntryImpl& entry,
      NavigationController::ReloadType reload_type) OVERRIDE;
  virtual bool NavigateToPendingEntry(
      RenderFrameHostImpl* render_frame_host,
      NavigationController::ReloadType reload_type) OVERRIDE;
  virtual base::TimeTicks GetCurrentLoadStart() OVERRIDE;

 private:
  virtual ~NavigatorImpl() {}

  bool ShouldAssignSiteForURL(const GURL& url);

  // The NavigationController that will keep track of session history for all
  // RenderFrameHost objects using this NavigatorImpl.
  // TODO(nasko): Move ownership of the NavigationController from
  // WebContentsImpl to this class.
  NavigationControllerImpl* controller_;

  // Used to notify the object embedding this Navigator about navigation
  // events. Can be NULL in tests.
  NavigatorDelegate* delegate_;

  // System time at which the current load was started.
  base::TimeTicks current_load_start_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
