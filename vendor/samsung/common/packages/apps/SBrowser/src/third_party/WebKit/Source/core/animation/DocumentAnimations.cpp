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

#include "config.h"
#include "core/animation/DocumentAnimations.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

namespace {

void updateAnimationTiming(Document& document)
{
    bool didTriggerStyleRecalc = document.timeline()->serviceAnimations();
    didTriggerStyleRecalc |= document.transitionTimeline()->serviceAnimations();
    if (!didTriggerStyleRecalc)
        document.animationClock().unfreeze();
}

void dispatchAnimationEvents(Document& document)
{
    document.timeline()->dispatchEvents();
    document.transitionTimeline()->dispatchEvents();
}

void dispatchAnimationEventsAsync(Document& document)
{
    document.timeline()->dispatchEventsAsync();
    document.transitionTimeline()->dispatchEventsAsync();
}

} // namespace

void DocumentAnimations::serviceOnAnimationFrame(Document& document, double monotonicAnimationStartTime)
{
    document.animationClock().updateTime(monotonicAnimationStartTime);
    updateAnimationTiming(document);
    dispatchAnimationEvents(document);
}

void DocumentAnimations::serviceBeforeGetComputedStyle(Node& node, CSSPropertyID property)
{
    if (!node.isElementNode())
        return;
    const Element& element = toElement(node);
    if (element.document().timeline()->hasPlayerNeedingUpdate()) {
        updateAnimationTiming(element.document());
        return;
    }
    if (const ActiveAnimations* activeAnimations = element.activeAnimations()) {
        if (activeAnimations->hasActiveAnimationsOnCompositor(property))
            updateAnimationTiming(element.document());
    }
}

void DocumentAnimations::serviceAfterStyleRecalc(Document& document)
{
    if (document.cssPendingAnimations().startPendingAnimations() && document.view())
        document.view()->scheduleAnimation();

    document.animationClock().unfreeze();
    dispatchAnimationEventsAsync(document);
}

} // namespace WebCore
