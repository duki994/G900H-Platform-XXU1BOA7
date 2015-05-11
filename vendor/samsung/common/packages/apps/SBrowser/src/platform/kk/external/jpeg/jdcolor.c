/*
 * jdcolor.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains output colorspace conversion routines.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#ifdef NV_ARM_NEON
#include "jsimd_neon.h"
#endif
#ifdef __ARM_HAVE_NEON
#include "arm_neon.h"
#endif

#ifdef SIMD_16BIT
#define SIMD_OPT
#endif

#ifdef SIMD_32BIT
#define SIMD_OPT
#endif

/* Private subobject */

typedef struct {
  struct jpeg_color_deconverter pub; /* public fields */

  /* Private state for YCC->RGB conversion */
  int * Cr_r_tab;		/* => table for Cr to R conversion */
  int * Cb_b_tab;		/* => table for Cb to B conversion */
  INT32 * Cr_g_tab;		/* => table for Cr to G conversion */
  INT32 * Cb_g_tab;		/* => table for Cb to G conversion */
} my_color_deconverter;

typedef my_color_deconverter * my_cconvert_ptr;


#ifdef ANDROID_RGB

/* Declarations for ordered dithering.
 * 
 * We use 4x4 ordered dither array packed into 32 bits. This array is
 * sufficent for dithering RGB_888 to RGB_565.
 */

#define DITHER_MASK         0x3
#define DITHER_ROTATE(x)    (((x)<<24) | (((x)>>8)&0x00FFFFFF))
static const INT32 dither_matrix[4] = {
  0x0008020A,
  0x0C040E06,
  0x030B0109,
  0x0F070D05
};

#endif


/**************** YCbCr -> RGB conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *	R = Y                + 1.40200 * Cr
 *	G = Y - 0.34414 * Cb - 0.71414 * Cr
 *	B = Y + 1.77200 * Cb
 * where Cb and Cr represent the incoming values less CENTERJSAMPLE.
 * (These numbers are derived from TIFF 6.0 section 21, dated 3-June-92.)
 *
 * To avoid floating-point arithmetic, we represent the fractional constants
 * as integers scaled up by 2^16 (about 4 digits precision); we have to divide
 * the products by 2^16, with appropriate rounding, to get the correct answer.
 * Notice that Y, being an integral input, does not contribute any fraction
 * so it need not participate in the rounding.
 *
 * For even more speed, we avoid doing any multiplications in the inner loop
 * by precalculating the constants times Cb and Cr for all possible values.
 * For 8-bit JSAMPLEs this is very reasonable (only 256 entries per table);
 * for 12-bit samples it is still acceptable.  It's not very reasonable for
 * 16-bit samples, but if you want lossless storage you shouldn't be changing
 * colorspace anyway.
 * The Cr=>R and Cb=>B values can be rounded to integers in advance; the
 * values for the G calculation are left scaled up, since we must add them
 * together before rounding.
 */

#define SCALEBITS	16	/* speediest right-shift on some machines */
#define ONE_HALF	((INT32) 1 << (SCALEBITS-1))
#define FIX(x)		((INT32) ((x) * (1L<<SCALEBITS) + 0.5))

extern void
ycc_rgba_8888_convert_sub_16bit(j_decompress_ptr,
				JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);
extern void
ycc_rgb_565_convert_sub_16bit(j_decompress_ptr cinfo,
			      JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);

/*
 * Initialize tables for YCC->RGB colorspace conversion.
 */

LOCAL(void)
build_ycc_rgb_table (j_decompress_ptr cinfo)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  int i;
  INT32 x;
  SHIFT_TEMPS

  cconvert->Cr_r_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                (MAXJSAMPLE+1) * SIZEOF(int));
  cconvert->Cb_b_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                (MAXJSAMPLE+1) * SIZEOF(int));
  cconvert->Cr_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                (MAXJSAMPLE+1) * SIZEOF(INT32));
  cconvert->Cb_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                (MAXJSAMPLE+1) * SIZEOF(INT32));

  for (i = 0, x = -CENTERJSAMPLE; i <= MAXJSAMPLE; i++, x++) {
    /* i is the actual input pixel value, in the range 0..MAXJSAMPLE */
    /* The Cb or Cr value we are thinking of is x = i - CENTERJSAMPLE */
    /* Cr=>R value is nearest int to 1.40200 * x */
    cconvert->Cr_r_tab[i] = (int)
                    RIGHT_SHIFT(FIX(1.40200) * x + ONE_HALF, SCALEBITS);
    /* Cb=>B value is nearest int to 1.77200 * x */
    cconvert->Cb_b_tab[i] = (int)
                    RIGHT_SHIFT(FIX(1.77200) * x + ONE_HALF, SCALEBITS);
    /* Cr=>G value is scaled-up -0.71414 * x */
    cconvert->Cr_g_tab[i] = (- FIX(0.71414)) * x;
    /* Cb=>G value is scaled-up -0.34414 * x */
    /* We also add in ONE_HALF so that need not do it in inner loop */
    cconvert->Cb_g_tab[i] = (- FIX(0.34414)) * x + ONE_HALF;
  }
}

/*
 * Convert some rows of samples to the output colorspace.
 *
 * Note that we change from noninterleaved, one-plane-per-component format
 * to interleaved-pixel format.  The output buffer is therefore three times
 * as wide as the input buffer.
 * A starting row offset is provided only for the input buffer.  The caller
 * can easily adjust the passed output_buf value to accommodate any row
 * offset required on that side.
 */

