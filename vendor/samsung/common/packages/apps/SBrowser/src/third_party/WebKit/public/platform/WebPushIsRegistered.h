// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushIsRegistered_h
#define WebPushIsRegistered_h

#if defined(ENABLE_PUSH_API)

namespace blink {

struct WebPushIsRegistered {
    WebPushIsRegistered(bool isRegistered)
        : isRegistered(isRegistered)
    {
    }

    bool isRegistered;
};

} // namespace blink

#endif // defined(ENABLE_PUSH_API)

#endif // WebPushIsRegistered_h
