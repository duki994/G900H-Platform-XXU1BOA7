/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderThemeChromiumAndroid_h
#define RenderThemeChromiumAndroid_h

#include "core/rendering/RenderThemeChromiumDefault.h"

namespace WebCore {

class RenderThemeChromiumAndroid FINAL : public RenderThemeChromiumDefault {
public:
    static PassRefPtr<RenderTheme> create();
    virtual String extraDefaultStyleSheet() OVERRIDE;

    virtual Color systemColor(CSSValueID) const OVERRIDE;

    virtual void adjustInnerSpinButtonStyle(RenderStyle*, Element*) const OVERRIDE;

    virtual bool delegatesMenuListRendering() const OVERRIDE { return true; }

    virtual bool paintMediaOverlayPlayButton(RenderObject*, const PaintInfo&, const IntRect&) OVERRIDE;

    virtual String extraMediaControlsStyleSheet() OVERRIDE;

    virtual Color platformTapHighlightColor() const OVERRIDE
    {
        return RenderThemeChromiumAndroid::defaultTapHighlightColor;
    }
    virtual Color activeSelectionBackgroundColor() const OVERRIDE
    {
        return platformActiveSelectionBackgroundColor();
    }

    virtual Color inactiveSelectionBackgroundColor() const OVERRIDE
    {
        return platformInactiveSelectionBackgroundColor();
    }

    virtual Color platformActiveSelectionBackgroundColor() const OVERRIDE
    {
#if defined(S_TEXT_HIGHLIGHT_SELECTION_COLOR)
        return makeRGB(178, 235, 242);
#else
        return makeRGB(66, 142, 186);
#endif
    }

    virtual Color platformInactiveSelectionBackgroundColor() const OVERRIDE
    {
#if defined(S_TEXT_HIGHLIGHT_SELECTION_COLOR)
        return makeRGB(178, 235, 242);
#else
        return makeRGB(66, 142, 186);
#endif
    }
    virtual Color platformActiveSelectionForegroundColor() const OVERRIDE
    {
#if defined(S_TEXT_HIGHLIGHT_SELECTION_COLOR)
        return Color::black;
#else
        return Color::white;
#endif
    }
    virtual Color platformActiveTextSearchHighlightColor()const OVERRIDE
    {
#if defined(S_PLM_P140605_06393)
        return makeRGB(121, 202, 242);
#else
        return Color(0x00,0x99,0xcc,0x99);
#endif
    }
    virtual Color platformInactiveTextSearchHighlightColor()const OVERRIDE
    {
        return Color(0x33,0xb5,0xe5,0x66);
    }

protected:
    virtual int menuListArrowPadding() const OVERRIDE;

private:
    virtual ~RenderThemeChromiumAndroid();

    static const RGBA32 defaultTapHighlightColor = 0x6633b5e5;
    static const RGBA32 defaultActiveSelectionBackgroundColor = 0x6633b5e5;
};

} // namespace WebCore

#endif // RenderThemeChromiumAndroid_h
