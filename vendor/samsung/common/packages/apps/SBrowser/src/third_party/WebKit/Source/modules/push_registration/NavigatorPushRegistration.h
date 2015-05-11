// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorPushRegistration_h
#define NavigatorPushRegistration_h

#if ENABLE(PUSH_API)

#include "platform/Supplementable.h"
#include "heap/Handle.h"

namespace WebCore {

class Navigator;
class PushRegistrationManager;

class NavigatorPushRegistration FINAL : public NoBaseWillBeGarbageCollectedFinalized<NavigatorPushRegistration>, public Supplement<Navigator> {
public:
    virtual ~NavigatorPushRegistration();
    static NavigatorPushRegistration& from(Navigator*);
    static PushRegistrationManager* pushRegistrationManager(Navigator*);

    void trace(Visitor*);

private:
    NavigatorPushRegistration();
    static const char* supplementName();

    PushRegistrationManager* pushManager(Navigator*);
    RefPtrWillBePersistent<PushRegistrationManager> m_pushRegistrationManager;
};

} // namespace WebCore

#endif // ENABLE(PUSH_API)

#endif // NavigatorPushRegistration_h
