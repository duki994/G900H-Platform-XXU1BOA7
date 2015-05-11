// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#if defined(S_FOP_SMOOTH_SCROLL)

#ifndef WebScrollOffsetAnimationCurve_h
#define WebScrollOffsetAnimationCurve_h

#include "WebAnimationCurve.h"
#include "WebFloatPoint.h"

namespace blink {

class WebScrollOffsetAnimationCurve : public WebAnimationCurve {
public:
    virtual ~WebScrollOffsetAnimationCurve() { }

    virtual void setInitialValue(WebFloatPoint) = 0;
    virtual WebFloatPoint getValue(double time) const = 0;
    virtual double duration() const = 0;
};

} // namespace blink

#endif // WebScrollOffsetAnimationCurve_h

#endif
