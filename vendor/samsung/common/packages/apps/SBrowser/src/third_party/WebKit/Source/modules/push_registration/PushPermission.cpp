// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(PUSH_API)

#include "modules/push_registration/PushPermission.h"

namespace WebCore {

String PushPermission::permissionString(blink::WebPushPermission::PermissionType type)
{
    switch (type) {
    case blink::WebPushPermission::Granted:
        return "Granted";
    case blink::WebPushPermission::Denied:
        return "Denied";
    case blink::WebPushPermission::Default:
        return "Default";
    case blink::WebPushPermission::InProgress:
        return "InProgress";
    }
    ASSERT_NOT_REACHED();
    return String();
}

String PushPermission::permissionString(PushPermission::PermissionType type)
{
    return permissionString(static_cast<blink::WebPushPermission::PermissionType>(type));
}

} // namespace WebCore

#endif // ENABLE(PUSH_API)
