#include "SkFontHost_DMC.h"

int SkFontHostDMC::getLuminance( int color )
{
    const int r = (color >> 16) & 0xFF;
    const int g = (color >>  8) & 0xFF;
    const int b = (color      ) & 0xFF;
    const int luminance = (r * 2 + g * 5 + b) >> 3;
    return luminance;
}

int SkFontHostDMC::getFontGammaFlag( int color )
{
    unsigned int luminance = getLuminance(color);

    // Calculates gamma flags from given colors
    if (luminance <= DEFAULT_TEXT_BLACK_GAMMA_THRESHOLD) {
        return SF_GAMMA_BLACK;
    } else if (luminance >= DEFAULT_TEXT_WHITE_GAMMA_THRESHOLD) {
        return SF_GAMMA_WHITE;
    } else {
        return SF_GAMMA_TABLE_GRAY;
    }
}
