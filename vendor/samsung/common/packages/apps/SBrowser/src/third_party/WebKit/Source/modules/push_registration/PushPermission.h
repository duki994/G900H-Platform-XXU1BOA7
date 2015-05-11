// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPermission_h
#define PushPermission_h

#if ENABLE(PUSH_API)

#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "heap/Handle.h"
#include "public/platform/WebPushPermission.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class PushPermission {
public:
    typedef blink::WebPushPermission WebType;

    static ScriptValue from(ScriptPromiseResolver* resolver, WebType* webPermissionRaw)
    {
        OwnPtr<WebType> webPermission = adoptPtr(webPermissionRaw);
        v8::Isolate* isolate = resolver->promise().isolate();
        return ScriptValue(v8AtomicString(isolate, permissionString(webPermission->type).utf8().data()), isolate);
    }

    enum PermissionType {
        Default,
        InProgress,
        Granted,
        Denied
    };

    static String permissionString(PermissionType);

private:
    static String permissionString(blink::WebPushPermission::PermissionType);

    PushPermission() WTF_DELETED_FUNCTION;
};

} // namespace WebCore

#endif // ENABLE(PUSH_API)

#endif // PushPermission_h
