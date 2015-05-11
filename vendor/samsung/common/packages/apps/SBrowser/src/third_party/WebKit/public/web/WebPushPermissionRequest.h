// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushRegistrationPermissionRequest_h
#define WebPushRegistrationPermissionRequest_h

#if defined(ENABLE_PUSH_API)

#include "../platform/WebCommon.h"
#include "../platform/WebPrivatePtr.h"

namespace WebCore {
class PushRegistrationManager;
}

namespace blink {
class WebSecurityOrigin;

class WebPushPermissionRequest {
public:
    BLINK_EXPORT void setIsAllowed(bool);

#if BLINK_IMPLEMENTATION
    WebPushPermissionRequest(WebCore::PushRegistrationManager* manager)
        : m_private(manager)
    {
    }

    WebCore::PushRegistrationManager* manager() const { return m_private; }
#endif

private:
    WebCore::PushRegistrationManager* m_private;
};
}

#endif // defined(ENABLE_PUSH_API)

#endif // WebPushRegistrationPermissionRequest_h
