/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8Element.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8BindingMacros.h"
#include "core/dom/Element.h"

namespace WebCore {

void V8Element::scrollLeftAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    ExceptionState exceptionState(ExceptionState::SetterContext, "scrollLeft", "Element", info.Holder(), info.GetIsolate());
    Element* imp = V8Element::toNative(info.Holder());

    if (RuntimeEnabledFeatures::cssomSmoothScrollEnabled() && value->IsObject()) {
        V8TRYCATCH_VOID(Dictionary, scrollOptionsHorizontal, Dictionary(value, info.GetIsolate()));
        imp->setScrollLeft(scrollOptionsHorizontal, exceptionState);
        exceptionState.throwIfNeeded();
        return;
    }

    V8TRYCATCH_EXCEPTION_VOID(int, position, toInt32(value, exceptionState), exceptionState);
    imp->setScrollLeft(position);
}

void V8Element::scrollTopAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    ExceptionState exceptionState(ExceptionState::SetterContext, "scrollTop", "Element", info.Holder(), info.GetIsolate());
    Element* imp = V8Element::toNative(info.Holder());

    if (RuntimeEnabledFeatures::cssomSmoothScrollEnabled() && value->IsObject()) {
        V8TRYCATCH_VOID(Dictionary, scrollOptionsVertical, Dictionary(value, info.GetIsolate()));
        imp->setScrollTop(scrollOptionsVertical, exceptionState);
        exceptionState.throwIfNeeded();
        return;
    }

    V8TRYCATCH_EXCEPTION_VOID(int, position, toInt32(value, exceptionState), exceptionState);
    imp->setScrollTop(position);
}

} // namespace WebCore
