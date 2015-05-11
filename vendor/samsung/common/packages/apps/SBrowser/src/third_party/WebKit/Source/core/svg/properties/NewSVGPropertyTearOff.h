/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef NewSVGPropertyTearOff_h
#define NewSVGPropertyTearOff_h

#include "core/dom/QualifiedName.h"
#include "core/svg/properties/NewSVGProperty.h"
#include "wtf/RefCounted.h"

namespace WebCore {

enum PropertyIsAnimValType {
    PropertyIsNotAnimVal,
    PropertyIsAnimVal
};

class NewSVGPropertyTearOffBase : public RefCounted<NewSVGPropertyTearOffBase> {
public:
    virtual ~NewSVGPropertyTearOffBase() { }

    PropertyIsAnimValType propertyIsAnimVal() const
    {
        return m_propertyIsAnimVal;
    }

    bool isAnimVal() const
    {
        return m_propertyIsAnimVal == PropertyIsAnimVal;
    }

    bool isReadOnlyProperty() const
    {
        return m_isReadOnlyProperty;
    }

    void setIsReadOnlyProperty()
    {
        m_isReadOnlyProperty = true;
    }

    bool isImmutable() const
    {
        return isReadOnlyProperty() || isAnimVal();
    }

    virtual void commitChange();

    SVGElement* contextElement()
    {
        return m_contextElement;
    }

    const QualifiedName& attributeName()
    {
        return m_attributeName;
    }

    virtual AnimatedPropertyType type() const = 0;

protected:
    NewSVGPropertyTearOffBase(SVGElement* contextElement, PropertyIsAnimValType propertyIsAnimVal, const QualifiedName& attributeName = nullQName())
        : m_contextElement(contextElement)
        , m_propertyIsAnimVal(propertyIsAnimVal)
        , m_isReadOnlyProperty(false)
        , m_attributeName(attributeName)
    {
    }

private:
    // These references are kept alive from V8 wrapper to prevent reference cycles
    SVGElement* m_contextElement;

    PropertyIsAnimValType m_propertyIsAnimVal;
    bool m_isReadOnlyProperty;
    QualifiedName m_attributeName;
};

template <typename Property>
class NewSVGPropertyTearOff : public NewSVGPropertyTearOffBase {
public:
    Property* target()
    {
        return m_target.get();
    }

    void setTarget(PassRefPtr<Property> target)
    {
        m_target = target;
    }

    virtual AnimatedPropertyType type() const OVERRIDE
    {
        return Property::classType();
    }

protected:
    NewSVGPropertyTearOff(PassRefPtr<Property> target, SVGElement* contextElement, PropertyIsAnimValType propertyIsAnimVal, const QualifiedName& attributeName = nullQName())
        : NewSVGPropertyTearOffBase(contextElement, propertyIsAnimVal, attributeName)
        , m_target(target)
    {
        ASSERT(m_target);
    }

private:
    RefPtr<Property> m_target;
};

}

#endif // NewSVGPropertyTearOff_h
