// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushRegistration_h
#define PushRegistration_h

#if ENABLE(PUSH_API)

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "public/platform/WebPushRegistration.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class PushRegistration FINAL : public RefCountedWillBeGarbageCollectedFinalized<PushRegistration>, public ScriptWrappable {
public:
    typedef blink::WebPushRegistration WebType;
    static PushRegistration* from(WebType* registrationRaw)
    {
        OwnPtr<WebType> registration = adoptPtr(registrationRaw);
        return new PushRegistration(registration->pushEndpoint, registration->pushRegistrationId);
    }

    static PassRefPtrWillBeRawPtr<PushRegistration> create(const String& pushEndpoint, const String& pushRegistrationId)
    {
        return adoptRefWillBeNoop(new PushRegistration(pushEndpoint, pushRegistrationId));
    }

    const String pushEndpoint() const { return m_pushEndpoint; }
    const String pushRegistrationId() const { return m_pushRegistrationId; }

    void setPushEndPoint(const String&);
    void setPushRegistrationId(const String&);
    ~PushRegistration();

    void trace(Visitor*) { }

private:
    PushRegistration(const String&, const String&);

    String m_pushEndpoint;
    String m_pushRegistrationId;
};

} // namespace WebCore

#endif // ENABLE(PUSH_API)

#endif // PushRegistration_h
