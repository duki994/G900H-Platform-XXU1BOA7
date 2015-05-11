#include <ft2build.h>
// Added by DMC Graphis Lab for FSN_FONT_DEBUG

#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_ADVANCES_H
#include FT_INTERNAL_OBJECTS_H
#include "sfreshaper.h"

#include <android/log.h>
#define FSNLOG(...) __android_log_print(ANDROID_LOG_DEBUG, "FSN_FONTSOLUTION", __VA_ARGS__);


FT_EXPORT_DEF( void )    
    SF_Calculate_Gradient( FT_Face  face) {        

    float sizeDiff = face->fetstate.iGlyphWeightMaxSize - face->fetstate.iGlyphWeightMinSize;
    sizeDiff = (sizeDiff == 0) ? 1: sizeDiff;

    face->fetstate.fGlyphWeightGradientX = 
        ((face->fetstate.iGlyphWeightMaxWeightX * 0.01f) - (face->fetstate.iGlyphWeightMinWeightX * 0.01f)) / sizeDiff;

    face->fetstate.fGlyphWeightGradientY = 
        ((face->fetstate.iGlyphWeightMaxWeightY * 0.01f) - (face->fetstate.iGlyphWeightMinWeightY * 0.01f)) / sizeDiff;
   
}


FT_EXPORT_DEF( void )
    SF_EmbodleXY_Fixed( FT_Face  face)
{
    FT_Outline* outline = &face->glyph->outline;    
    //  Adaptive Extended Font Weight
    FT_Pos x_strength = 0.0f;
    FT_Pos y_strength = 0.0f;
    float weightX = face->fetstate.iGlyphWeightMaxWeightX * 0.01f;
    float weightY = face->fetstate.iGlyphWeightMaxWeightY * 0.01f;

    x_strength = (FT_MulFix(face->units_per_EM, face->size->metrics.x_scale) * weightX) / 100;
    y_strength = (FT_MulFix(face->units_per_EM, face->size->metrics.y_scale) * weightY) / 100;
    if (x_strength != 0 || y_strength != 0) {
        FT_Outline_EmboldenXY(outline, x_strength, y_strength);
    }
}




FT_EXPORT_DEF( void )
    SF_EmbodleXY_Saturation( FT_Face  face)
{
    FT_Outline* outline = &face->glyph->outline;    
    FT_Pos x_strength = 0.0f;
    FT_Pos y_strength = 0.0f;
    float weightX = 0.01f;
    float weightY = 0.01f;


    // Min
    if ( face->size->metrics.y_ppem <= face->fetstate.iGlyphWeightMinSize) {
        weightX = face->fetstate.iGlyphWeightMinWeightX * 0.01f;
        weightY = face->fetstate.iGlyphWeightMinWeightY * 0.01f;
    }
    // Max
    else if ( face->size->metrics.y_ppem >= face->fetstate.iGlyphWeightMaxSize) {
        weightX = face->fetstate.iGlyphWeightMaxWeightX * 0.01f;
        weightY = face->fetstate.iGlyphWeightMaxWeightY * 0.01f;
    }
    // Adaptive Interpolation
    else {      
        weightX = (face->size->metrics.y_ppem - face->fetstate.iGlyphWeightMinSize) * 
            face->fetstate.fGlyphWeightGradientX + face->fetstate.iGlyphWeightMinWeightX * 0.01f;
        weightY = (face->size->metrics.y_ppem - face->fetstate.iGlyphWeightMinSize) * 
            face->fetstate.fGlyphWeightGradientY + face->fetstate.iGlyphWeightMinWeightY * 0.01f;
    }

    x_strength = (FT_MulFix(face->units_per_EM, face->size->metrics.x_scale)*weightX)/100;
    y_strength = (FT_MulFix(face->units_per_EM, face->size->metrics.y_scale)*weightY)/100;
 
    if (x_strength != 0 || y_strength != 0) {
        FT_Outline_EmboldenXY(outline, x_strength, y_strength);
    }
}




FT_EXPORT_DEF( void )
    SF_Apply_GlyphWeightExtension( FT_Face  face ) 
{
    if ( face != NULL ) {
        switch ( face->fetstate.iGlyphWeightExtensionMode ) {                                         
        case SF_GLYPH_WEIGHT_FIXED :
            SF_EmbodleXY_Fixed(face);
            break;
        case SF_GLYPH_WEIGHT_SATURATION :
            SF_EmbodleXY_Saturation(face);
            break;
        case SF_GLYPH_WEIGHT_NONE :
        default:
            break;
        }
    }
}