METHODDEF(void)
ycc_rgb_convert (j_decompress_ptr cinfo,
		 JSAMPIMAGE input_buf, JDIMENSION input_row,
		 JSAMPARRAY output_buf, int num_rows)
{

#ifdef SIMD_16BIT // sub function call

  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  register int * Crrtab = cconvert->Cr_r_tab;
  register int * Cbbtab = cconvert->Cb_b_tab;
  register INT32 * Crgtab = cconvert->Cr_g_tab;
  register INT32 * Cbgtab = cconvert->Cb_g_tab;
  uint8x8_t vdu8_b,vdu8_g,vdu8_r;
  const short constVal[4] = { 227, 44, 179, 91};
  SHIFT_TEMPS
    asm volatile(
         "VMOV.I16 q3, #0x80  \n\t"
          "MOV r0, %[constVal]  \n\t"
          "VLD1.16 {d30}, [r0]  \n\t"
          "VDUP.16 q0, d30[0]   \n\t"
          "VDUP.16 q1, d30[1]   \n\t"
          "VDUP.16 q2, d30[2]  \n\t"
          "VDUP.16 q5, d30[3]  \n\t"
          "VSHR.S16 q4,q3,#1   \n\t"

          ::[constVal] "r" (constVal)
        : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15",
        "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23", "d30", "d31", "r0"
          );


  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;

    for (col = 0; col < (num_cols & 0xfffffff8); col+=8) {
        asm volatile (

            "ADD r0, %[inptr0], %[col]    \n\t"
            "VLD1.8 {d12}, [r0]         \n\t"
            "ADD r0, %[inptr1], %[col]    \n\t"
            "VLD1.8 {d13}, [r0]         \n\t"
            "ADD r0, %[inptr2], %[col]    \n\t"
            "VLD1.8 {d14}, [r0]         \n\t"
            "VMOVL.U8 q8,d12            \n\t"
            "VMOVL.U8 q9,d13            \n\t"
            "VMOVL.U8 q10,d14           \n\t"
            "VSUB.I16 q11,q9,q3         \n\t"
            "VSUB.I16 q12,q10,q3         \n\t"

            "VMOV     q13,q4             \n\t"
            "VMLA.I16 q13,q11,q0         \n\t"
            "VSHR.S16 q13,q13,#7         \n\t"
            "VADD.I16 q13,q13,q8         \n\t"
            "VQMOVUN.S16 d14,q13         \n\t"

            "VMOV     q13,q4             \n\t"
            "VMLA.I16 q13,q11,q1         \n\t"

            "VMLA.I16 q13,q12,q5         \n\t"

            "VSHR.S16 q13,q13,#7         \n\t"

            "VSUB.I16 q13,q8,q13         \n\t"
            "VQMOVUN.S16 d13,q13         \n\t"

            "VMOV     q13,q4             \n\t"
            "VMLA.I16 q13,q12,q2         \n\t"
            "VSHR.S16 q13,q13,#7         \n\t"
            "VADD.I16 q13,q13,q8         \n\t"
            "VQMOVUN.S16 d12,q13         \n\t"

            "MOV    r0, %[outptr]        \n\t"
            "VST3.8 {d12, d13, d14},[r0] \n\t"
            ::[inptr0] "r" (inptr0), [inptr1] "r" (inptr1), [inptr2] "r" (inptr2), [col] "r" (col), [outptr] "r" (outptr)
            : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
            "s17", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
            "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "r0"
            );

            outptr += RGB_PIXELSIZE * 8;
    }

    for (; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[RGB_RED] =   range_limit[y + Crrtab[cr]];
      outptr[RGB_GREEN] = range_limit[y +
                  ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
                         SCALEBITS))];
      outptr[RGB_BLUE] =  range_limit[y + Cbbtab[cb]];
      outptr += RGB_PIXELSIZE;
    }
  }

#elif defined SIMD_32BIT        /*SIMD_32BIT*/
    my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
    register int y, cb, cr;
    register JSAMPROW outptr;
    register JSAMPROW inptr0, inptr1, inptr2;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    register int * Crrtab = cconvert->Cr_r_tab;
    register int * Cbbtab = cconvert->Cb_b_tab;
    register INT32 * Crgtab = cconvert->Cr_g_tab;
    register INT32 * Cbgtab = cconvert->Cb_g_tab;

    const int constVal[4] = { 116130, 91881, -22554, -46802};
     SHIFT_TEMPS

     asm volatile(
        "VMOV.I16 d18, #0x80         \n\t"
        "VMOV.I32 q0, #0x8000         \n\t"
        "MOV r0, %[constVal]        \n\t"
        "VLD1.32 {q4}, [r0]            \n\t"
        "VDUP.32 q1, d8[0]            \n\t"
        "VDUP.32 q8, d8[1]            \n\t"
        "VDUP.32 q2, d9[0]            \n\t"
        "VDUP.32 q3, d9[1]            \n\t"
        ::[constVal] "r" (constVal)
        : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
        "s17", "s18", "s19", "d16", "d17", "d18", "d19", "r0"
        );

    while (--num_rows >= 0) {
        inptr0 = input_buf[0][input_row];
        inptr1 = input_buf[1][input_row];
        inptr2 = input_buf[2][input_row];
        input_row++;
        outptr = *output_buf++;

        for (col = 0; col < (num_cols & 0xfffffff8); col+=8) {
            asm volatile(
            "ADD r0, %[inptr0], %[col]    \n\t"
            "VLD1.8 {d19}, [r0]         \n\t"
            "ADD r0, %[inptr1], %[col]    \n\t"
            "VLD1.8 {d20}, [r0]         \n\t"
            "VMOVL.U8 q10, d20            \n\t"
            "ADD r0, %[inptr2], %[col]    \n\t"
            "VLD1.8 {d22}, [r0]         \n\t"
            "VMOVL.U8 q11, d22            \n\t"
            "VSUBL.S16 q12, d20, d18    \n\t"
            "VSUBL.S16 q15, d22, d18    \n\t"
            "VMUL.I32 q13, q12, q1        \n\t"
            "VMUL.I32 q14, q15, q8        \n\t"
            "VADD.I32 q13, q13, q0        \n\t"
            "VSUBL.S16 q10, d21, d18    \n\t"
            "VADD.I32 q14, q14, q0        \n\t"
            "VSHRN.I32 d27, q13, #16     \n\t"
            "VSHRN.I32 d26, q14, #16     \n\t"
            "VMUL.I32 q14, q10, q1        \n\t"
            "VMUL.I32 q12, q12, q2        \n\t"
            "VMUL.I32 q10, q10, q2        \n\t"
            "VSUBL.S16 q11, d23, d18    \n\t"
            "VADD.I32 q12, q12, q0        \n\t"
            "VMLA.I32 q12, q15, q3        \n\t"
            "VADD.I32 q10, q10, q0        \n\t"
            "VADD.I32 q14, q14, q0        \n\t"
            "VMLA.I32 q10, q11, q3        \n\t"
            "VMUL.I32 q15, q11, q8        \n\t"
            "VSHRN.I32 d28, q14, #16     \n\t"
            "VSHRN.I32 d29, q10, #16     \n\t"
            "VMOV d9,d28                \n\t"
            "VADD.I32 q15, q15, q0        \n\t"
            "VMOVL.U8 q10,d19            \n\t"
            "VMOV d8, d27                \n\t"
            "VSHRN.I32 d24, q12, #16    \n\t"
            "VADD.I16 q11, q4, q10        \n\t"
            "VSHRN.I32 d25, q15, #16    \n\t"
            "VMOV d8, d26                \n\t"
            "VMOV d9, d25                \n\t"
            "VQMOVUN.S16 d10, q11        \n\t"
            "VADD.I16 q11, q4, q10        \n\t"
            "VMOV d28, d24                \n\t"
            "VQMOVUN.S16 d8, q11        \n\t"
            "VADD.I16 q10,q10,q14        \n\t"
            "VQMOVUN.S16 d9, q10        \n\t"
            "MOV    r0, %[outptr]        \n\t"
            "VST3.8 {d8, d9, d10}, [r0] \n\t"
            ::[inptr0] "r" (inptr0), [inptr1] "r" (inptr1), [inptr2] "r" (inptr2), [col] "r" (col), [outptr] "r" (outptr)
            : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
            "s17", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
            "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "r0"
            );

            outptr += RGB_PIXELSIZE * 8;
        }

        for (; col < num_cols; col++) {
            y  = GETJSAMPLE(inptr0[col]);
            cb = GETJSAMPLE(inptr1[col]);
            cr = GETJSAMPLE(inptr2[col]);
            /* Range-limiting is essential due to noise introduced by DCT losses. */
            outptr[RGB_RED] =   range_limit[y + Crrtab[cr]];
            outptr[RGB_GREEN] = range_limit[y +
                ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
                    SCALEBITS))];
            outptr[RGB_BLUE] =  range_limit[y + Cbbtab[cb]];
            outptr += RGB_PIXELSIZE;
        }
    }
