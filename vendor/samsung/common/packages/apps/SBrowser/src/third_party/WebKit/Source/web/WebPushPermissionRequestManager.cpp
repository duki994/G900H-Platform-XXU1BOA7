// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(PUSH_API)

#include "WebPushPermissionRequestManager.h"

#include "WebPushPermissionRequest.h"
#include "wtf/HashMap.h"

namespace blink {

using namespace WebCore;

typedef HashMap<PushRegistrationManager*, int> PushIdMap;
typedef HashMap<int, PushRegistrationManager*> IdPushMap;

class WebPushPermissionRequestManagerPrivate {
public:
    PushIdMap m_pushIdMap;
    IdPushMap m_idPushMap;
};

int WebPushPermissionRequestManager::add(const blink::WebPushPermissionRequest& permissionRequest)
{
    PushRegistrationManager* manager = permissionRequest.manager();
    ASSERT(!m_private->m_pushIdMap.contains(manager));
    int id = ++m_lastId;
    m_private->m_pushIdMap.add(manager, id);
    m_private->m_idPushMap.add(id, manager);
    return id;
}

bool WebPushPermissionRequestManager::remove(const blink::WebPushPermissionRequest& permissionRequest, int& id)
{
    PushRegistrationManager* manager = permissionRequest.manager();
    PushIdMap::iterator it = m_private->m_pushIdMap.find(manager);
    if (it == m_private->m_pushIdMap.end())
        return false;
    id = it->value;
    m_private->m_pushIdMap.remove(it);
    m_private->m_idPushMap.remove(id);
    return true;
}

bool WebPushPermissionRequestManager::remove(int id, blink::WebPushPermissionRequest& permissionRequest)
{
    IdPushMap::iterator it = m_private->m_idPushMap.find(id);
    if (it == m_private->m_idPushMap.end())
        return false;
    PushRegistrationManager* manager = it->value;
    permissionRequest = WebPushPermissionRequest(manager);
    m_private->m_idPushMap.remove(it);
    m_private->m_pushIdMap.remove(manager);
    return true;
}

void WebPushPermissionRequestManager::init()
{
    m_lastId = 0;
    m_private.reset(new WebPushPermissionRequestManagerPrivate);
}

void WebPushPermissionRequestManager::reset()
{
    m_private.reset(0);
}

} // namespace blink

#endif // ENABLE(PUSH_API)
