// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushPermissionRequestManager_h
#define WebPushPermissionRequestManager_h

#if defined(ENABLE_PUSH_API)

#include "../platform/WebNonCopyable.h"
#include "../platform/WebPrivateOwnPtr.h"

namespace blink {

class WebPushPermissionRequest;
class WebPushPermissionRequestManagerPrivate;

class WebPushPermissionRequestManager : public WebNonCopyable {
public:
    WebPushPermissionRequestManager() { init(); }
    ~WebPushPermissionRequestManager() { reset(); }

    BLINK_EXPORT int add(const blink::WebPushPermissionRequest&);
    BLINK_EXPORT bool remove(const blink::WebPushPermissionRequest&, int&);
    BLINK_EXPORT bool remove(int, blink::WebPushPermissionRequest&);

private:
    BLINK_EXPORT void init();
    BLINK_EXPORT void reset();

    WebPrivateOwnPtr<WebPushPermissionRequestManagerPrivate> m_private;
    int m_lastId;
};

}

#endif // defined(ENABLE_PUSH_API)

#endif // WebPushPermissionRequestManager_h