#else
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  register int * Crrtab = cconvert->Cr_r_tab;
  register int * Cbbtab = cconvert->Cb_b_tab;
  register INT32 * Crgtab = cconvert->Cr_g_tab;
  register INT32 * Cbgtab = cconvert->Cb_g_tab;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[RGB_RED] =   range_limit[y + Crrtab[cr]];
      outptr[RGB_GREEN] = range_limit[y +
                              ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
                                                 SCALEBITS))];
      outptr[RGB_BLUE] =  range_limit[y + Cbbtab[cb]];
      outptr += RGB_PIXELSIZE;
    }
  }
#endif
}

#ifdef ANDROID_RGB
METHODDEF(void)
ycc_rgba_8888_convert (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
#if defined SIMD_16BIT // sub function call
    ycc_rgba_8888_convert_sub_16bit( cinfo, input_buf, input_row, output_buf, num_rows );
#elif defined SIMD_32BIT
    ycc_rgba_8888_convert_sub_32bit( cinfo, input_buf, input_row, output_buf, num_rows );
#else
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  register int * Crrtab = cconvert->Cr_r_tab;
  register int * Cbbtab = cconvert->Cb_b_tab;
  register INT32 * Crgtab = cconvert->Cr_g_tab;
  register INT32 * Cbgtab = cconvert->Cb_g_tab;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[RGB_RED] =   range_limit[y + Crrtab[cr]];
      outptr[RGB_GREEN] = range_limit[y +
                              ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
                                                 SCALEBITS))];
      outptr[RGB_BLUE] =  range_limit[y + Cbbtab[cb]];
      outptr[RGB_ALPHA] =  0xFF;
      outptr += 4;
    }
  }
#endif
}

METHODDEF(void)
ycc_rgb_565_convert (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
#if defined SIMD_16BIT // sub function call
    ycc_rgb_565_convert_sub_16bit( cinfo, input_buf, input_row, output_buf, num_rows );
#elif defined SIMD_32BIT
    ycc_rgb_565_convert_sub_32bit( cinfo, input_buf, input_row, output_buf, num_rows );
#else
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  register int * Crrtab = cconvert->Cr_r_tab;
  register int * Cbbtab = cconvert->Cb_b_tab;
  register INT32 * Crgtab = cconvert->Cr_g_tab;
  register INT32 * Cbgtab = cconvert->Cb_g_tab;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    INT32 rgb;
    unsigned int r, g, b;
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    
    if (PACK_NEED_ALIGNMENT(outptr)) {
        y  = GETJSAMPLE(*inptr0++);
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        r = range_limit[y + Crrtab[cr]];
        g = range_limit[y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS))];
        b = range_limit[y + Cbbtab[cb]];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
        outptr += 2;
        num_cols--;
    }
    for (col = 0; col < (num_cols>>1); col++) {
      y  = GETJSAMPLE(*inptr0++);
      cb = GETJSAMPLE(*inptr1++);
      cr = GETJSAMPLE(*inptr2++);
      r = range_limit[y + Crrtab[cr]];
      g = range_limit[y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS))];
      b = range_limit[y + Cbbtab[cb]];
      rgb = PACK_SHORT_565(r,g,b);

      y  = GETJSAMPLE(*inptr0++);
      cb = GETJSAMPLE(*inptr1++);
      cr = GETJSAMPLE(*inptr2++);
      r = range_limit[y + Crrtab[cr]];
      g = range_limit[y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS))];
      b = range_limit[y + Cbbtab[cb]];
      rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
      WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
      outptr += 4;
    }
    if (num_cols&1) {
      y  = GETJSAMPLE(*inptr0);
      cb = GETJSAMPLE(*inptr1);
      cr = GETJSAMPLE(*inptr2);
      r = range_limit[y + Crrtab[cr]];
      g = range_limit[y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS))];
      b = range_limit[y + Cbbtab[cb]];
      rgb = PACK_SHORT_565(r,g,b);
      *(INT16*)outptr = rgb;
    }
  }
#endif
}

