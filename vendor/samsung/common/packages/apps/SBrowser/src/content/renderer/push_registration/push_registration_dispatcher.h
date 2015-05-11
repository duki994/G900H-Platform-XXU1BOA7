// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_REGISTRATION_PUSH_REGISTRATION_DISPATCHER_H_
#define CONTENT_RENDERER_PUSH_REGISTRATION_PUSH_REGISTRATION_DISPATCHER_H_

#if defined(ENABLE_PUSH_API)

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/platform/WebPushClient.h"

class GURL;

namespace IPC {
class Message;
} // namespace IPC

namespace blink {
class WebPushPermissionRequest;
class WebPushPermissionRequestManager;
class WebSecurityOrigin;
} // namespace blink

namespace content {
class RenderViewImpl;

class PushRegistrationDispatcher : public RenderViewObserver,
                                   public blink::WebPushClient {
 public:
  explicit PushRegistrationDispatcher(RenderViewImpl* render_view);
  virtual ~PushRegistrationDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebPushClient implementation.
  virtual void registerPush(
      const blink::WebSecurityOrigin& origin,
      blink::WebPushRegistrationCallbacks* callbacks) OVERRIDE;

  virtual void unregisterPush(
      const blink::WebSecurityOrigin& origin,
      blink::WebPushUnregistrationCallbacks* callbacks) OVERRIDE;

  virtual void isRegisteredPush(
      const blink::WebSecurityOrigin& origin,
      blink::WebPushIsRegisteredCallbacks* callbacks) OVERRIDE;

  virtual void hasPermissionPush(
      const blink::WebSecurityOrigin& origin,
      blink::WebPushHasPermissionCallbacks* callbacks) OVERRIDE;

  virtual void requestPermission(
      const blink::WebSecurityOrigin& origin,
      const blink::WebPushPermissionRequest&) OVERRIDE;

  void OnRegisterSuccess(
      int32 callbacks_id,
      const base::string16& endpoint,
      const base::string16& registration_id);

  void OnRegisterError(int32 callbacks_id);

  void OnUnregisterSuccess(int32 callbacks_id);
  void OnUnregisterError(int32 callbacks_id);

  void OnIsRegisteredSuccess(
      int32 callbacks_id,
      bool is_registered);
  void OnIsRegisteredError(int32 callbacks_id);

  void OnHasPermissionSuccess(
      int32 callbacks_id,
      bool is_registered);
  void OnHasPermissionError(int32 callbacks_id);

  void OnPermissionSet(int32 callbacks_id, bool is_allowed);

  IDMap<blink::WebPushRegistrationCallbacks, IDMapOwnPointer> registration_callbacks_;
  IDMap<blink::WebPushUnregistrationCallbacks, IDMapOwnPointer> unregistration_callbacks_;
  IDMap<blink::WebPushIsRegisteredCallbacks, IDMapOwnPointer> isregistered_callbacks_;
  IDMap<blink::WebPushHasPermissionCallbacks, IDMapOwnPointer> haspermission_callbacks_;
  scoped_ptr<blink::WebPushPermissionRequestManager> pending_permissions_;

  DISALLOW_COPY_AND_ASSIGN(PushRegistrationDispatcher);
};

} // namespace content

#endif // defined(ENABLE_PUSH_API)

#endif // CONTENT_RENDERER_PUSH_REGISTRATION_PUSH_REGISTRATION_DISPATCHER_H_
