/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGViewSpec_h
#define SVGViewSpec_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGTransformList.h"
#include "core/svg/SVGZoomAndPan.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class SVGTransformListPropertyTearOff;

class SVGViewSpec FINAL : public RefCounted<SVGViewSpec>, public ScriptWrappable, public SVGZoomAndPan, public SVGFitToViewBox {
public:
    using RefCounted<SVGViewSpec>::ref;
    using RefCounted<SVGViewSpec>::deref;

    static PassRefPtr<SVGViewSpec> create(SVGSVGElement* contextElement)
    {
        return adoptRef(new SVGViewSpec(contextElement));
    }

    bool parseViewSpec(const String&);
    void reset();

    SVGElement* viewTarget() const;
    String viewBoxString() const;

    String preserveAspectRatioString() const;

    void setTransformString(const String&);
    String transformString() const;

    void setViewTargetString(const String& string) { m_viewTargetString = string; }
    String viewTargetString() const { return m_viewTargetString; }

    SVGElement* contextElement() const { return m_contextElement; }
    void detachContextElement();

    // Custom non-animated 'transform' property.
    SVGTransformListPropertyTearOff* transform();
    SVGTransformList transformBaseValue() const { return m_transform; }

private:
    explicit SVGViewSpec(SVGSVGElement*);

    static const SVGPropertyInfo* transformPropertyInfo();

    static const AtomicString& transformIdentifier();

    static PassRefPtr<SVGAnimatedProperty> lookupOrCreateTransformWrapper(SVGViewSpec* contextElement);

    template<typename CharType>
    bool parseViewSpecInternal(const CharType* ptr, const CharType* end);

    // FIXME(oilpan): This is back-ptr to be cleared from contextElement.
    SVGSVGElement* m_contextElement;
    SVGTransformList m_transform;
    String m_viewTargetString;
};

} // namespace WebCore

#endif