METHODDEF(void)
ycc_rgb_565D_convert (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
#if defined SIMD_32BIT // sub function call
    my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
    register int y, cb, cr;
    register JSAMPROW outptr;
    register JSAMPROW inptr0, inptr1, inptr2;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;

    int16_t dither_matrix_neon[4][8] = {{0x0a, 0x02, 0x08, 0x00, 0x0a, 0x02, 0x08, 0x00}
                                    ,{0x06, 0x0e, 0x04, 0x0c, 0x06, 0x0e, 0x04, 0x0c}
                                    ,{0x09, 0x01, 0x0b, 0x03, 0x09, 0x01, 0x0b, 0x03}
                                    ,{0x05, 0x0d, 0x07, 0x0f, 0x05, 0x0d, 0x07, 0x0f}};

    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    register int * Crrtab = cconvert->Cr_r_tab;
    register int * Cbbtab = cconvert->Cb_b_tab;
    register INT32 * Crgtab = cconvert->Cr_g_tab;
    register INT32 * Cbgtab = cconvert->Cb_g_tab;
    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    SHIFT_TEMPS

    int16x8_t vqs16_dither_matrix = vld1q_s16( dither_matrix_neon[0] + ((cinfo->output_scanline % 4) * 8));

    int32x4_t vqs32_1_772, vqs32_0_34414, vqs32_1_402 , vqs32_0_71414, vqs32_half;
    int32x4_t vqs32_tmp, vqs32_tmp2;
    int16x4_t vds16_tmp_low,vds16_tmp_high, vds16_128;
    int16x8_t vqs16_128;

    vqs32_1_772 = vdupq_n_s32(116130);
    vqs32_0_34414 = vdupq_n_s32(-22554);
    vqs32_1_402 = vdupq_n_s32(91881);
    vqs32_0_71414 = vdupq_n_s32(-46802);
    vds16_128 = vmov_n_s16(128);
    vqs32_half = vdupq_n_s32(32768);
    vqs16_128 = vmovq_n_s16(128);

    while (--num_rows >= 0) {
        INT32 rgb;
        unsigned int r, g, b;
        inptr0 = input_buf[0][input_row];
        inptr1 = input_buf[1][input_row];
        inptr2 = input_buf[2][input_row];
        input_row++;
        outptr = *output_buf++;
        if (PACK_NEED_ALIGNMENT(outptr)) {
            y  = GETJSAMPLE(*inptr0++);
            cb = GETJSAMPLE(*inptr1++);
            cr = GETJSAMPLE(*inptr2++);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            rgb = PACK_SHORT_565(r,g,b);
            *(INT16*)outptr = rgb;
            outptr += 2;
            num_cols--;
        }
        for (col = 0; col < (num_cols - (num_cols & 0x7) ); col+=8) {
            uint8x8_t vdu8_y = vld1_u8(inptr0);
            uint8x8_t vdu8_cb = vld1_u8(inptr1);
            uint8x8_t vdu8_cr = vld1_u8(inptr2);

            inptr0+=8;
            inptr1+=8;
            inptr2+=8;

            int16x8_t vqs16_cb = vreinterpretq_s16_u16( vmovl_u8(vdu8_cb) );
            int16x8_t vqs16_cr = vreinterpretq_s16_u16( vmovl_u8(vdu8_cr) );

            vqs16_cb = vsubq_s16(vqs16_cb, vqs16_128);
            vqs16_cr = vsubq_s16(vqs16_cr, vqs16_128);

            int32x4_t vqs32_cb_l = vmovl_s16(vget_low_s16(vqs16_cb));
            int32x4_t vqs32_cr_l = vmovl_s16(vget_low_s16(vqs16_cr));

            int32x4_t vqs32_cb_h = vmovl_s16(vget_high_s16(vqs16_cb));
            int32x4_t vqs32_cr_h = vmovl_s16(vget_high_s16(vqs16_cr));

            //Low_32x4
            int16x4_t vds16_b_low = vshrn_n_s32(vaddq_s32(vmulq_s32(vqs32_cb_l, vqs32_1_772),vqs32_half), 16);
            int16x4_t vds16_r_low = vshrn_n_s32(vaddq_s32(vmulq_s32(vqs32_cr_l, vqs32_1_402),vqs32_half), 16);

            vqs32_tmp = vaddq_s32(vmulq_s32( vqs32_cb_l, vqs32_0_34414),vqs32_half);
            vqs32_tmp2 = vmlaq_s32(vqs32_tmp, vqs32_cr_l, vqs32_0_71414);
            vds16_tmp_low = vshrn_n_s32(vqs32_tmp2, 16);

            //High_32x4
            int16x4_t vds16_b_high = vshrn_n_s32(vaddq_s32(vmulq_s32(vqs32_cb_h, vqs32_1_772),vqs32_half), 16);
            int16x4_t vds16_r_high = vshrn_n_s32(vaddq_s32(vmulq_s32(vqs32_cr_h, vqs32_1_402),vqs32_half), 16);

            vqs32_tmp = vaddq_s32(vmulq_s32( vqs32_cb_h, vqs32_0_34414), vqs32_half);
            vqs32_tmp2 = vmlaq_s32(vqs32_tmp, vqs32_cr_h, vqs32_0_71414);
            vds16_tmp_high = vshrn_n_s32(vqs32_tmp2, 16);

            int16x8_t vqs16_y = vreinterpretq_s16_u16( vmovl_u8(vdu8_y) );

            // dither
            int16x8_t vqs16_y_rb = vaddq_s16(vqs16_y, vqs16_dither_matrix);
            int16x8_t vqs16_y_g = vaddq_s16(vqs16_y, vshrq_n_s16(vqs16_dither_matrix, 1));

            // Check the value (value > 0, value <= 255)
            uint16x8_t vqu16_b = vqshluq_n_s16(vaddq_s16(vcombine_s16(vds16_b_low,vds16_b_high), vqs16_y_rb), 8);
            uint16x8_t vqu16_r = vqshluq_n_s16(vaddq_s16(vcombine_s16(vds16_r_low,vds16_r_high), vqs16_y_rb), 8);

            uint16x8_t vqu16_g = vqshluq_n_s16(vaddq_s16(vqs16_y_g, vcombine_s16(vds16_tmp_low, vds16_tmp_high)), 8);


            // Packing RGB565
            vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
            vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

            // Store RGB565 and Increase Each Destination Pointers
            vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));
            outptr += 2 * 8;
        }
        for (; col < (num_cols&0xfffffffe); col+=2) {
            y  = GETJSAMPLE(*inptr0++);
            cb = GETJSAMPLE(*inptr1++);
            cr = GETJSAMPLE(*inptr2++);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            d0 = DITHER_ROTATE(d0);
            rgb = PACK_SHORT_565(r,g,b);
            y  = GETJSAMPLE(*inptr0++);
            cb = GETJSAMPLE(*inptr1++);
            cr = GETJSAMPLE(*inptr2++);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            d0 = DITHER_ROTATE(d0);
            rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
            WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
            outptr += 4;
        }
        if (num_cols&1) {
            y  = GETJSAMPLE(*inptr0);
            cb = GETJSAMPLE(*inptr1);
            cr = GETJSAMPLE(*inptr2);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            rgb = PACK_SHORT_565(r,g,b);
            *(INT16*)outptr = rgb;
        }
    }
