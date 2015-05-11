#pragma once

//{ Added by DMC.Graphics lab for Font Effect
#ifdef USE_SAMSUNG_FONT_EFFECT
#include "SamsungFontEffect.h"

struct SkFontGradient {
    uint32_t color; // Gradient color
    float alpha;    // Gradient color's alpha. It has range [0.0f:1.0f]
    float position; // Gradient color's position. It has range [0.0f:1.0f]
};

/** \class SkFontEffect

    The SkFontEffect class adapt samsungfonteffect with skia.
*/
class SkFontEffect : public SFontEffect::SFEffect
{
public:
    SkFontEffect(void){}
    virtual ~SkFontEffect(void){}
    SkFontEffect(const SkFontEffect& src) : SFontEffect::SFEffect(src){}
    SkFontEffect(void* buf) : SFontEffect::SFEffect(buf){}
};
#endif
//}