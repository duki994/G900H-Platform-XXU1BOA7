// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(PUSH_API)

#include "modules/push_registration/PushRegistrationManager.h"

#include "base/logging.h"

#include "bindings/v8/CallbackPromiseAdapterContext.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/DOMError.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/frame/Navigator.h"
#include "modules/push_registration/PushController.h"
#include "modules/push_registration/PushError.h"
#include "modules/push_registration/PushIsRegistered.h"
#include "modules/push_registration/PushRegistration.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebPushClient.h"
#include "public/platform/WebPushError.h"
#include "public/platform/WebString.h"
#include "public/web/WebPushPermissionRequest.h"
#include "public/web/WebSecurityOrigin.h"

#include <v8.h>

namespace WebCore {

PushRegistrationManager::PushRegistrationManager(Navigator* navigator)
    : m_pushController(PushController::from(navigator->frame()->page()))
    , m_pushPermission(PushPermission::Default)
{
    ScriptWrappable::init(this);
}

PushRegistrationManager::~PushRegistrationManager()
{
}

class UndefinedValue {
public:
    typedef blink::WebPushIsRegistered WebType;
    static ScriptValue from(ScriptPromiseResolver* resolver, WebType*)
    {
        ASSERT(!promise); // Anything passed here will be leaked.
        v8::Isolate* isolate = resolver->promise().isolate();
        return ScriptValue(v8::Undefined(isolate), isolate);
    }

private:
    UndefinedValue();
};

DEFINE_GC_INFO(PushRegistrationManager::PushNotifier);

PushRegistrationManager::PushNotifier::PushNotifier(PushRegistrationManager* manager, PushRegisterCallback* callback, ExecutionContext* context)
    : m_manager(manager)
    , m_callback(callback)
    , m_context(context)
{
    ASSERT(m_manager);
    ASSERT(m_callback);
    ASSERT(m_context);   
}

void PushRegistrationManager::PushNotifier::trace(Visitor* visitor)
{
    visitor->trace(m_manager);
}

ScriptPromise PushRegistrationManager::registerPush(ExecutionContext* executionContext)
{
    ScriptPromise promise = ScriptPromise::createPending(executionContext);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(promise, executionContext);
    PushRegisterCallback* callback = new PushRegisterCallback(resolver, executionContext);

    RefPtrWillBeRawPtr<PushNotifier> notifier = PushNotifier::create(this, callback, executionContext);
    startRequest(notifier.get(), executionContext);

    return promise;
}

void PushRegistrationManager::startRequest(PushNotifier* notifier, ExecutionContext* executionContext)
{
    switch (m_pushPermission) {
    case PushPermission::Denied :
        notifier->permissionDenied();
        return;
    case PushPermission::Granted :
        notifier->permissionGranted();
        return;
    case PushPermission::InProgress :
    case PushPermission::Default :
        m_pendingForPermissionNotifiers.add(notifier);
        requestPermission(executionContext);
    }
}

void PushRegistrationManager::PushNotifier::permissionGranted()
{
    m_manager->controller()->client()->registerPush(
            blink::WebSecurityOrigin(m_context->securityOrigin()), m_callback);
}

void PushRegistrationManager::PushNotifier::permissionDenied()
{
    m_callback->onError(new blink::WebPushError(
            blink::WebPushError::PermissionDeniedError,
            blink::WebString::fromUTF8("User denied Push")));
}

void PushRegistrationManager::requestPermission(ExecutionContext* executionContext)
{
    if (m_pushPermission > PushPermission::Default)
        return;

    m_pushController->client()->requestPermission(blink::WebSecurityOrigin(executionContext->securityOrigin()),
            blink::WebPushPermissionRequest(this));
}

ScriptPromise PushRegistrationManager::unregisterPush(ExecutionContext* executionContext)
{
    ScriptPromise promise = ScriptPromise::createPending(executionContext);
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(promise, executionContext);

    // Undefined
    m_pushController->client()->unregisterPush(
            blink::WebSecurityOrigin(executionContext->securityOrigin()),
            new CallbackPromiseAdapterContext<UndefinedValue, PushError>(resolver, executionContext));
    return promise;
}

ScriptPromise PushRegistrationManager::isRegisteredPush(ExecutionContext* executionContext)
{
    ScriptPromise promise = ScriptPromise::createPending(executionContext);
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(promise, executionContext);

    m_pushController->client()->isRegisteredPush(
            blink::WebSecurityOrigin(executionContext->securityOrigin()),
            new CallbackPromiseAdapterContext<PushIsRegistered, PushError>(resolver, executionContext));
    return promise;
}

ScriptPromise PushRegistrationManager::hasPermissionPush(ExecutionContext* executionContext)
{
    ScriptPromise promise = ScriptPromise::createPending(executionContext);
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(promise, executionContext);

    if (m_pushPermission != PushPermission::Default) {
        DOMRequestState requestState(executionContext);
        v8::Isolate* isolate = requestState.isolate();

        PushPermission::PermissionType permission =
                (permission == PushPermission::InProgress) ? PushPermission::Default : m_pushPermission;

        resolver->resolve(ScriptValue(v8AtomicString(isolate, PushPermission::permissionString(m_pushPermission).utf8().data()), isolate));
        return promise;
    }

    m_pushController->client()->hasPermissionPush(
            blink::WebSecurityOrigin(executionContext->securityOrigin()),
            new CallbackPromiseAdapterContext<PushPermission, PushError>(resolver, executionContext));

    return promise;
}

void PushRegistrationManager::setIsAllowed(bool allowed)
{
    RefPtrWillBeRawPtr<PushRegistrationManager> protect(this);

    m_pushPermission = allowed ? PushPermission::Granted : PushPermission::Denied;

    if (!m_pendingForPermissionNotifiers.isEmpty()) {
        handlePendingPermissionNotifiers();
        m_pendingForPermissionNotifiers.clear();
    }
}

void PushRegistrationManager::handlePendingPermissionNotifiers()
{
    PushNotifierSet::const_iterator end = m_pendingForPermissionNotifiers.end();
    for (PushNotifierSet::const_iterator iter = m_pendingForPermissionNotifiers.begin(); iter != end; ++iter) {
        PushNotifier* notifier = iter->get();
        if (isGranted())
            notifier->permissionGranted();
        else
            notifier->permissionDenied();
    }
}

} // namespace WebCore

#endif // ENABLE(PUSH_API)
