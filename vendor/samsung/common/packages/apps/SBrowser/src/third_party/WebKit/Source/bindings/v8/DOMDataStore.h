/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMDataStore_h
#define DOMDataStore_h

#include "bindings/v8/DOMWrapperMap.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptWrappable.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include <v8.h>
#include "wtf/Noncopyable.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

class Node;

class DOMDataStore {
    WTF_MAKE_NONCOPYABLE(DOMDataStore);
public:
    explicit DOMDataStore(WrapperWorldType);
    ~DOMDataStore();

    static DOMDataStore& current(v8::Isolate*);

    // We can use a wrapper stored in a ScriptWrappable when we're in the main world.
    // This method does the fast check if we're in the main world. If this method returns true,
    // it is guaranteed that we're in the main world. On the other hand, if this method returns
    // false, nothing is guaranteed (we might be in the main world).
    template<typename T>
    static bool canUseScriptWrappable(T* object)
    {
        return !DOMWrapperWorld::isolatedWorldsExist()
            && !canExistInWorker(object)
            && ScriptWrappable::wrapperCanBeStoredInObject(object);
    }

    template<typename V8T, typename T, typename Wrappable>
    static bool setReturnValueFromWrapperFast(v8::ReturnValue<v8::Value> returnValue, T* object, v8::Local<v8::Object> holder, Wrappable* wrappable)
    {
        if (canUseScriptWrappable(object))
            return ScriptWrappable::setReturnValueWithSecurityCheck<V8T>(returnValue, object);
        // The second fastest way to check if we're in the main world is to check if
        // the wrappable's wrapper is the same as the holder.
        // FIXME: Investigate if it's worth having this check for performance.
        if (holderContainsWrapper(holder, wrappable)) {
            if (ScriptWrappable::wrapperCanBeStoredInObject(object))
                return ScriptWrappable::setReturnValueWithSecurityCheck<V8T>(returnValue, object);
            return DOMWrapperWorld::mainWorld()->domDataStore().m_wrapperMap.setReturnValueFrom(returnValue, V8T::toInternalPointer(object));
        }
        return current(returnValue.GetIsolate()).template setReturnValueFrom<V8T>(returnValue, object);
    }

    template<typename V8T, typename T>
    static bool setReturnValueFromWrapper(v8::ReturnValue<v8::Value> returnValue, T* object)
    {
        if (canUseScriptWrappable(object))
            return ScriptWrappable::setReturnValueWithSecurityCheck<V8T>(returnValue, object);
        return current(returnValue.GetIsolate()).template setReturnValueFrom<V8T>(returnValue, object);
    }

    template<typename V8T, typename T>
    static bool setReturnValueFromWrapperForMainWorld(v8::ReturnValue<v8::Value> returnValue, T* object)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object))
            return ScriptWrappable::setReturnValue(returnValue, object);
        return DOMWrapperWorld::mainWorld()->domDataStore().m_wrapperMap.setReturnValueFrom(returnValue, V8T::toInternalPointer(object));
    }

    template<typename V8T, typename T>
    static v8::Handle<v8::Object> getWrapper(T* object, v8::Isolate* isolate)
    {
        if (canUseScriptWrappable(object)) {
            v8::Handle<v8::Object> result = ScriptWrappable::getUnsafeWrapperFromObject(object).newLocal(isolate);
            // Security: always guard against malicious tampering.
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result.IsEmpty() || result->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex) == V8T::toInternalPointer(object));
            return result;
        }
        return current(isolate).template get<V8T>(object, isolate);
    }

    template<typename V8T, typename T>
    static void setWrapperReference(const v8::Persistent<v8::Object>& parent, T* child, v8::Isolate* isolate)
    {
        if (canUseScriptWrappable(child)) {
            UnsafePersistent<v8::Object> unsafePersistent = ScriptWrappable::getUnsafeWrapperFromObject(child);
            // Security: always guard against malicious tampering.
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(unsafePersistent.isEmpty() || unsafePersistent.value()->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex) == V8T::toInternalPointer(child));
            unsafePersistent.setReferenceFrom(parent, isolate);
            return;
        }
        current(isolate).template setReference<V8T>(parent, child, isolate);
    }

    template<typename V8T, typename T>
    static void setWrapper(T* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        if (canUseScriptWrappable(object)) {
            ScriptWrappable::setWrapperInObject(object, wrapper, isolate, configuration);
            return;
        }
        return current(isolate).template set<V8T>(object, wrapper, isolate, configuration);
    }

    template<typename V8T, typename T>
    static bool containsWrapper(T* object, v8::Isolate* isolate)
    {
        return current(isolate).template containsWrapper<V8T>(object);
    }

    template<typename V8T, typename T>
    inline v8::Handle<v8::Object> get(T* object, v8::Isolate* isolate)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && m_type == MainWorld)
            return ScriptWrappable::getUnsafeWrapperFromObject(object).newLocal(isolate);
        return m_wrapperMap.newLocal(V8T::toInternalPointer(object), isolate);
    }

    template<typename V8T, typename T>
    inline void setReference(const v8::Persistent<v8::Object>& parent, T* child, v8::Isolate* isolate)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(child) && m_type == MainWorld) {
            ScriptWrappable::getUnsafeWrapperFromObject(child).setReferenceFrom(parent, isolate);
            return;
        }
        m_wrapperMap.setReference(parent, V8T::toInternalPointer(child), isolate);
    }

    template<typename V8T, typename T>
    inline bool setReturnValueFrom(v8::ReturnValue<v8::Value> returnValue, T* object)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && m_type == MainWorld)
            return ScriptWrappable::setReturnValue(returnValue, object);
        return m_wrapperMap.setReturnValueFrom(returnValue, V8T::toInternalPointer(object));
    }

    template<typename V8T, typename T>
    inline bool containsWrapper(T* object)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && m_type == MainWorld)
            return !ScriptWrappable::getUnsafeWrapperFromObject(object).isEmpty();
        return m_wrapperMap.containsKey(V8T::toInternalPointer(object));
    }

private:
    template<typename V8T, typename T>
    inline void set(T* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        ASSERT(!!object);
        ASSERT(!wrapper.IsEmpty());
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && m_type == MainWorld) {
            ScriptWrappable::setWrapperInObject(object, wrapper, isolate, configuration);
            return;
        }
        m_wrapperMap.set(V8T::toInternalPointer(object), wrapper, configuration);
    }

    static bool canExistInWorker(void*) { return true; }
    static bool canExistInWorker(Node*) { return false; }

    static bool holderContainsWrapper(v8::Local<v8::Object>, void*)
    {
        return false;
    }

    static bool holderContainsWrapper(v8::Local<v8::Object> holder, ScriptWrappable* wrappable)
    {
        // Verify our assumptions about the main world.
        UnsafePersistent<v8::Object> unsafePersistent = wrappable->unsafePersistent();
        ASSERT(unsafePersistent.isEmpty() || !(holder == *unsafePersistent.persistent()) || current(v8::Isolate::GetCurrent()).m_type == MainWorld);
        return holder == *unsafePersistent.persistent();
    }

    WrapperWorldType m_type;
    DOMWrapperMap<void> m_wrapperMap;
};

} // namespace WebCore

#endif // DOMDataStore_h
