// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushIsRegistered_h
#define PushIsRegistered_h

#if ENABLE(PUSH_API)

#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMError.h"
#include "heap/Handle.h"
#include "public/platform/WebPushIsRegistered.h"

namespace WebCore {

class PushIsRegistered {
public:
    typedef blink::WebPushIsRegistered WebType;

    static ScriptValue from(ScriptPromiseResolver* resolver, WebType* webErrorRaw)
    {
        ASSERT(!promise);
        v8::Isolate* isolate = resolver->promise().isolate();
        return ScriptValue(v8Boolean(webErrorRaw->isRegistered, isolate), isolate);
    }

private:
    PushIsRegistered(const bool isRegistered) { m_isRegistered = isRegistered; };

    bool m_isRegistered;
};

} // namespace WebCore

#endif // ENABLE(PUSH_API)

#endif // PushIsRegistered_h