#elif defined SIMD_16BIT
    my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
    register int y, cb, cr;
    register JSAMPROW outptr;
    register JSAMPROW inptr0, inptr1, inptr2;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;

    int16_t dither_matrix_neon[4][8] = {{0x0a, 0x02, 0x08, 0x00, 0x0a, 0x02, 0x08, 0x00}
                                    ,{0x06, 0x0e, 0x04, 0x0c, 0x06, 0x0e, 0x04, 0x0c}
                                    ,{0x09, 0x01, 0x0b, 0x03, 0x09, 0x01, 0x0b, 0x03}
                                    ,{0x05, 0x0d, 0x07, 0x0f, 0x05, 0x0d, 0x07, 0x0f}};

    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    register int * Crrtab = cconvert->Cr_r_tab;
    register int * Cbbtab = cconvert->Cb_b_tab;
    register INT32 * Crgtab = cconvert->Cr_g_tab;
    register INT32 * Cbgtab = cconvert->Cb_g_tab;

    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    SHIFT_TEMPS

    int16x8_t vqs16_dither_matrix = vld1q_s16( dither_matrix_neon[0] + ((cinfo->output_scanline % 4) * 8));

    int16x8_t vqs16_1_772, vqs16_0_34414, vqs16_1_402, vqs16_0_71414, vqs16_one_half, vqs16_128;
    int16x8_t vqs16_tmp, vqs16_tmp2;
    vqs16_1_772 = vdupq_n_s16( 227 );
    vqs16_0_34414 = vdupq_n_s16( 44 );
    vqs16_1_402 = vdupq_n_s16( 179 );
    vqs16_128 = vdupq_n_s16( 128 );
    vqs16_0_71414 = vdupq_n_s16( 91 );
    vqs16_one_half = vmovq_n_s16( 64 );
    uint8x8_t vdu8_128 = vmov_n_u8(128);

    while (--num_rows >= 0) {
        INT32 rgb;
        unsigned int r, g, b;
        inptr0 = input_buf[0][input_row];
        inptr1 = input_buf[1][input_row];
        inptr2 = input_buf[2][input_row];
        input_row++;
        outptr = *output_buf++;
        if (PACK_NEED_ALIGNMENT(outptr)) {
            y  = GETJSAMPLE(*inptr0++);
            cb = GETJSAMPLE(*inptr1++);
            cr = GETJSAMPLE(*inptr2++);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            rgb = PACK_SHORT_565(r,g,b);
            *(INT16*)outptr = rgb;
            outptr += 2;
            num_cols--;
        }
        for (col = 0; col < (num_cols - (num_cols & 0x7) ); col+=8) {
        uint8x8_t vdu8_y = vld1_u8(inptr0);
        uint8x8_t vdu8_cb = vld1_u8(inptr1);
        uint8x8_t vdu8_cr = vld1_u8(inptr2);

        inptr0+=8;
        inptr1+=8;
        inptr2+=8;

        int16x8_t vqs16_y = vreinterpretq_s16_u16(vmovl_u8(vdu8_y));
        int16x8_t vqs16_cb = vreinterpretq_s16_u16(vmovl_u8(vdu8_cb));
        int16x8_t vqs16_cr = vreinterpretq_s16_u16(vmovl_u8(vdu8_cr));

        vqs16_cb = vsubq_s16( vqs16_cb, vqs16_128 );
        vqs16_cr = vsubq_s16( vqs16_cr, vqs16_128 );


        // dither
        int16x8_t vqs16_y_rb = vaddq_s16(vqs16_y, vqs16_dither_matrix);
        int16x8_t vqs16_y_g = vaddq_s16(vqs16_y, vshrq_n_s16(vqs16_dither_matrix, 1));

        uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16(vqs16_one_half, vqs16_1_772, vqs16_cb ), 7), vqs16_y_rb));

        vqs16_tmp = vmlaq_s16( vqs16_one_half, vqs16_0_34414, vqs16_cb );
        vqs16_tmp2 = vmlaq_s16( vqs16_tmp, vqs16_0_71414, vqs16_cr );
        vqs16_tmp = vshrq_n_s16( vqs16_tmp2, 7 );

        uint8x8_t vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y_g, vqs16_tmp));
        uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16( vqs16_one_half, vqs16_1_402, vqs16_cr ), 7), vqs16_y_rb));

        uint16x8_t vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        uint16x8_t vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        uint16x8_t vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr += 2 * 8;
        }
        for (; col < (num_cols&0xfffffffe); col+=2) {
            y  = GETJSAMPLE(*inptr0++);
            cb = GETJSAMPLE(*inptr1++);
            cr = GETJSAMPLE(*inptr2++);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            d0 = DITHER_ROTATE(d0);
            rgb = PACK_SHORT_565(r,g,b);
            y  = GETJSAMPLE(*inptr0++);
            cb = GETJSAMPLE(*inptr1++);
            cr = GETJSAMPLE(*inptr2++);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            d0 = DITHER_ROTATE(d0);
            rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
            WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
            outptr += 4;
        }
        if (num_cols&1) {
            y  = GETJSAMPLE(*inptr0);
            cb = GETJSAMPLE(*inptr1);
            cr = GETJSAMPLE(*inptr2);
            r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
            g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
            b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
            rgb = PACK_SHORT_565(r,g,b);
            *(INT16*)outptr = rgb;
        }
    }
