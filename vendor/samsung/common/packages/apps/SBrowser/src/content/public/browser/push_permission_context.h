// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_PUSH_PERMISSION_CONTEXT_H_

#if defined(ENABLE_PUSH_API)

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class CONTENT_EXPORT PushPermissionContext
    : public base::RefCountedThreadSafe<PushPermissionContext> {
 public:
  virtual void RequestPushPermission(
      int render_process_id,
      int routing_id,
      int callback_id,
      const GURL& origin,
      base::Callback<void(bool)> callback) = 0;

 protected:
   virtual ~PushPermissionContext() {}

 private:
  friend class base::RefCountedThreadSafe<PushPermissionContext>;
};

}  // namespace content

#endif  // defined(ENABLE_PUSH_API)

#endif  // CONTENT_BROWSER_PUSH_PERMISSION_CONTEXT_H_
