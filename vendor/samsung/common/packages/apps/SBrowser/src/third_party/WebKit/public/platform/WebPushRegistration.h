// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushRegistration_h
#define WebPushRegistration_h

#if defined(ENABLE_PUSH_API)

#include "WebString.h"

namespace blink {

struct WebPushRegistration {
    WebPushRegistration(const WebString& endpoint, const WebString& registrationId)
        : pushEndpoint(endpoint)
        , pushRegistrationId(registrationId)
    {
    }

    WebString pushEndpoint;
    WebString pushRegistrationId;
};

} // namespace blink

#endif // defined(ENABLE_PUSH_API)

#endif // WebPushRegistration_h
