/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef Filter_h
#define Filter_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class FilterEffect;

class PLATFORM_EXPORT Filter : public RefCounted<Filter> {
public:
    Filter(const AffineTransform& absoluteTransform)
    : m_isAccelerated(false)
    , m_absoluteTransform(absoluteTransform)
    , m_inverseTransform(absoluteTransform.inverse())
    {
        // Filters can only accept scaling and translating transformations, as coordinates
        // in most primitives are given in horizontal and vertical directions.
        ASSERT(!absoluteTransform.b() && !absoluteTransform.c());
    }
    virtual ~Filter() { }

    void setSourceImage(PassOwnPtr<ImageBuffer> sourceImage) { m_sourceImage = sourceImage; }
    ImageBuffer* sourceImage() { return m_sourceImage.get(); }

    const AffineTransform& absoluteTransform() const { return m_absoluteTransform; }

    void setAbsoluteTransform(const AffineTransform& absoluteTransform)
    {
        // Filters can only accept scaling and translating transformations, as coordinates
        // in most primitives are given in horizontal and vertical directions.
        ASSERT(!absoluteTransform.b() && !absoluteTransform.c());
        m_absoluteTransform = absoluteTransform;
        m_inverseTransform = absoluteTransform.inverse();
        m_absoluteFilterRegion = m_absoluteTransform.mapRect(m_filterRegion);
    }
    FloatPoint mapAbsolutePointToLocalPoint(const FloatPoint& point) const { return m_inverseTransform.mapPoint(point); }
    FloatRect mapLocalRectToAbsoluteRect(const FloatRect& rect) const { return m_absoluteTransform.mapRect(rect); }
    FloatRect mapAbsoluteRectToLocalRect(const FloatRect& rect) const { return m_inverseTransform.mapRect(rect); }

    bool isAccelerated() const { return m_isAccelerated; }
    void setIsAccelerated(bool isAccelerated) { m_isAccelerated = isAccelerated; }

    virtual float applyHorizontalScale(float value) const
    {
        return value * m_absoluteTransform.a();
    }
    virtual float applyVerticalScale(float value) const
    {
        return value * m_absoluteTransform.d();
    }

    virtual IntRect sourceImageRect() const = 0;

    FloatRect absoluteFilterRegion() const { return m_absoluteFilterRegion; }

    FloatRect filterRegion() const { return m_filterRegion; }
    void setFilterRegion(const FloatRect& rect)
    {
        m_filterRegion = rect;
        m_absoluteFilterRegion = m_absoluteTransform.mapRect(m_filterRegion);
    }


private:
    OwnPtr<ImageBuffer> m_sourceImage;
    bool m_isAccelerated;
    AffineTransform m_absoluteTransform;
    AffineTransform m_inverseTransform;
    FloatRect m_absoluteFilterRegion;
    FloatRect m_filterRegion;
};

} // namespace WebCore

#endif // Filter_h
