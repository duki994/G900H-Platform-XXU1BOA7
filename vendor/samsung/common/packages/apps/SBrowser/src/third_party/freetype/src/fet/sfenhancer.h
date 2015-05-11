/********************************************************************
    created:    2013/04/02
    created:    2:4:2013    14:01
    filename:   freetype\src\fet\sfenhancer.h
    file path:  freetype\src\fet
    file base:  sfenhancer
    file ext:   h
    author:   DMC R&D Graphics Lab
*********************************************************************/


#ifndef __SFENHANCER_H__
#define __SFENHANCER_H__
//#include <ft2build.h>
#include "sftypes.h"

FT_BEGIN_HEADER


/********************************************************************/
/* <Function>                                                       */
/*    SF_Build_GammaTable                                           */
/* <Description>                                                    */
/*    Builds  gamma table using given coefficient                   */
/*******************************************************************/
FT_EXPORT_DEF( void )
    SF_Build_GammaTable( unsigned char   *dstTable, 
                         int                 coeff );

/********************************************************************/
/* <Function>                                                       */
/*    SF_Apply_GammaCorrection                                      */
/* <Description>                                                    */
/*    Applies gamma tables to given raw glyph                       */
/*******************************************************************/
FT_EXPORT_DEF( void )
    SF_Apply_GammaCorrection( unsigned char*       bitmap, 
                              int                   width,
                              int                  height,
                              int                rowBytes,
                              FT_Face                face );


FT_END_HEADER

#endif /* __SFENHANCER_H__ */

    /* END */