#else
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  register int * Crrtab = cconvert->Cr_r_tab;
  register int * Cbbtab = cconvert->Cb_b_tab;
  register INT32 * Crgtab = cconvert->Cr_g_tab;
  register INT32 * Cbgtab = cconvert->Cb_g_tab;
  INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    INT32 rgb;
    unsigned int r, g, b;
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    if (PACK_NEED_ALIGNMENT(outptr)) {
        y  = GETJSAMPLE(*inptr0++);
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
        g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
        b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
        outptr += 2;
        num_cols--;
    }
    for (col = 0; col < (num_cols>>1); col++) {
      y  = GETJSAMPLE(*inptr0++);
      cb = GETJSAMPLE(*inptr1++);
      cr = GETJSAMPLE(*inptr2++);
      r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
      g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
      b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
      d0 = DITHER_ROTATE(d0);
      rgb = PACK_SHORT_565(r,g,b);
      y  = GETJSAMPLE(*inptr0++);
      cb = GETJSAMPLE(*inptr1++);
      cr = GETJSAMPLE(*inptr2++);
      r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
      g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
      b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
      d0 = DITHER_ROTATE(d0);
      rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
      WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
      outptr += 4;
    }
    if (num_cols&1) {
      y  = GETJSAMPLE(*inptr0);
      cb = GETJSAMPLE(*inptr1);
      cr = GETJSAMPLE(*inptr2);
      r = range_limit[DITHER_565_R(y + Crrtab[cr], d0)];
      g = range_limit[DITHER_565_G(y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS)), d0)];
      b = range_limit[DITHER_565_B(y + Cbbtab[cb], d0)];
      rgb = PACK_SHORT_565(r,g,b);
      *(INT16*)outptr = rgb;
    }
  }
#endif
}

#endif

/**************** Cases other than YCbCr -> RGB(A) **************/

#ifdef ANDROID_RGB
METHODDEF(void)
rgb_rgba_8888_convert (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      *outptr++ = *inptr0++;
      *outptr++ = *inptr1++;
      *outptr++ = *inptr2++;
      *outptr++ = 0xFF;
    }
  }
}

METHODDEF(void)
rgb_rgb_565_convert (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    INT32 rgb;
    unsigned int r, g, b;
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    if (PACK_NEED_ALIGNMENT(outptr)) {
        r = GETJSAMPLE(*inptr0++);
        g = GETJSAMPLE(*inptr1++);
        b = GETJSAMPLE(*inptr2++);
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
        outptr += 2;
        num_cols--;
    }
    for (col = 0; col < (num_cols>>1); col++) {
      r = GETJSAMPLE(*inptr0++);
      g = GETJSAMPLE(*inptr1++);
      b = GETJSAMPLE(*inptr2++);
      rgb = PACK_SHORT_565(r,g,b);
      r = GETJSAMPLE(*inptr0++);
      g = GETJSAMPLE(*inptr1++);
      b = GETJSAMPLE(*inptr2++);
      rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
      WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
      outptr += 4;
    }
    if (num_cols&1) {
      r = GETJSAMPLE(*inptr0);
      g = GETJSAMPLE(*inptr1);
      b = GETJSAMPLE(*inptr2);
      rgb = PACK_SHORT_565(r,g,b);
      *(INT16*)outptr = rgb;
    }
  }
}


METHODDEF(void)
rgb_rgb_565D_convert (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2;
  register JDIMENSION col;
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  JDIMENSION num_cols = cinfo->output_width;
  INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    INT32 rgb;
    unsigned int r, g, b;
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    if (PACK_NEED_ALIGNMENT(outptr)) {
        r = range_limit[DITHER_565_R(GETJSAMPLE(*inptr0++), d0)];
        g = range_limit[DITHER_565_G(GETJSAMPLE(*inptr1++), d0)];
        b = range_limit[DITHER_565_B(GETJSAMPLE(*inptr2++), d0)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
        outptr += 2;
        num_cols--;
    }
    for (col = 0; col < (num_cols>>1); col++) {
      r = range_limit[DITHER_565_R(GETJSAMPLE(*inptr0++), d0)];
      g = range_limit[DITHER_565_G(GETJSAMPLE(*inptr1++), d0)];
      b = range_limit[DITHER_565_B(GETJSAMPLE(*inptr2++), d0)];
      d0 = DITHER_ROTATE(d0);
      rgb = PACK_SHORT_565(r,g,b);
      r = range_limit[DITHER_565_R(GETJSAMPLE(*inptr0++), d0)];
      g = range_limit[DITHER_565_G(GETJSAMPLE(*inptr1++), d0)];
      b = range_limit[DITHER_565_B(GETJSAMPLE(*inptr2++), d0)];
      d0 = DITHER_ROTATE(d0);
      rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
      WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
      outptr += 4;
    }
    if (num_cols&1) {
      r = range_limit[DITHER_565_R(GETJSAMPLE(*inptr0), d0)];
      g = range_limit[DITHER_565_G(GETJSAMPLE(*inptr1), d0)];
      b = range_limit[DITHER_565_B(GETJSAMPLE(*inptr2), d0)];
      rgb = PACK_SHORT_565(r,g,b);
      *(INT16*)outptr = rgb;
    }
  }
}

#endif

/*
 * Color conversion for no colorspace change: just copy the data,
 * converting from separate-planes to interleaved representation.
 */

METHODDEF(void)
null_convert (j_decompress_ptr cinfo,
	      JSAMPIMAGE input_buf, JDIMENSION input_row,
	      JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW inptr, outptr;
  register JDIMENSION count;
  register int num_components = cinfo->num_components;
  JDIMENSION num_cols = cinfo->output_width;
  int ci;

  while (--num_rows >= 0) {
    for (ci = 0; ci < num_components; ci++) {
      inptr = input_buf[ci][input_row];
      outptr = output_buf[0] + ci;
      for (count = num_cols; count > 0; count--) {
	*outptr = *inptr++;	/* needn't bother with GETJSAMPLE() here */
	outptr += num_components;
      }
    }
    input_row++;
    output_buf++;
  }
}


/*
 * Color conversion for grayscale: just copy the data.
 * This also works for YCbCr -> grayscale conversion, in which
 * we just copy the Y (luminance) component and ignore chrominance.
 */

METHODDEF(void)
grayscale_convert (j_decompress_ptr cinfo,
		   JSAMPIMAGE input_buf, JDIMENSION input_row,
		   JSAMPARRAY output_buf, int num_rows)
{
  jcopy_sample_rows(input_buf[0], (int) input_row, output_buf, 0,
		    num_rows, cinfo->output_width);
}


/*
 * Convert grayscale to RGB: just duplicate the graylevel three times.
 * This is provided to support applications that don't want to cope
 * with grayscale as a separate case.
 */

