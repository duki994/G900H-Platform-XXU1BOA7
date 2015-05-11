// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushPermission_h
#define WebPushPermission_h

#if defined(ENABLE_PUSH_API)

namespace blink {

struct WebPushPermission {
    typedef enum {
        Default,
        InProgress,
        Granted,
        Denied
    } PermissionType;

    WebPushPermission(PermissionType permissionType)
        : type(permissionType)
    {
    }

    PermissionType type;
};

} // namespace blink

#endif // defined(ENABLE_PUSH_API)

#endif // WebPushPermission_h
