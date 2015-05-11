// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_PUSH_API)

#include "content/renderer/push_registration/push_registration_dispatcher.h"

#include "base/logging.h"
#include "content/common/push_registration/push_registration_messages.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebPushError.h"
#include "third_party/WebKit/public/platform/WebPushIsRegistered.h"
#include "third_party/WebKit/public/platform/WebPushPermission.h"
#include "third_party/WebKit/public/platform/WebPushRegistration.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebPushPermissionRequest.h"
#include "third_party/WebKit/public/web/WebPushPermissionRequestManager.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "url/gurl.h"

using blink::WebPushPermissionRequestManager;
using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

PushRegistrationDispatcher::PushRegistrationDispatcher(RenderViewImpl* render_view)
    : RenderViewObserver(render_view)
    , pending_permissions_(new WebPushPermissionRequestManager()) {}

PushRegistrationDispatcher::~PushRegistrationDispatcher() {}

bool PushRegistrationDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushRegistrationDispatcher, message)
  IPC_MESSAGE_HANDLER(PushRegistrationMsg_RegisterSuccess, OnRegisterSuccess)
  IPC_MESSAGE_HANDLER(PushRegistrationMsg_RegisterError, OnRegisterError)

  IPC_MESSAGE_HANDLER(PushRegistrationMsg_UnregisterSuccess, OnUnregisterSuccess)
  IPC_MESSAGE_HANDLER(PushRegistrationMsg_UnregisterError, OnUnregisterError)

  IPC_MESSAGE_HANDLER(PushRegistrationMsg_IsRegisteredSuccess, OnIsRegisteredSuccess)
  IPC_MESSAGE_HANDLER(PushRegistrationMsg_IsRegisteredError, OnIsRegisteredError)

  IPC_MESSAGE_HANDLER(PushRegistrationMsg_HasPermissionSuccess, OnHasPermissionSuccess)
  IPC_MESSAGE_HANDLER(PushRegistrationMsg_HasPermissionError, OnHasPermissionError)

  IPC_MESSAGE_HANDLER(PushRegistrationMsg_PermissionSet, OnPermissionSet)

  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushRegistrationDispatcher::registerPush(
    const WebSecurityOrigin& origin,
    blink::WebPushRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);
  int callbacks_id = registration_callbacks_.Add(callbacks);
  Send(new PushRegistrationHostMsg_Register(
       routing_id(), callbacks_id, GURL(origin.toString()).GetOrigin()));
}

void PushRegistrationDispatcher::unregisterPush(
    const WebSecurityOrigin& origin,
    blink::WebPushUnregistrationCallbacks* callbacks) {
  DCHECK(callbacks);
  int callbacks_id = unregistration_callbacks_.Add(callbacks);
  Send(new PushRegistrationHostMsg_Unregister(
       routing_id(), callbacks_id, GURL(origin.toString()).GetOrigin()));
}

void PushRegistrationDispatcher::isRegisteredPush(
    const WebSecurityOrigin& origin,
    blink::WebPushIsRegisteredCallbacks* callbacks) {
  DCHECK(callbacks);
  int callbacks_id = isregistered_callbacks_.Add(callbacks);
  Send(new PushRegistrationHostMsg_IsRegistered(
       routing_id(), callbacks_id, GURL(origin.toString()).GetOrigin()));
}

void PushRegistrationDispatcher::hasPermissionPush(
    const WebSecurityOrigin& origin,
    blink::WebPushHasPermissionCallbacks* callbacks) {
  DCHECK(callbacks);
  int callbacks_id = haspermission_callbacks_.Add(callbacks);
  Send(new PushRegistrationHostMsg_HasPermission(
       routing_id(), callbacks_id, GURL(origin.toString()).GetOrigin()));
}

void PushRegistrationDispatcher::requestPermission(
    const WebSecurityOrigin& origin,
    const blink::WebPushPermissionRequest& request) {
  int callbacks_id = pending_permissions_->add(request);
  Send(new PushRegistrationHostMsg_RequestPermission(
       routing_id(), callbacks_id, GURL(origin.toString()).GetOrigin()));
}

// For Register
void PushRegistrationDispatcher::OnRegisterSuccess(
    int32 callbacks_id,
    const base::string16& endpoint,
    const base::string16& registration_id) {
    blink::WebPushRegistrationCallbacks* callbacks =
    registration_callbacks_.Lookup(callbacks_id);
    CHECK(callbacks);

    scoped_ptr<blink::WebPushRegistration> registration(
        new blink::WebPushRegistration(
            WebString(endpoint),
            WebString(registration_id)));

    callbacks->onSuccess(registration.release());
    registration_callbacks_.Remove(callbacks_id);
}

