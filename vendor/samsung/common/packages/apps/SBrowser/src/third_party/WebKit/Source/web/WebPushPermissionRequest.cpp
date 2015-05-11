// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(PUSH_API)

#include "WebPushPermissionRequest.h"

#include "modules/push_registration/PushRegistrationManager.h"

using namespace WebCore;

namespace blink {

void WebPushPermissionRequest::setIsAllowed(bool allowed)
{
    m_private->setIsAllowed(allowed);
}

}

#endif // ENABLE(PUSH_API)
