/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositionUnderline_h
#define CompositionUnderline_h

#include "platform/graphics/Color.h"

namespace WebCore {

struct CompositionUnderline {
    CompositionUnderline()
        : startOffset(0), endOffset(0), thick(false)
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
        , startHighlightOffset(-1), endHighlightOffset(-1),backgroundColor( 0 )
#endif
        { }
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
    CompositionUnderline(unsigned s, unsigned e, const Color& c, bool t,unsigned s_h, unsigned e_h,const Color& b_c )
        : startOffset(s), endOffset(e), color(c), thick(t),
          startHighlightOffset(s_h), endHighlightOffset(e_h) ,backgroundColor( b_c ){ }
#else
    CompositionUnderline(unsigned s, unsigned e, const Color& c, bool t)
        : startOffset(s), endOffset(e), color(c), thick(t) { }
#endif
    unsigned startOffset;
    unsigned endOffset;
    Color color;
    bool thick;
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
    int startHighlightOffset;
    int endHighlightOffset;
    Color backgroundColor;
#endif
};

} // namespace WebCore

#endif // CompositionUnderline_h
