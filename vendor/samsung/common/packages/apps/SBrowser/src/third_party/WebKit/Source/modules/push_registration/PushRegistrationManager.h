// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushRegistrationManager_h
#define PushRegistrationManager_h

#if ENABLE(PUSH_API)

#include "bindings/v8/CallbackPromiseAdapter.h"
#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "modules/push_registration/PushPermission.h"

namespace WebCore {

class ExecutionContext;
class Navigator;
class PushController;
class PushError;
class PushRegistration;
class ScriptPromise;
class ScriptPromiseResolver;

class PushRegistrationManager FINAL : public RefCountedWillBeGarbageCollectedFinalized<PushRegistrationManager>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<PushRegistrationManager> create(Navigator* navigator)
    {
        return adoptRefWillBeNoop(new PushRegistrationManager(navigator));
    }
    virtual ~PushRegistrationManager();

    ScriptPromise registerPush(ExecutionContext*);
    ScriptPromise unregisterPush(ExecutionContext*);
    ScriptPromise isRegisteredPush(ExecutionContext*);
    ScriptPromise hasPermissionPush(ExecutionContext*);

    PushController* controller() { return m_pushController; }
    void setPermission(bool);
    void trace(Visitor*) { }
    void setIsAllowed(bool);

private:
    PushRegistrationManager(Navigator* navigator);
    PushController* m_pushController;

    typedef CallbackPromiseAdapter<PushRegistration, PushError> PushRegisterCallback;


    class PushNotifier : public RefCountedWillBeGarbageCollectedFinalized<PushNotifier> {
        DECLARE_GC_INFO;
    public:
        static PassRefPtrWillBeRawPtr<PushNotifier> create(PushRegistrationManager* manager, PushRegisterCallback* callback, ExecutionContext* context)
        {
            return adoptRefWillBeNoop(new PushNotifier(manager, callback, context));
        }
        void trace(Visitor*);

        void permissionDenied();
        void permissionGranted();

    private:
        PushNotifier(PushRegistrationManager*, PushRegisterCallback*, ExecutionContext*);

        RefPtrWillBeMember<PushRegistrationManager> m_manager;
        PushRegisterCallback* m_callback;
        ExecutionContext* m_context;
    };

    typedef WillBeHeapHashSet<RefPtrWillBeMember<PushNotifier> > PushNotifierSet;

    bool isGranted() const { return m_pushPermission == PushPermission::Granted; }
    bool isDenied() const { return m_pushPermission == PushPermission::Denied; }

    void startRequest(PushNotifier*, ExecutionContext*);
    void requestPermission(ExecutionContext*);
    void handlePendingPermissionNotifiers();

    PushPermission::PermissionType m_pushPermission;
    PushNotifierSet m_pendingForPermissionNotifiers;
};

} // namespace WebCore

#endif // ENABLE(PUSH_API)

#endif // PushRegistrationManager_h
