// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/dom/DocumentSupplementable.h"
#include "public/platform/WebScreenOrientation.h"

namespace WebCore {

class ScreenOrientationController FINAL : public DocumentSupplement {
public:
    virtual ~ScreenOrientationController();

    void didChangeScreenOrientation(blink::WebScreenOrientation);

    blink::WebScreenOrientation orientation() const;

    // DocumentSupplement API.
    static ScreenOrientationController* from(Document*);
    static const char* supplementName();

private:
    explicit ScreenOrientationController(Document&);

    void dispatchOrientationChangeEvent();

    Document& m_document;
    mutable blink::WebScreenOrientations m_orientation;
};

} // namespace WebCore

#endif // ScreenOrientationController_h