void PushRegistrationDispatcher::OnRegisterError(int32 callbacks_id) {
    const std::string kAbortErrorReason = "Registration failed.";
    blink::WebPushRegistrationCallbacks* callbacks =
            registration_callbacks_.Lookup(callbacks_id);

    CHECK(callbacks);

    scoped_ptr<blink::WebPushError> error(
        new blink::WebPushError(
            blink::WebPushError::ErrorTypeAbort,
            WebString::fromUTF8(kAbortErrorReason)));

    callbacks->onError(error.release());
    registration_callbacks_.Remove(callbacks_id);
}

// For Unregister
void PushRegistrationDispatcher::OnUnregisterSuccess(
    int32 callbacks_id) {
    blink::WebPushUnregistrationCallbacks* callbacks =
    unregistration_callbacks_.Lookup(callbacks_id);
    CHECK(callbacks);

    scoped_ptr<blink::WebPushIsRegistered> unregistration(
        new blink::WebPushIsRegistered(true));

    callbacks->onSuccess(unregistration.release());
    unregistration_callbacks_.Remove(callbacks_id);
}

void PushRegistrationDispatcher::OnUnregisterError(int32 callbacks_id) {
    const std::string kAbortErrorReason = "Unregistration failed.";
    blink::WebPushUnregistrationCallbacks* callbacks =
        unregistration_callbacks_.Lookup(callbacks_id);
    CHECK(callbacks);

    scoped_ptr<blink::WebPushError> error(
        new blink::WebPushError(
            blink::WebPushError::ErrorTypeAbort,
            WebString::fromUTF8(kAbortErrorReason)));

    callbacks->onError(error.release());
    unregistration_callbacks_.Remove(callbacks_id);
}

// For isRegistered
void PushRegistrationDispatcher::OnIsRegisteredSuccess(
    int32 callbacks_id,
    bool is_registered) {
  blink::WebPushIsRegisteredCallbacks* callbacks =
  isregistered_callbacks_.Lookup(callbacks_id);
  CHECK(callbacks);

  scoped_ptr<blink::WebPushIsRegistered> registered(
      new blink::WebPushIsRegistered(is_registered));

  callbacks->onSuccess(registered.release());
  isregistered_callbacks_.Remove(callbacks_id);
}

void PushRegistrationDispatcher::OnIsRegisteredError(int32 callbacks_id) {
  const std::string kAbortErrorReason = "isRegistered failed.";
  blink::WebPushIsRegisteredCallbacks* callbacks =
          isregistered_callbacks_.Lookup(callbacks_id);

  CHECK(callbacks);

  scoped_ptr<blink::WebPushError> error(
      new blink::WebPushError(
          blink::WebPushError::ErrorTypeAbort,
          WebString::fromUTF8(kAbortErrorReason)));

  callbacks->onError(error.release());
  isregistered_callbacks_.Remove(callbacks_id);
}

void PushRegistrationDispatcher::OnHasPermissionSuccess(
    int32 callbacks_id,
    bool is_registered) {
  blink::WebPushHasPermissionCallbacks* callbacks =
  haspermission_callbacks_.Lookup(callbacks_id);

  CHECK(callbacks);

  blink::WebPushPermission::PermissionType permission =
      is_registered ? blink::WebPushPermission::Granted : blink::WebPushPermission::Default;

  scoped_ptr<blink::WebPushPermission> has_permission(
      new blink::WebPushPermission(permission));

  callbacks->onSuccess(has_permission.release());
  haspermission_callbacks_.Remove(callbacks_id);
}

void PushRegistrationDispatcher::OnHasPermissionError(int32 callbacks_id) {
  const std::string kAbortErrorReason = "Has Permission failed.";
  blink::WebPushHasPermissionCallbacks* callbacks =
      haspermission_callbacks_.Lookup(callbacks_id);

  CHECK(callbacks);

  scoped_ptr<blink::WebPushError> error(
      new blink::WebPushError(
          blink::WebPushError::ErrorTypeAbort,
          WebString::fromUTF8(kAbortErrorReason)));

  callbacks->onError(error.release());
  haspermission_callbacks_.Remove(callbacks_id);
}

void PushRegistrationDispatcher::OnPermissionSet(int32 callbacks_id, bool is_allowed) {
  blink::WebPushPermissionRequest request;
  if (!pending_permissions_->remove(callbacks_id, request))
    return;

  request.setIsAllowed(is_allowed);
}

} // namespace content

#endif // defined(ENABLE_PUSH_API)

