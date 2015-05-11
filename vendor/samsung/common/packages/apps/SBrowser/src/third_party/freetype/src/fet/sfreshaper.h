/********************************************************************
    created:    2013/04/02
    created:    2:4:2013    11:28
    filename:   freetype\src\fet\sfreshaper.h
    file path:  freetype\src\fet
    file base:  sfreshaper
    file ext:   h
*********************************************************************/

#ifndef __SFRESHAPER_H__
#define __SFRESHAPER_H__

#include "sftypes.h"

FT_BEGIN_HEADER

/*******************************************************************/
/* <Function>                                                      */
/*   SF_Calculate_Gradient                                         */
/*                                                                 */
/* <Description>                                                   */
/*  Calculates glyph weight coefficient for linear interpolation   */
/*                                                                 */
/*******************************************************************/
FT_EXPORT_DEF( void )
    SF_Calculate_Gradient( FT_Face  face);

/********************************************************************
/* <Function>                                                      */
/*    SF_EmbodleXY_Fixed                                           */
/* <Description>                                                   */
/*   Calls glyph embolden function with fixed weight               */
/*******************************************************************/
FT_EXPORT( void )
    SF_EmbodleXY_Fixed( FT_Face  face);



/*******************************************************************/
/* <Function>                                                      */
/*     SF_EmbodleXY_Saturation                                     */
/* <Description>                                                   */
/*     Calls glyph embolden function with saturation weight        */
/*******************************************************************/
FT_EXPORT_DEF( void )
    SF_EmbodleXY_Saturation( FT_Face  face);


/********************************************************************
/* <Function>                                                      */
/*    SF_Apply_GlyphWeightExtension                                */
/* <Description>                                                   */
/*    Applies hybrid hinting using given glyph index and load flag */
/*******************************************************************/
FT_EXPORT_DEF( void )
    SF_Apply_GlyphWeightExtension( FT_Face  face ); 


/*******************************************************************/
/* <Function>                                                      */
/*    SF_Apply_HybridHinting                                       */
/* <Description>                                                   */
/*    Applies hybrid hinting using given glyph index and load flag */
/***************************** *************************************/
FT_EXPORT_DEF( void )
    SF_Apply_HybridHinting( FT_Face     face,
                            FT_UInt     glyph_index,
                            FT_Int32    *load_flags );

/********************************************************************/
/* <Function>                                                       */
/*    SF_Get_HybridHinting_Metrics                                  */
/* <Description>                                                    */
/*    Gets hybrid hinting metrics using given glyph index           */
/*    and load flag                                                 */
/********************************************************************/
FT_EXPORT_DEF( FT_Error )
    SF_Get_HybridHinting_Metrics( FT_Face    face,
                                  FT_UInt    gindex,
                                  FT_Int32   flags,
                                  FT_BBox   *bbox,
                                  FT_Bool    isEmbolden);


/********************************************************************/
/* <Function>                                                       */
/*   SF_Ignore_HintingInstruction                                   */
/* <Description>                                                    */
/*    Determines whether  using hinting instructions or not         */
/*******************************************************************/
FT_EXPORT_DEF(FT_Bool) SF_Ignore_HintingInstruction(FT_Int32     size, 
                                                   const char * family);

/********************************************************************/
/* <Function>                                                       */
/*   SF_Outline_Translate_With_Reshaper                             */
/* <Description>                                                    */
/*   Translates outline  for sub-pixel positioning                  */
/********************************************************************/
FT_EXPORT_DEF( void )
    SF_Outline_Translate_With_Reshaper( FT_Face            face,
                                        FT_BBox           *bbox,
                                        const FT_Outline*  outline,
                                        FT_Int32           glyphSubFixedX,
                                        FT_Int32           glyphSubFixedY );


/********************************************************************/
/* <Function>                                                       */
/*   SF_GetUnicodeGlyphindex                                        */
/* <Description>                                                    */
/*   Gets glyph Unicode value using given glyph id                  */
/********************************************************************/
FT_EXPORT_DEF( FT_ULong )
    SF_GetUnicodeGlyphindex( FT_Face   face, FT_UInt   glyphindex);

FT_END_HEADER

#endif /* __SFRESHAPER_H__ */

/* END */