FT_EXPORT_DEF( FT_Error )
    SF_Get_HybridHinting_Metrics( FT_Face    face,
                                  FT_UInt    gindex,
                                  FT_Int32   flags,
                                  FT_BBox   *bbox,
                                  FT_Bool    isEmbolden)
{
    if ( !face )
        return FT_Err_Invalid_Face_Handle;

    if ( gindex >= (FT_UInt)face->num_glyphs )
        return FT_Err_Invalid_Glyph_Index;

    if ( face->fetstate.iHybridHintMode != SF_HYBRID_HINTING_NONE ) {
        FT_Error    err;
        err = FT_Load_Glyph( face, gindex, flags);

        if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) 
            return err;

        if (face->glyph->outline.n_contours != 0) {

            // **** Fake Bold **** 
            if (isEmbolden && !(face->style_flags & FT_STYLE_FLAG_BOLD)) {
                FT_Pos strength;
                strength = FT_MulFix(face->units_per_EM, face->size->metrics.y_scale)
                    / 34; // SK_OUTLINE_EMBOLDEN_DIVISOR
                FT_Outline_Embolden(&face->glyph->outline, strength);
            }


            // **** Get BBox ****
            FT_Outline_Get_CBox(&face->glyph->outline, bbox);
            // outset the box to integral boundaries        
            bbox->xMin &= ~63;
            bbox->yMin &= ~63;
            bbox->xMax  = (bbox->xMax + 63) & ~63;
            bbox->yMax  = (bbox->yMax + 63) & ~63;
        }
        else {
            bbox->xMin  = 0;
            bbox->yMin  = 0;
            bbox->xMax  = 0;
            bbox->yMax  = 0;
        }
        return err;
    }
    return FT_Err_Invalid_Handle;
}

FT_EXPORT_DEF( void )
    SF_Outline_Translate_With_Reshaper( FT_Face            face,
                                        FT_BBox           *bbox,
                                        const FT_Outline*  outline,
                                        FT_Int32           glyphSubFixedX,
                                        FT_Int32           glyphSubFixedY )
{
    if ((face->glyph->control_len > 0) &&
        (face->fetstate.iHybridHintMode != SF_HYBRID_HINTING_NONE)) {
            glyphSubFixedX = 0;
            glyphSubFixedY = 0;
    }
     
    FT_Outline_Translate(outline, glyphSubFixedX - ((bbox->xMin + glyphSubFixedX) & ~63),
                                    glyphSubFixedY - ((bbox->yMin + glyphSubFixedY) & ~63));
}



static void
    sf_glyphslot_clear( FT_GlyphSlot  slot )
{
    /* free bitmap if needed */
    ft_glyphslot_free_bitmap( slot );

    /* clear all public fields in the glyph slot */
    FT_ZERO( &slot->metrics );
    FT_ZERO( &slot->outline );

    slot->bitmap.width      = 0;
    slot->bitmap.rows       = 0;
    slot->bitmap.pitch      = 0;
    slot->bitmap.pixel_mode = 0;
    /* `slot->bitmap.buffer' has been handled by ft_glyphslot_free_bitmap */

    slot->bitmap_left   = 0;
    slot->bitmap_top    = 0;
    slot->num_subglyphs = 0;
    slot->subglyphs     = 0;
    slot->control_data  = 0;
    slot->control_len   = 0;
    slot->other         = 0;
    slot->format        = FT_GLYPH_FORMAT_NONE;

    slot->linearHoriAdvance = 0;
    slot->linearVertAdvance = 0;
    slot->lsb_delta         = 0;
    slot->rsb_delta         = 0;
}


FT_EXPORT_DEF( void )
    SF_Apply_HybridHinting( FT_Face     face,
                            FT_UInt     glyph_index,
                            FT_Int32    *load_flags )
{
    FT_Error      error;
    FT_Driver     driver;
    FT_GlyphSlot  slot;
    FT_Int32 original_flags = *load_flags;



    driver  = face->driver;
    slot = face->glyph;
    sf_glyphslot_clear( slot );

    *load_flags &= ~(FT_LOAD_TARGET_LIGHT | FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT );
    *load_flags |= FT_LOAD_NO_AUTOHINT;
    // Test
    error = driver->clazz->load_glyph( slot,
        face->size,
        glyph_index,
        *load_flags );

    if(!error) {
        if(face->glyph->control_len <=0) {
            *load_flags &= ~FT_LOAD_NO_AUTOHINT;
            switch ( face->fetstate.iHybridHintMode )
            {
            case SF_HYBRID_HINTING_LIGHT:
                *load_flags |= FT_LOAD_TARGET_LIGHT;
                break;

            case SF_HYBRID_HINTING_NORMAL:
                *load_flags |= FT_LOAD_TARGET_NORMAL;
                break; 
            default:    // NONE
                *load_flags = original_flags;
                break;
            }
        }

        sf_glyphslot_clear( slot );
    }

}


FT_EXPORT_DEF(FT_Bool) SF_Ignore_HintingInstruction(FT_Int32 size, const char * family)
{
    if (((strcmp(family, "Droid Sans Fallback") == 0) || (strcmp(family, "SamsungKorean") == 0) ) && 
        ((size < HYBRID_HINTING_START_SIZE) || (size > HYBRID_HINTING_END_SIZE))) {
            return 1;
    }
    return 0;
}


FT_EXPORT_DEF( FT_ULong )
    SF_GetUnicodeGlyphindex( FT_Face   face, FT_UInt   glyphindex)
{

    if(face->fetstate.glyph_unicodes == NULL) {
        return 0;
    }
    return face->fetstate.glyph_unicodes[glyphindex];

}
