// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_REGISTRATION_CHROME_PUSH_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PUSH_REGISTRATION_CHROME_PUSH_PERMISSION_CONTEXT_H_

#if defined(ENABLE_PUSH_API)

#include <string>
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "content/public/browser/push_permission_context.h"

namespace content {
class WebContents;
}

class PermissionRequestID;
class Profile;

class ChromePushPermissionContext
    : public content::PushPermissionContext {
 public:
  explicit ChromePushPermissionContext(Profile* profile);

  virtual void RequestPushPermission(
      int render_process_id,
      int routing_id,
      int callback_id,
      const GURL& origin,
      base::Callback<void(bool)> callback) OVERRIDE;

  void ShutdownOnUIThread();

  void NotifyPermissionSet(
      const PermissionRequestID& id,
      const GURL& origin,
      base::Callback<void(bool)> callback,
      bool allowed);

 protected:
  virtual ~ChromePushPermissionContext();

  Profile* profile() const { return profile_; }

  PermissionQueueController* QueueController();

  virtual void DecidePermission(
      const PermissionRequestID& id,
      const GURL& origin,
      base::Callback<void(bool)> callback);

  virtual void PermissionDecided(
    const PermissionRequestID& id,
    const GURL& origin,
    const GURL& embedder,
    base::Callback<void(bool)> callback,
    bool allowed);

  virtual PermissionQueueController* CreateQueueController();

 private:
  void CancelPendingInfobarRequest(const PermissionRequestID& id);

  Profile* const profile_;
  bool shutting_down_;
  scoped_ptr<PermissionQueueController> permission_queue_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromePushPermissionContext);
};

#endif  // defined(ENABLE_PUSH_API)

#endif  // CHROME_BROWSER_PUSH_REGISTRATION_CHROME_PUSH_PERMISSION_CONTEXT_H_
