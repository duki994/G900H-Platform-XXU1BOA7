#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_IMAGE_H
#include FT_INTERNAL_OBJECTS_H
#include "sfenhancer.h"
#include <math.h>
//#include FT_CONFIG_STANDARD_LIBRARY_H
#define BLACK_LUMINANCE_LIMIT   0x40
#define WHITE_LUMINANCE_LIMIT   0xA0

FT_EXPORT_DEF( void )
    SF_Build_GammaTable( unsigned char   *dstTable, 
                         int coeff ) {
    unsigned int i;
    float v;
    float gamma;

    for (i = 0; i < GAMMA_GREY_SIZE; i++) {
        dstTable[i] = i;

        v = i / GAMMA_MAX_VALUE;
        gamma = pow(v, coeff/100.0f);

        dstTable[i] = (unsigned char)(floor(gamma * 255.0f + 0.5f));
    }
}



FT_EXPORT_DEF( void )
    SF_GammaCorrect( unsigned char*       bitmap, 
                             int                  width,
                             int                  height,
                             int                  rowBytes, // 
                             const unsigned char* table )
{
    int y;
    int x;

    unsigned char* dst = bitmap;

    if (table != NULL) {
        for (y = height - 1; y >= 0; --y) {
            for (x = width - 1; x >= 0; --x) {
                dst[x] = table[dst[x]];
            }
            dst += rowBytes;
        }
    }
}


const unsigned char *SF_Get_GammaTable( FT_Face face ) 
{
    switch (face->fetstate.iGammeTableFeature) {
    case SF_GAMMA_TABLE_BLACK:
        return face->fetstate.blackGammaTable;
    case SF_GAMMA_TABLE_WHITE:
        return face->fetstate.whiteGammaTable;
        break;
    case SF_GAMMA_TABLE_GREY:
    case SF_GAMMA_TABLE_NONE:
    default:
        return NULL;
    }
}


FT_EXPORT_DEF( void )
    SF_Apply_GammaCorrection( unsigned char*       bitmap, 
                              int                  width,
                              int                  height,
                              int                  rowBytes,
                              FT_Face              face ) 
{
    const unsigned char *table;

    table = SF_Get_GammaTable(face);

    if ( face != NULL &&          
        (width > 0 && height > 0) ) {
            switch ( face->fetstate.iGammaCorrectionMode ) {
            case SF_GAMMA_CORRECTION_COEFF :
            case SF_GAMMA_CORRECTION_TABLE :
                SF_GammaCorrect( 
                    bitmap, 
                    width, 
                    height,
                    rowBytes,
                    table);
            default:
                break;
            }

    }
}
