/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebCompositionUnderline_h
#define WebCompositionUnderline_h

#include "../platform/WebColor.h"

namespace blink {

// Class WebCompositionUnderline is intended to be used with WebWidget's
// setComposition() method.
struct WebCompositionUnderline {
    WebCompositionUnderline()
        : startOffset(0)
        , endOffset(0)
        , color(0)
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
        , thick(false)
        , startHighlightOffset(-1)
        , endHighlightOffset(-1)
        , backgroundColor(0)  { }
#else
        , thick(false) { }
#endif
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
    WebCompositionUnderline(unsigned s, unsigned e, WebColor c, bool t)
        : startOffset(s)
        , endOffset(e)
        , color(c)
        , thick(t)
        , startHighlightOffset(-1)
        , endHighlightOffset(-1)
        , backgroundColor(0) { }

    WebCompositionUnderline(unsigned s, unsigned e, WebColor c, bool t, unsigned s_h, unsigned e_h, WebColor b_c)
#else
    WebCompositionUnderline(unsigned s, unsigned e, WebColor c, bool t)
#endif
        : startOffset(s)
        , endOffset(e)
        , color(c)
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
        , thick(t)
        , startHighlightOffset(s_h)
        , endHighlightOffset(e_h)
        , backgroundColor(b_c){ }
#else
        , thick(t) { }
#endif
    unsigned startOffset;
    unsigned endOffset;
    WebColor color;
    bool thick;
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
    int startHighlightOffset;
    int endHighlightOffset;
    WebColor backgroundColor;
#endif
};

} // namespace blink

#endif
