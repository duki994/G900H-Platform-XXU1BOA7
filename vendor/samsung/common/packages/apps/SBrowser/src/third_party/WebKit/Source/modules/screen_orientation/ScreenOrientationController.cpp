// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientationController.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Screen.h"
#include "modules/screen_orientation/ScreenOrientationDispatcher.h"
#include "public/platform/Platform.h"

namespace WebCore {

static const unsigned UnknownOrientation = 0;

ScreenOrientationController::~ScreenOrientationController()
{
    ScreenOrientationDispatcher::instance().removeController(this);
}

ScreenOrientationController* ScreenOrientationController::from(Document* document)
{
    ScreenOrientationController* controller = static_cast<ScreenOrientationController*>(DocumentSupplement::from(document, supplementName()));
    if (!controller) {
        controller = new ScreenOrientationController(*document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtr(controller));
    }
    return controller;
}

ScreenOrientationController::ScreenOrientationController(Document& document)
    : m_document(document)
    , m_orientation(UnknownOrientation)
{
    // FIXME: We should listen for screen orientation change events only when the page is visible.
    ScreenOrientationDispatcher::instance().addController(this);
}

void ScreenOrientationController::dispatchOrientationChangeEvent()
{
    if (m_document.domWindow() && m_document.domWindow()->screen()
        && !m_document.activeDOMObjectsAreSuspended()
        && !m_document.activeDOMObjectsAreStopped()) {
        if (RuntimeEnabledFeatures::screenOrientationEnabled())
            m_document.domWindow()->screen()->dispatchEvent(Event::create(EventTypeNames::orientationchange));
        if (RuntimeEnabledFeatures::prefixedScreenOrientationEnabled())
            m_document.domWindow()->screen()->dispatchEvent(Event::create(EventTypeNames::webkitorientationchange));
    }
}

const char* ScreenOrientationController::supplementName()
{
    return "ScreenOrientationController";
}

void ScreenOrientationController::didChangeScreenOrientation(blink::WebScreenOrientation orientation)
{
    if (orientation == m_orientation)
        return;

    m_orientation = orientation;
    dispatchOrientationChangeEvent();
}

blink::WebScreenOrientation ScreenOrientationController::orientation() const
{
    if (m_orientation == UnknownOrientation)
        m_orientation = blink::Platform::current()->currentScreenOrientation();
    return static_cast<blink::WebScreenOrientation>(m_orientation);
}

} // namespace WebCore
