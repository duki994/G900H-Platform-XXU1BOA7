#include "SkFontHost_Freetype_DMC.h"
#include "SkFontHost_DMC.h"
#include "properties.h"

#include <android/log.h>
#define FSNLOG(...) __android_log_print(ANDROID_LOG_DEBUG, "FSN_FONTSOLUTION", __VA_ARGS__);

#define CJK_UNIFIED_START   0x4E00
#define CJK_UNIFIED_END     0x9FFF
#define CJK_UNIFIED_EXTA_START   0x3400
#define CJK_UNIFIED_EXTA_END     0x4DFF
#define CJK_UNIFIED_EXTB_START   0x20000
#define CJK_UNIFIED_EXTB_END     0x2A6DF
#define CJK_COMPATABILITY_START     0xF900
#define CJK_COMPATABILITY_END       0xFAFF
#define CJK_COMPATABILITY_SUPPLEMENT_START     0x2F800
#define CJK_COMPATABILITY_SUPPLEMENT_END       0x2FA1F
#define JPN_HIRAGANA_START       0x3040
#define JPN_KATAKANA_END         0x30FF
#define KOREAN_UNICODE_START 0xAC00
#define KOREAN_UNICODE_END 0xD7A3

//#define TEST_TUNNING

// Initialize static font weight strength
FSFontWeight SkAutoFontProperty::fFontWeights[FS_LANG_MAX]= {
    {SF_GLYPH_WEIGHT_SATURATION, 9, 40, 0, 0}, //korean
    {SF_GLYPH_WEIGHT_SATURATION, 9, 40, 40, 0}, //chinese, japanese
    {SF_GLYPH_WEIGHT_SATURATION, 9, 40, 140, 0}, //other language(latin, english, etc..)
};

SkAutoFontProperty::SkAutoFontProperty(FT_Face face, unsigned int glyphId, int fontGammaFlag) : fFace(face)
{
    setProperty(fFace,glyphId, fontGammaFlag);
}

SkAutoFontProperty::~SkAutoFontProperty()
{
    if(fFace) {
        clearProperty(fFace);
        fFace = NULL;
    }
}

void SkAutoFontProperty::setProperty( FT_Face face, unsigned int glyphId, int fontGammaFlag)
{
    FT_ULong unicode = SF_GetUnicodeGlyphindex(face,glyphId);
    int glyphWeightLang = 0;
    int maxWeight = -1;
    int minWeight = -1;

#ifdef TEST_TUNNING
    char FONT_WEIGHT_MAX[] = "persist.maxweight\0";
    char property[PROPERTY_VALUE_MAX];
    if(property_get(FONT_WEIGHT_MAX, property, NULL) > 0) {
        maxWeight = atoi(property);
    }

    char FONT_WEIGHT_MIN[] = "persist.minweight\0";
    char property2[PROPERTY_VALUE_MAX];
    if(property_get(FONT_WEIGHT_MIN, property2, NULL) > 0) {
        minWeight = atoi(property2);
    }
#endif

    // Gets languae flag from given glyph Id
    if( (unicode >= CJK_UNIFIED_START && unicode <= CJK_UNIFIED_END) ||
        (unicode >= CJK_UNIFIED_EXTA_START && unicode <= CJK_UNIFIED_EXTA_END) ||
        (unicode >= CJK_UNIFIED_EXTB_START && unicode <= CJK_UNIFIED_EXTB_END) ||
        (unicode >= CJK_COMPATABILITY_START && unicode <= CJK_COMPATABILITY_END) ||
        (unicode >= CJK_COMPATABILITY_SUPPLEMENT_START && unicode <= CJK_COMPATABILITY_SUPPLEMENT_END) ||
        (unicode >= JPN_HIRAGANA_START &&unicode <= JPN_KATAKANA_END) ){
            glyphWeightLang = FS_LANG_CJ;
    }
    else if(unicode >= KOREAN_UNICODE_START &&unicode <= KOREAN_UNICODE_END) {
        glyphWeightLang = FS_LANG_KOR;
    }
    else
        glyphWeightLang = FS_LANG_OTHERS;

    // Puts glyph weight values per given language
    fFace->fetstate.iGlyphWeightExtensionMode = fFontWeights[glyphWeightLang].glyphWeightMode;
    fFace->fetstate.iGlyphWeightMinSize = fFontWeights[glyphWeightLang].glyphWeightMinFontSize;
    fFace->fetstate.iGlyphWeightMaxSize = fFontWeights[glyphWeightLang].glyphWeightMaxFontSize;
    fFace->fetstate.iGlyphWeightMinWeightX = fFontWeights[glyphWeightLang].glyphWeightMinStrengthX;
    fFace->fetstate.iGlyphWeightMaxWeightX = fFontWeights[glyphWeightLang].glyphWeightMaxStrenghtX;

#ifdef TEST_TUNNING
    if(maxWeight >=0) {
        fFace->fetstate.iGlyphWeightMinWeightX  = minWeight;
            ("MaxWeight = %d", maxWeight);
    }

    if(minWeight >=0) {
        fFace->fetstate.iGlyphWeightMaxWeightX  = maxWeight;
        FSNLOG("MinWeight = %d", minWeight);
    }

#endif
    face->fetstate.iGammaCorrectionMode = SF_GAMMA_CORRECTION_TABLE;
    // Should assign given gamma flag from luminance
    face->fetstate.iGammeTableFeature = fontGammaFlag;
    face->fetstate.blackGammaTable = gBlackFontGamma;
    face->fetstate.whiteGammaTable = gWhiteGamma;

    SF_Calculate_Gradient(face);
}

// Clears font property with initial value
void SkAutoFontProperty::clearProperty( FT_Face face )
{
    face->fetstate.iGlyphWeightExtensionMode = SF_GLYPH_WEIGHT_NONE;
    face->fetstate.iGlyphWeightMinSize=0;
    face->fetstate.iGlyphWeightMaxSize= 0;
    face->fetstate.iGlyphWeightMinWeightX = 0;
    face->fetstate.iGlyphWeightMaxWeightX=0;

    face->fetstate.iGammaCorrectionMode= SF_GAMMA_TABLE_NONE;
    face->fetstate.iGammeTableFeature= SF_GAMMA_TABLE_NONE;
    face->fetstate.blackGammaTable=NULL;
    face->fetstate.whiteGammaTable=NULL;
}
