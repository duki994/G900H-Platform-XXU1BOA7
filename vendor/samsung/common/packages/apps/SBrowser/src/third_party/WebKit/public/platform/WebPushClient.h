// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushClient_h
#define WebPushClient_h

#if defined(ENABLE_PUSH_API)

#include "public/platform/WebCallbacks.h"

namespace blink {

class WebPushError;
class WebSecurityOrigin;
class WebPushPermissionRequest;

struct WebPushIsRegistered;
struct WebPushRegistration;
struct WebPushPermission;

typedef WebCallbacks<WebPushRegistration, WebPushError> WebPushRegistrationCallbacks;
typedef WebCallbacks<WebPushIsRegistered, WebPushError> WebPushUnregistrationCallbacks;
typedef WebCallbacks<WebPushIsRegistered, WebPushError> WebPushIsRegisteredCallbacks;
typedef WebCallbacks<WebPushPermission, WebPushError> WebPushHasPermissionCallbacks;

class WebPushClient {
public:
    virtual void registerPush(const WebSecurityOrigin& origin, WebPushRegistrationCallbacks*) = 0;
    virtual void unregisterPush(const WebSecurityOrigin& origin, WebPushUnregistrationCallbacks*) = 0;
    virtual void isRegisteredPush(const WebSecurityOrigin& origin, WebPushIsRegisteredCallbacks*) = 0;
    virtual void hasPermissionPush(const WebSecurityOrigin& origin, WebPushHasPermissionCallbacks*) = 0;

    virtual void requestPermission(const WebSecurityOrigin& origin, const WebPushPermissionRequest&) = 0;
};

} // namespace blink

#endif // defined(ENABLE_PUSH_API)

#endif // WebPushClient_h
