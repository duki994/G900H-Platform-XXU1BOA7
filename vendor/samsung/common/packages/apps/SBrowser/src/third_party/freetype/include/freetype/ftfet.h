/********************************************************************
    created:    2013/04/02
    created:    2:4:2013    21:02
    filename:   freetype\include\freetype\ftfet.h
    file path:  freetype\include\freetype
    file base:  ftfet
    file ext:   h
    author:   DMC R&D Graphics Lab
*********************************************************************/


#ifndef __FTFET_H__
#define __FTFET_H__

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef FREETYPE_H
#error "freetype.h of FreeType 1 has been loaded!"
#error "Please fix the directory search order for header files"
#error "so that freetype.h of FreeType 2 is found first."
#endif

FT_BEGIN_HEADER

/*
 * Calculates glyph weight coefficient 
 */
FT_EXPORT_DEF( void )
    SF_Calculate_Gradient( FT_Face  face);

/*
 * Applies glyph weight extension
 */ 
FT_EXPORT_DEF( void )
    SF_Apply_GlyphWeightExtension( FT_Face  face );

/*
 * Applies hybrid hinting using given glyph index and load flag
 */
FT_EXPORT_DEF( void )
    SF_Apply_HybridHinting( FT_Face     face,
                            FT_UInt     glyph_index,
                            FT_Int32    *load_flags );

/*
 * Gets hybrid hinting metrics using given glyph index and load flag
*/
FT_EXPORT_DEF( FT_Error )
    SF_Get_HybridHinting_Metrics( FT_Face    face,
                                  FT_UInt    gindex,
                                  FT_Int32   flags,
                                  FT_BBox   *bbox,
                                  FT_Bool    isEmbolden);

/*
 * Determines whether  using hinting instructions or not
 */ 
FT_EXPORT_DEF(FT_Bool) 
    SF_Ignore_HintingInstruction(FT_Int32     size, 
                                                   const char * family);

/*
 * Translates outline  for sub-pixel positioning 
 */ 
FT_EXPORT_DEF( void )
    SF_Outline_Translate_With_Reshaper( FT_Face            face,
                                        FT_BBox           *bbox,
                                        const FT_Outline*  outline,
                                        FT_Int32           glyphSubFixedX,
                                        FT_Int32           glyphSubFixedY );

/*
* Gets glyph Unicode  value using given glyph id
*/
FT_EXPORT_DEF( FT_ULong )
    SF_GetUnicodeGlyphindex( FT_Face   face, FT_UInt   glyphindex);

/*
* Builds  gamma table using given coefficient
*/
FT_EXPORT_DEF( void )
    SF_Build_GammaTable( unsigned char   *dstTable, 
                         int                 coeff );


/*
 * Applies gamma tables to given raw glyph
*/
FT_EXPORT_DEF( void )
    SF_Apply_GammaCorrection( unsigned char*       bitmap, 
                              int                  width,
                              int                  height,
                              int                  rowBytes,
                              FT_Face              face );


FT_END_HEADER

#endif /* __FTFET_H__ */


/* END */
