// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackPromiseAdapterContext_h
#define CallbackPromiseAdapterContext_h

#if ENABLE(PUSH_API)

#include "bindings/v8/DOMRequestState.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "public/platform/WebCallbacks.h"

namespace WebCore {

template<typename S, typename T>
class CallbackPromiseAdapterContext FINAL : public blink::WebCallbacks<typename S::WebType, typename T::WebType> {
public:
    explicit CallbackPromiseAdapterContext(PassRefPtr<ScriptPromiseResolver> resolver, ExecutionContext* context)
        : m_resolver(resolver)
        , m_requestState(context)
    {
    }
    virtual ~CallbackPromiseAdapterContext() { }

    virtual void onSuccess(typename S::WebType* result) OVERRIDE
    {
        DOMRequestState::Scope scope(m_requestState);
        m_resolver->resolve(S::from(m_resolver.get(), result));
    }

    virtual void onError(typename T::WebType* error) OVERRIDE
    {
        DOMRequestState::Scope scope(m_requestState);
        m_resolver->reject(T::from(m_resolver.get(), error));
    }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
    DOMRequestState m_requestState;
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapterContext);
};

} // namespace WebCore

#endif // ENABLE(PUSH_API)

#endif