METHODDEF(void)
gray_rgb_convert (j_decompress_ptr cinfo,
		  JSAMPIMAGE input_buf, JDIMENSION input_row,
		  JSAMPARRAY output_buf, int num_rows)
{
#ifdef SIMD_OPT
    register JSAMPROW inptr, outptr;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;

    while (--num_rows >= 0) {
        inptr = input_buf[0][input_row];
        outptr = *output_buf++;
        for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8) {

            asm volatile (
                " VLD1.8   {d0},[%[inptr]]! \n\t"
                " VMOV     d1,d0 \n\t"
                " VMOV     d2,d0 \n\t"
                " VST3.8   {d0,d1,d2},[%[outptr]]! \n\t"
                :[outptr] "+r" (outptr)
             :[inptr] "r" (inptr)
                : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7"
                );

        }
        inptr = input_buf[0][input_row++];

        for (; col < num_cols; col++) {
            /* We can dispense with GETJSAMPLE() here */
            outptr[RGB_RED] = outptr[RGB_GREEN] = outptr[RGB_BLUE] = inptr[col];
            outptr += RGB_PIXELSIZE;
        }
    }

#else
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;

  while (--num_rows >= 0) {
    inptr = input_buf[0][input_row++];
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      /* We can dispense with GETJSAMPLE() here */
      outptr[RGB_RED] = outptr[RGB_GREEN] = outptr[RGB_BLUE] = inptr[col];
      outptr += RGB_PIXELSIZE;
    }
  }
#endif
}

#ifdef ANDROID_RGB
METHODDEF(void)
gray_rgba_8888_convert (j_decompress_ptr cinfo,
          JSAMPIMAGE input_buf, JDIMENSION input_row,
          JSAMPARRAY output_buf, int num_rows)
{
#ifdef SIMD_OPT
    register JSAMPROW inptr, outptr;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;
    uint8x8_t vdu8_alpha = vmov_n_u8(0xff);

    while (--num_rows >= 0) {
        inptr = input_buf[0][input_row++];
        outptr = *output_buf++;
        for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8) {

            uint8x8_t vdu8_inptr = vld1_u8(&(inptr[col]));
            uint8x8x4_t vdu8x4_outptr;
            vdu8x4_outptr.val[0] = vdu8_inptr;
            vdu8x4_outptr.val[1] = vdu8_inptr;
            vdu8x4_outptr.val[2] = vdu8_inptr;
            vdu8x4_outptr.val[3] = vdu8_alpha;

            vst4_u8((uint8_t*)outptr, vdu8x4_outptr);
            outptr += 4 * 8;
        }
        for (; col < num_cols; col++) {
            /* We can dispense with GETJSAMPLE() here */
            outptr[RGB_RED] = outptr[RGB_GREEN] = outptr[RGB_BLUE] = inptr[col];
            outptr[RGB_ALPHA] = 0xff;
            outptr += 4;
        }
    }
#else
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;

  while (--num_rows >= 0) {
    inptr = input_buf[0][input_row++];
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      /* We can dispense with GETJSAMPLE() here */
      outptr[RGB_RED] = outptr[RGB_GREEN] = outptr[RGB_BLUE] = inptr[col];
      outptr[RGB_ALPHA] = 0xff;
      outptr += 4;
    }
  }
#endif
}

METHODDEF(void)
gray_rgb_565_convert (j_decompress_ptr cinfo,
          JSAMPIMAGE input_buf, JDIMENSION input_row,
          JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;

  while (--num_rows >= 0) {
    INT32 rgb;
    unsigned int g;
    inptr = input_buf[0][input_row++];
    outptr = *output_buf++;
    if (PACK_NEED_ALIGNMENT(outptr)) {
        g = *inptr++;
        rgb = PACK_SHORT_565(g, g, g);
        *(INT16*)outptr = rgb;
        outptr += 2;
        num_cols--;
    }
    for (col = 0; col < (num_cols>>1); col++) {
      g = *inptr++;
      rgb = PACK_SHORT_565(g, g, g);
      g = *inptr++;
      rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(g, g, g));
      WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
      outptr += 4;
    }
    if (num_cols&1) {
      g = *inptr;
      rgb = PACK_SHORT_565(g, g, g);
      *(INT16*)outptr = rgb;
    }
  }
}

METHODDEF(void)
gray_rgb_565D_convert (j_decompress_ptr cinfo,
          JSAMPIMAGE input_buf, JDIMENSION input_row,
          JSAMPARRAY output_buf, int num_rows)
{
  register JSAMPROW inptr, outptr;
  register JDIMENSION col;
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  JDIMENSION num_cols = cinfo->output_width;
  INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];

  while (--num_rows >= 0) {
    INT32 rgb;
    unsigned int g;
    inptr = input_buf[0][input_row++];
    outptr = *output_buf++;
    if (PACK_NEED_ALIGNMENT(outptr)) {
        g = *inptr++;
        g = range_limit[DITHER_565_R(g, d0)];
        rgb = PACK_SHORT_565(g, g, g);
        *(INT16*)outptr = rgb;
        outptr += 2;
        num_cols--;
    }
    for (col = 0; col < (num_cols>>1); col++) {
      g = *inptr++;
      g = range_limit[DITHER_565_R(g, d0)];
      rgb = PACK_SHORT_565(g, g, g);
      d0 = DITHER_ROTATE(d0);
      g = *inptr++;
      g = range_limit[DITHER_565_R(g, d0)];
      rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(g, g, g));
      d0 = DITHER_ROTATE(d0);
      WRITE_TWO_ALIGNED_PIXELS(outptr, rgb);
      outptr += 4;
    }
    if (num_cols&1) {
      g = *inptr;
      g = range_limit[DITHER_565_R(g, d0)];
      rgb = PACK_SHORT_565(g, g, g);
      *(INT16*)outptr = rgb;
    }
  }
}
#endif

/*
 * Adobe-style YCCK->CMYK conversion.
 * We convert YCbCr to R=1-C, G=1-M, and B=1-Y using the same
 * conversion as above, while passing K (black) unchanged.
 * We assume build_ycc_rgb_table has been called.
 */

