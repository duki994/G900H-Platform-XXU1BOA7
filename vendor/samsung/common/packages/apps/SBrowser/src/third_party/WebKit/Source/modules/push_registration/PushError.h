// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushError_h
#define PushError_h

#if ENABLE(PUSH_API)

#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMError.h"
#include "heap/Handle.h"
#include "public/platform/WebPushError.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class PushError {
public:
    typedef blink::WebPushError WebType;
    static PassRefPtrWillBeRawPtr<DOMError> from(WebType* webErrorRaw)
    {
        OwnPtr<WebType> webError = adoptPtr(webErrorRaw);
        RefPtrWillBeRawPtr<DOMError> error = DOMError::create(errorString(webError->errorType), webError->message);
        return error.release();
    }

    static PassRefPtrWillBeRawPtr<DOMError> from(ScriptPromiseResolver* resolver, WebType* webErrorRaw)
    {
        OwnPtr<WebType> webError = adoptPtr(webErrorRaw);
        RefPtrWillBeRawPtr<DOMError> error = DOMError::create(errorString(webError->errorType), webError->message);
        return error.release();
    }

    static String errorString(blink::WebPushError::ErrorType);

private:
    PushError() WTF_DELETED_FUNCTION;
};

} // namespace WebCore

#endif // ENABLE(PUSH_API)

#endif // PushError_h
