// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(PUSH_API)

#include "modules/push_registration/NavigatorPushRegistration.h"

#include "core/dom/Document.h"
#include "core/frame/Navigator.h"
#include "modules/push_registration/PushRegistrationManager.h"

namespace WebCore {

NavigatorPushRegistration::NavigatorPushRegistration()
{
}

NavigatorPushRegistration::~NavigatorPushRegistration()
{
}

const char* NavigatorPushRegistration::supplementName()
{
    return "NavigatorPushRegistration";
}

NavigatorPushRegistration& NavigatorPushRegistration::from(Navigator* navigator)
{
    NavigatorPushRegistration* supplement = static_cast<NavigatorPushRegistration*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorPushRegistration();
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

PushRegistrationManager* NavigatorPushRegistration::pushRegistrationManager(Navigator* navigator)
{
    return NavigatorPushRegistration::from(navigator).pushManager(navigator);
}

PushRegistrationManager* NavigatorPushRegistration::pushManager(Navigator* navigator)
{
    if (!m_pushRegistrationManager)
        m_pushRegistrationManager = PushRegistrationManager::create(navigator);
    return m_pushRegistrationManager.get();
}

} // namespace WebCore

#endif // ENABLE(PUSH_API)