METHODDEF(void)
ycck_cmyk_convert (j_decompress_ptr cinfo,
		   JSAMPIMAGE input_buf, JDIMENSION input_row,
		   JSAMPARRAY output_buf, int num_rows)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int y, cb, cr;
  register JSAMPROW outptr;
  register JSAMPROW inptr0, inptr1, inptr2, inptr3;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;
  /* copy these pointers into registers if possible */
  register JSAMPLE * range_limit = cinfo->sample_range_limit;
  register int * Crrtab = cconvert->Cr_r_tab;
  register int * Cbbtab = cconvert->Cb_b_tab;
  register INT32 * Crgtab = cconvert->Cr_g_tab;
  register INT32 * Cbgtab = cconvert->Cb_g_tab;
  SHIFT_TEMPS

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    inptr3 = input_buf[3][input_row];
    input_row++;
    outptr = *output_buf++;
    for (col = 0; col < num_cols; col++) {
      y  = GETJSAMPLE(inptr0[col]);
      cb = GETJSAMPLE(inptr1[col]);
      cr = GETJSAMPLE(inptr2[col]);
      /* Range-limiting is essential due to noise introduced by DCT losses. */
      outptr[0] = range_limit[MAXJSAMPLE - (y + Crrtab[cr])];   /* red */
      outptr[1] = range_limit[MAXJSAMPLE - (y +                 /* green */
                              ((int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr],
                                                 SCALEBITS)))];
      outptr[2] = range_limit[MAXJSAMPLE - (y + Cbbtab[cb])];   /* blue */
      /* K passes through unchanged */
      outptr[3] = inptr3[col];	/* don't need GETJSAMPLE here */
      outptr += 4;
    }
  }
}


/*
 * Empty method for start_pass.
 */

METHODDEF(void)
start_pass_dcolor (j_decompress_ptr cinfo)
{
  /* no work needed */
}


/*
 * Module initialization routine for output colorspace conversion.
 */

GLOBAL(void)
jinit_color_deconverter (j_decompress_ptr cinfo)
{
  my_cconvert_ptr cconvert;
  int ci;

  cconvert = (my_cconvert_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(my_color_deconverter));
  cinfo->cconvert = (struct jpeg_color_deconverter *) cconvert;
  cconvert->pub.start_pass = start_pass_dcolor;

  /* Make sure num_components agrees with jpeg_color_space */
  switch (cinfo->jpeg_color_space) {
  case JCS_GRAYSCALE:
    if (cinfo->num_components != 1)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;

  case JCS_RGB:
  case JCS_YCbCr:
    if (cinfo->num_components != 3)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;

  case JCS_CMYK:
  case JCS_YCCK:
    if (cinfo->num_components != 4)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;

  default:			/* JCS_UNKNOWN can be anything */
    if (cinfo->num_components < 1)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    break;
  }

  /* Set out_color_components and conversion method based on requested space.
   * Also clear the component_needed flags for any unused components,
   * so that earlier pipeline stages can avoid useless computation.
   */

  switch (cinfo->out_color_space) {
  case JCS_GRAYSCALE:
    cinfo->out_color_components = 1;
    if (cinfo->jpeg_color_space == JCS_GRAYSCALE ||
	cinfo->jpeg_color_space == JCS_YCbCr) {
      cconvert->pub.color_convert = grayscale_convert;
      /* For color->grayscale conversion, only the Y (0) component is needed */
      for (ci = 1; ci < cinfo->num_components; ci++)
	cinfo->comp_info[ci].component_needed = FALSE;
    } else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_RGB:
    cinfo->out_color_components = RGB_PIXELSIZE;
    if (cinfo->jpeg_color_space == JCS_YCbCr) {
      cconvert->pub.color_convert = ycc_rgb_convert;
      build_ycc_rgb_table(cinfo);
    } else if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
      cconvert->pub.color_convert = gray_rgb_convert;
    } else if (cinfo->jpeg_color_space == JCS_RGB && RGB_PIXELSIZE == 3) {
      cconvert->pub.color_convert = null_convert;
    } else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

#ifdef ANDROID_RGB
  case JCS_RGBA_8888:
    cinfo->out_color_components = 4;
    if (cinfo->jpeg_color_space == JCS_YCbCr) {
#if defined(NV_ARM_NEON) && defined(__ARM_HAVE_NEON)
      if (cap_neon_ycc_rgb()) {
        cconvert->pub.color_convert = jsimd_ycc_rgba8888_convert;
      } else {
        cconvert->pub.color_convert = ycc_rgba_8888_convert;
      }
#else
      cconvert->pub.color_convert = ycc_rgba_8888_convert;
#endif
      build_ycc_rgb_table(cinfo);
    } else if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
      cconvert->pub.color_convert = gray_rgba_8888_convert;
    } else if (cinfo->jpeg_color_space == JCS_RGB) {
      cconvert->pub.color_convert = rgb_rgba_8888_convert;
    } else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_RGB_565:
    cinfo->out_color_components = RGB_PIXELSIZE;
    if (cinfo->dither_mode == JDITHER_NONE) {
      if (cinfo->jpeg_color_space == JCS_YCbCr) {
#if defined(NV_ARM_NEON) && defined(__ARM_HAVE_NEON)
        if (cap_neon_ycc_rgb())  {
          cconvert->pub.color_convert = jsimd_ycc_rgb565_convert;
        } else {
          cconvert->pub.color_convert = ycc_rgb_565_convert;
        }
#else
        cconvert->pub.color_convert = ycc_rgb_565_convert;
#endif
        build_ycc_rgb_table(cinfo);
      } else if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
        cconvert->pub.color_convert = gray_rgb_565_convert;
      } else if (cinfo->jpeg_color_space == JCS_RGB) {
        cconvert->pub.color_convert = rgb_rgb_565_convert;
      } else
        ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    } else {
      /* only ordered dither is supported */
      if (cinfo->jpeg_color_space == JCS_YCbCr) {
        cconvert->pub.color_convert = ycc_rgb_565D_convert;
        build_ycc_rgb_table(cinfo);
      } else if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
        cconvert->pub.color_convert = gray_rgb_565D_convert;
      } else if (cinfo->jpeg_color_space == JCS_RGB) {
        cconvert->pub.color_convert = rgb_rgb_565D_convert;
      } else
        ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    }
    break;
#endif

  case JCS_CMYK:
    cinfo->out_color_components = 4;
    if (cinfo->jpeg_color_space == JCS_YCCK) {
      cconvert->pub.color_convert = ycck_cmyk_convert;
      build_ycc_rgb_table(cinfo);
    } else if (cinfo->jpeg_color_space == JCS_CMYK) {
      cconvert->pub.color_convert = null_convert;
    } else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  default:
    /* Permit null conversion to same output space */
    if (cinfo->out_color_space == cinfo->jpeg_color_space) {
      cinfo->out_color_components = cinfo->num_components;
      cconvert->pub.color_convert = null_convert;
    } else			/* unsupported non-null conversion */
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;
  }

  if (cinfo->quantize_colors)
    cinfo->output_components = 1; /* single colormapped output component */
  else
    cinfo->output_components = cinfo->out_color_components;
}
