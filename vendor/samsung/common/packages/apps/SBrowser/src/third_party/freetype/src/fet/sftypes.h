/********************************************************************
    created:    2013/04/02
    created:    2:4:2013    13:30
    filename:   freetype\src\fet\sftypes.h
    file path:  freetype\src\fet
    file base:  sftypes
    file ext:   h
    author:     Jinsu Shin (Font / Graphics Lab./ DMC R&D Center)

    purpose:    
........Added by DMC Graphis Lab for Samsung Font Engine Extension
*********************************************************************/


#ifndef __SFTYPES_H__
#define __SFTYPES_H__

#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_INTERNAL_OBJECTS_H
#include FT_INTERNAL_DEBUG_H

FT_BEGIN_HEADER


#ifndef GAMMA_GREY_SIZE
#define GAMMA_GREY_SIZE 256
#define GAMMA_MAX_VALUE 255.0f
#endif

#include FT_CONFIG_STANDARD_LIBRARY_H

#if defined(FSN_FONT_SOLUTION)
#define HYBRID_HINTING_START_SIZE  9
#define HYBRID_HINTING_END_SIZE    20
#endif


typedef struct _SF_CONV_MATRIX {
   int iElem[9];
} SF_CONV_MATRIX;


FT_END_HEADER


#endif /* __SFTYPES_H__ */


/* END */
