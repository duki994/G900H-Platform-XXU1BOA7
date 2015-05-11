/*
 * jdcolor.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains output colorspace conversion routines.
 */

#if defined (__ARM_HAVE_NEON) && (SIMD_16BIT || SIMD_32BIT)

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"

#include <arm_neon.h>

/* Private subobject */

typedef struct {
  struct jpeg_color_deconverter pub; /* public fields */

  /* Private state for YCC->RGB conversion */
  int * Cr_r_tab;        /* => table for Cr to R conversion */
  int * Cb_b_tab;        /* => table for Cb to B conversion */
  INT32 * Cr_g_tab;        /* => table for Cr to G conversion */
  INT32 * Cb_g_tab;        /* => table for Cb to G conversion */
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
//static const INT32 dither_matrix[4] = {
//  0x0008020A,
//  0x0C040E06,
//  0x030B0109,
//  0x0F070D05
//};

#endif

/**************** YCbCr -> RGB conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *    R = Y                + 1.40200 * Cr
 *    G = Y - 0.34414 * Cb - 0.71414 * Cr
 *    B = Y + 1.77200 * Cb
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

#define SCALEBITS    16    /* speediest right-shift on some machines */
#define ONE_HALF    ((INT32) 1 << (SCALEBITS-1))
#define FIX(x)        ((INT32) ((x) * (1L<<SCALEBITS) + 0.5))


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
GLOBAL(void)
ycc_rgb_convert_sub_16bit (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
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

  int16x8_t vqs16_1_772, vqs16_0_34414, vqs16_1_402, vqs16_128, vqs16_0_71414, vqs16_64;
  int16x8_t vqs16_tmp, vqs16_tmp2;
  uint8x8x3_t vdu8x3_rgb;
  vqs16_1_772 = vdupq_n_s16( 227 );
  vqs16_0_34414 = vdupq_n_s16( 44 );
  vqs16_1_402 = vdupq_n_s16( 179 );
  vqs16_128 = vdupq_n_s16( 128 );
  vqs16_64 = vshrq_n_s16(vqs16_128, 1);
  vqs16_0_71414 = vdupq_n_s16( 91 );

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;

    for (col = 0; col < (num_cols & 0xfffffff8); col+=8) {
      uint8x8_t vdu8_y = vld1_u8(&inptr0[col]);
      uint8x8_t vdu8_cb = vld1_u8(&inptr1[col]);
      uint8x8_t vdu8_cr = vld1_u8(&inptr2[col]);

      int16x8_t vqs16_y = vreinterpretq_s16_u16( vmovl_u8(vdu8_y) );
      int16x8_t vqs16_cb = vreinterpretq_s16_u16( vmovl_u8(vdu8_cb) );
      int16x8_t vqs16_cr = vreinterpretq_s16_u16( vmovl_u8(vdu8_cr) );

      vqs16_cb = vsubq_s16( vqs16_cb, vqs16_128 );
      vqs16_cr = vsubq_s16( vqs16_cr, vqs16_128 );

      uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_cb, vqs16_1_772), 7), vqs16_y));

      vqs16_tmp = vmlaq_s16( vqs16_64, vqs16_cb, vqs16_0_34414);
      vqs16_tmp2 = vmlaq_s16( vqs16_tmp, vqs16_cr, vqs16_0_71414);
      vqs16_tmp = vshrq_n_s16( vqs16_tmp2, 7 );

      uint8x8_t vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y, vqs16_tmp));
      uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_cr, vqs16_1_402), 7), vqs16_y));

      vdu8x3_rgb.val[0] = vdu8_r;
      vdu8x3_rgb.val[1] = vdu8_g;
      vdu8x3_rgb.val[2] = vdu8_b;

      vst3_u8(&(outptr[RGB_RED]), vdu8x3_rgb);
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
}

GLOBAL(void)
ycc_rgb_convert_sub_32bit (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
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

        int32x4_t vqs32_1_772, vqs32_0_34414, vqs32_1_402 , vqs32_0_71414, vqs32_half;
    int32x4_t vqs32_tmp, vqs32_tmp2;
    int16x4_t vds16_tmp_low,vds16_tmp_high, vds16_128;
    uint8x8x3_t vdu8x3_rgb;

    vqs32_1_772 = vdupq_n_s32(116130);
    vqs32_0_34414 = vdupq_n_s32(-22554);
    vqs32_1_402 = vdupq_n_s32(91881);
    vqs32_0_71414 = vdupq_n_s32(-46802);
    vds16_128 = vmov_n_s16(128);
    vqs32_half = vdupq_n_s32(32768);

    while (--num_rows >= 0) {
        inptr0 = input_buf[0][input_row];
        inptr1 = input_buf[1][input_row];
        inptr2 = input_buf[2][input_row];
        input_row++;
        outptr = *output_buf++;

        for (col = 0; col < (num_cols & 0xfffffff8); col+=8) {
            uint8x8_t vdu8_y = vld1_u8(&inptr0[col]);
            uint8x8_t vdu8_cb = vld1_u8(&inptr1[col]);
            uint8x8_t vdu8_cr = vld1_u8(&inptr2[col]);

            //int16x8_t vqs16_y = vreinterpretq_s16_u16( vmovl_u8(vdu8_y) );
            int16x8_t vqs16_cb = vreinterpretq_s16_u16( vmovl_u8(vdu8_cb) );
            int16x8_t vqs16_cr = vreinterpretq_s16_u16( vmovl_u8(vdu8_cr) );

            int32x4_t vqs32_cb_l = vsubl_s16(vget_low_s16(vqs16_cb), vds16_128);
            int32x4_t vqs32_cr_l = vsubl_s16(vget_low_s16(vqs16_cr), vds16_128);

            int32x4_t vqs32_cb_h = vsubl_s16(vget_high_s16(vqs16_cb), vds16_128);
            int32x4_t vqs32_cr_h = vsubl_s16(vget_high_s16(vqs16_cr), vds16_128);

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

            //Result
            uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vcombine_s16(vds16_b_low,vds16_b_high), vqs16_y));
            uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vcombine_s16(vds16_r_low,vds16_r_high), vqs16_y));

            uint8x8_t vdu8_g = vqmovun_s16(vaddq_s16(vqs16_y, vcombine_s16(vds16_tmp_low, vds16_tmp_high)));

            vdu8x3_rgb.val[0] = vdu8_r;
            vdu8x3_rgb.val[1] = vdu8_g;
            vdu8x3_rgb.val[2] = vdu8_b;

            vst3_u8(&(outptr[RGB_RED]), vdu8x3_rgb);

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
}

GLOBAL(void)
ycc_rgba_8888_convert_sub_16bit (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
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


    int16x8_t vqs16_1_772, vqs16_0_34414, vqs16_1_402, vqs16_128, vqs16_0_71414, vqs16_64;
    int16x8_t vqs16_tmp, vqs16_tmp2;
    uint8x8x4_t vdu8x4_rgb;
    uint8x8_t vdu8_a = vmov_n_u8(0xFF);
    vqs16_1_772 = vdupq_n_s16( 227 );
    vqs16_0_34414 = vdupq_n_s16( 44 );
    vqs16_1_402 = vdupq_n_s16( 179 );
    vqs16_128 = vdupq_n_s16( 128 );
    vqs16_64 = vshrq_n_s16(vqs16_128, 1);
    vqs16_0_71414 = vdupq_n_s16( 91 );

    while (--num_rows >= 0) {
        inptr0 = input_buf[0][input_row];
        inptr1 = input_buf[1][input_row];
        inptr2 = input_buf[2][input_row];
        input_row++;
        outptr = *output_buf++;

        for (col = 0; col < (num_cols & 0xfffffff8); col+=8) {
            uint8x8_t vdu8_y = vld1_u8(&inptr0[col]);
            uint8x8_t vdu8_cb = vld1_u8(&inptr1[col]);
            uint8x8_t vdu8_cr = vld1_u8(&inptr2[col]);

            int16x8_t vqs16_y = vreinterpretq_s16_u16( vmovl_u8(vdu8_y) );
            int16x8_t vqs16_cb = vreinterpretq_s16_u16( vmovl_u8(vdu8_cb) );
            int16x8_t vqs16_cr = vreinterpretq_s16_u16( vmovl_u8(vdu8_cr) );

            vqs16_cb = vsubq_s16( vqs16_cb, vqs16_128 );
            vqs16_cr = vsubq_s16( vqs16_cr, vqs16_128 );

            uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_cb, vqs16_1_772), 7), vqs16_y));

            vqs16_tmp = vmlaq_s16( vqs16_64, vqs16_cb, vqs16_0_34414);
            vqs16_tmp2 = vmlaq_s16( vqs16_tmp, vqs16_cr, vqs16_0_71414);
            vqs16_tmp = vshrq_n_s16( vqs16_tmp2 , 7 );

            uint8x8_t vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y, vqs16_tmp));
            uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16( vqs16_64, vqs16_cr, vqs16_1_402), 7), vqs16_y));

            vdu8x4_rgb.val[0] = vdu8_r;
            vdu8x4_rgb.val[1] = vdu8_g;
            vdu8x4_rgb.val[2] = vdu8_b;
            vdu8x4_rgb.val[3] = vdu8_a;

            vst4_u8(&(outptr[RGB_RED]), vdu8x4_rgb);

            outptr += 4 * 8;
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
            outptr[RGB_ALPHA] =  0xFF;
            outptr += 4;
        }
    }
}

GLOBAL(void)
ycc_rgba_8888_convert_sub_32bit (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
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

        int32x4_t vqs32_1_772, vqs32_0_34414, vqs32_1_402 , vqs32_0_71414, vqs32_half;
    int32x4_t vqs32_tmp, vqs32_tmp2;
    int16x4_t vds16_tmp_low,vds16_tmp_high, vds16_128;
    uint8x8x4_t vdu8x4_rgb;

    vqs32_1_772 = vdupq_n_s32(116130);
    vqs32_0_34414 = vdupq_n_s32(-22554);
    vqs32_1_402 = vdupq_n_s32(91881);
    vqs32_0_71414 = vdupq_n_s32(-46802);
    vds16_128 = vmov_n_s16(128);
    vqs32_half = vdupq_n_s32(32768);
    uint8x8_t vdu8_a = vmov_n_u8(0xFF);

    while (--num_rows >= 0) {
        inptr0 = input_buf[0][input_row];
        inptr1 = input_buf[1][input_row];
        inptr2 = input_buf[2][input_row];
        input_row++;
        outptr = *output_buf++;

        for (col = 0; col < (num_cols & 0xfffffff8); col+=8) {
            uint8x8_t vdu8_y = vld1_u8(&inptr0[col]);
            uint8x8_t vdu8_cb = vld1_u8(&inptr1[col]);
            uint8x8_t vdu8_cr = vld1_u8(&inptr2[col]);

            int16x8_t vqs16_cb = vreinterpretq_s16_u16( vmovl_u8(vdu8_cb) );
            int16x8_t vqs16_cr = vreinterpretq_s16_u16( vmovl_u8(vdu8_cr) );

            int32x4_t vqs32_cb_l = vsubl_s16(vget_low_s16(vqs16_cb), vds16_128);
            int32x4_t vqs32_cr_l = vsubl_s16(vget_low_s16(vqs16_cr), vds16_128);

            int32x4_t vqs32_cb_h = vsubl_s16(vget_high_s16(vqs16_cb), vds16_128);
            int32x4_t vqs32_cr_h = vsubl_s16(vget_high_s16(vqs16_cr), vds16_128);

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

            //Result
            uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vcombine_s16(vds16_b_low,vds16_b_high), vqs16_y));
            uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vcombine_s16(vds16_r_low,vds16_r_high), vqs16_y));

            uint8x8_t vdu8_g = vqmovun_s16(vaddq_s16(vqs16_y, vcombine_s16(vds16_tmp_low, vds16_tmp_high)));

            vdu8x4_rgb.val[0] = vdu8_r;
            vdu8x4_rgb.val[1] = vdu8_g;
            vdu8x4_rgb.val[2] = vdu8_b;
            vdu8x4_rgb.val[3] = vdu8_a;

            vst4_u8(&(outptr[RGB_RED]), vdu8x4_rgb);

            outptr += 4 * 8;
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
            outptr[RGB_ALPHA] =  0xFF;
            outptr += 4;
        }
    }
}

GLOBAL(void)
ycc_rgb_565_convert_sub_32bit (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
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

  int32x4_t vqs32_1_772, vqs32_0_34414, vqs32_1_402 , vqs32_0_71414, vqs32_half;
  int32x4_t vqs32_tmp, vqs32_tmp2;
  int16x4_t vds16_tmp_low,vds16_tmp_high;
  int16x8_t vqs16_128;

  vqs32_1_772 = vdupq_n_s32(116130);
  vqs32_0_34414 = vdupq_n_s32(-22554);
  vqs32_1_402 = vdupq_n_s32(91881);
  vqs32_0_71414 = vdupq_n_s32(-46802);
  vqs32_half = vdupq_n_s32(32768);
  vqs16_128 = vmovq_n_s16(128);
  uint8x8_t vdu8_a = vmov_n_u8(0xFF);

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

      // Check the value (value > 0, value <= 255)
      uint16x8_t vqu16_b = vqshluq_n_s16(vaddq_s16(vcombine_s16(vds16_b_low,vds16_b_high), vqs16_y), 8);
      uint16x8_t vqu16_r = vqshluq_n_s16(vaddq_s16(vcombine_s16(vds16_r_low,vds16_r_high), vqs16_y), 8);

      uint16x8_t vqu16_g = vqshluq_n_s16(vaddq_s16(vqs16_y, vcombine_s16(vds16_tmp_low, vds16_tmp_high)), 8);

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
}

GLOBAL(void)
ycc_rgb_565_convert_sub_16bit (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf, JDIMENSION input_row,
         JSAMPARRAY output_buf, int num_rows)
{
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
        r = range_limit[y + Crrtab[cr]];
        g = range_limit[y + ((int)RIGHT_SHIFT(Cbgtab[cb]+Crgtab[cr], SCALEBITS))];
        b = range_limit[y + Cbbtab[cb]];
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

      uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16(vqs16_one_half, vqs16_1_772, vqs16_cb ), 7), vqs16_y));

      vqs16_tmp = vmlaq_s16( vqs16_one_half, vqs16_0_34414, vqs16_cb );
      vqs16_tmp2 = vmlaq_s16( vqs16_tmp, vqs16_0_71414, vqs16_cr );
      vqs16_tmp = vshrq_n_s16( vqs16_tmp2, 7 );

      uint8x8_t vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y, vqs16_tmp));
      uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16( vqs16_one_half, vqs16_1_402, vqs16_cr ), 7), vqs16_y));

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
}

#define DITHER_MASK         0x3
#define DITHER_ROTATE(x)    (((x)<<24) | (((x)>>8)&0x00FFFFFF))
static const INT32 dither_matrix[4] = {
    0x0008020A,
    0x0C040E06,
    0x030B0109,
    0x0F070D05
};

GLOBAL(void)
ycc_rgb_565D_convert_sub_32bit (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
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
    int16x4_t vds16_tmp_low,vds16_tmp_high;
    int16x8_t vqs16_128;

    vqs32_1_772 = vdupq_n_s32(116130);
    vqs32_0_34414 = vdupq_n_s32(-22554);
    vqs32_1_402 = vdupq_n_s32(91881);
    vqs32_0_71414 = vdupq_n_s32(-46802);
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
}

// ycc_rgb_565D_convert 16bit
GLOBAL(void)
ycc_rgb_565D_convert_sub_16bit (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
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
}

//ycc_rgb_565D_convert 32bit 8x8 matrix dither .. eppl dither
GLOBAL(void)
ycc_rgb_565D_convert_sub_8matrix_32bit (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
    my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
    register int y, cb, cr;
    register JSAMPROW outptr;
    register JSAMPROW inptr0, inptr1, inptr2;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;

const unsigned char Bayer_8x8Matrix[8][8] = {{84,148,100,164,88,152,104,168}
                                            ,{212,20,228,36,216,24,232,40}
                                            ,{116,180,68,132,120,184,72,136}
                                            ,{244,52,196,4,248,56,200,8}
                                            ,{92,156,108,172,80,144,96,160}
                                            ,{220,28,236,44,208,16,224,32}
                                            ,{124,188,76,140,112,176,64,128}
                                            ,{252,60,204,12,240,48,192,0}};

    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    register int * Crrtab = cconvert->Cr_r_tab;
    register int * Cbbtab = cconvert->Cb_b_tab;
    register INT32 * Crgtab = cconvert->Cr_g_tab;
    register INT32 * Cbgtab = cconvert->Cb_g_tab;
    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    SHIFT_TEMPS

    int32x4_t vqs32_1_772, vqs32_0_34414, vqs32_1_402 , vqs32_0_71414, vqs32_half;
    int32x4_t vqs32_tmp, vqs32_tmp2;
    int16x4_t vds16_tmp_low,vds16_tmp_high;
    int16x8_t vqs16_128;

    vqs32_1_772 = vdupq_n_s32(116130);
    vqs32_0_34414 = vdupq_n_s32(-22554);
    vqs32_1_402 = vdupq_n_s32(91881);
    vqs32_0_71414 = vdupq_n_s32(-46802);
    vqs32_half = vdupq_n_s32(32768);
    vqs16_128 = vmovq_n_s16(128);

    uint16x8_t vqFloodR, vqFloodG, vqFloodB;
    uint8x8_t vdFracR, vdFracG, vdFracB, vdFracCmp;

    uint8x8_t vdDCoff1 = vmov_n_u8(249);
    uint8x8_t vdDCoff2 = vmov_n_u8(253);

    vdFracCmp = vld1_u8(Bayer_8x8Matrix[0] + ((cinfo->output_scanline % 8) * 8));

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

            //int16x8_t vqs16_y = vreinterpretq_s16_u16( vmovl_u8(vdu8_y) );
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


            // Check the value (value > 0, value <= 255)
            uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vcombine_s16(vds16_b_low,vds16_b_high), vqs16_y));
            uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vcombine_s16(vds16_r_low,vds16_r_high), vqs16_y));

            uint8x8_t vdu8_g = vqmovun_s16(vaddq_s16(vqs16_y, vcombine_s16(vds16_tmp_low, vds16_tmp_high)));

            // Dithering
            vqFloodR = vmull_u8(vdu8_r, vdDCoff1);
            vqFloodG = vmull_u8(vdu8_g, vdDCoff2);
            vqFloodB = vmull_u8(vdu8_b, vdDCoff1);

            vdFracR = vshrn_n_u16(vqFloodR, 3);
            vdFracG = vshrn_n_u16(vqFloodG, 2);
            vdFracB = vshrn_n_u16(vqFloodB, 3);

            //vdFracCmp = vld1_u8(Bayer_8x8Matrix[0] + ((h % 8) * 8));

            vdFracR = vcgt_u8(vdFracR, vdFracCmp);
            vdFracG = vcgt_u8(vdFracG, vdFracCmp);
            vdFracB = vcgt_u8(vdFracB, vdFracCmp);

            vdFracR = vshr_n_u8(vdFracR, 7);
            vdFracG = vshr_n_u8(vdFracG, 7);
            vdFracB = vshr_n_u8(vdFracB, 7);

            vqFloodR = vshrq_n_u16(vqFloodR, 11);
            vqFloodG = vshrq_n_u16(vqFloodG, 10);
            vqFloodB = vshrq_n_u16(vqFloodB, 11);

            vqFloodR = vaddw_u8(vqFloodR, vdFracR);
            vqFloodG = vaddw_u8(vqFloodG, vdFracG);
            vqFloodB = vaddw_u8(vqFloodB, vdFracB);

            vqFloodB = vsliq_n_u16(vqFloodB, vqFloodG, 5);
            vqFloodR = vsliq_n_u16(vqFloodB, vqFloodR, 11);

            // Store RGB565 and Increase Each Destination Pointers
            vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqFloodR));
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
}

//ycc_rgb_565D_convert 16bit 8x8 matrix dither .. eppl dither
GLOBAL(void)
ycc_rgb_565D_convert_sub_8matrix_16bit (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
    my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
    register int y, cb, cr;
    register JSAMPROW outptr;
    register JSAMPROW inptr0, inptr1, inptr2;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;

    const unsigned char Bayer_8x8Matrix[8][8] = {{84,148,100,164,88,152,104,168}
                                            ,{212,20,228,36,216,24,232,40}
                                            ,{116,180,68,132,120,184,72,136}
                                            ,{244,52,196,4,248,56,200,8}
                                            ,{92,156,108,172,80,144,96,160}
                                            ,{220,28,236,44,208,16,224,32}
                                            ,{124,188,76,140,112,176,64,128}
                                            ,{252,60,204,12,240,48,192,0}};

    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    register int * Crrtab = cconvert->Cr_r_tab;
    register int * Cbbtab = cconvert->Cb_b_tab;
    register INT32 * Crgtab = cconvert->Cr_g_tab;
    register INT32 * Cbgtab = cconvert->Cb_g_tab;

    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    SHIFT_TEMPS

    int16x8_t vqs16_1_772, vqs16_0_34414, vqs16_1_402, vqs16_0_71414, vqs16_one_half, vqs16_128;
    int16x8_t vqs16_tmp, vqs16_tmp2;
    vqs16_1_772 = vdupq_n_s16( 227 );
    vqs16_0_34414 = vdupq_n_s16( 44 );
    vqs16_1_402 = vdupq_n_s16( 179 );
    vqs16_128 = vdupq_n_s16( 128 );
    vqs16_0_71414 = vdupq_n_s16( 91 );
    vqs16_one_half = vmovq_n_s16( 64 );
    uint8x8_t vdu8_128 = vmov_n_u8(128);

    uint16x8_t vqFloodR, vqFloodG, vqFloodB;
    uint8x8_t vdFracR, vdFracG, vdFracB, vdFracCmp;

    uint8x8_t vdDCoff1 = vmov_n_u8(249);
    uint8x8_t vdDCoff2 = vmov_n_u8(253);

    vdFracCmp = vld1_u8(Bayer_8x8Matrix[0] + ((cinfo->output_scanline % 8) * 8));

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

        uint8x8_t vdu8_b = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16(vqs16_one_half, vqs16_1_772, vqs16_cb ), 7), vqs16_y));

        vqs16_tmp = vmlaq_s16( vqs16_one_half, vqs16_0_34414, vqs16_cb );
        vqs16_tmp2 = vmlaq_s16( vqs16_tmp, vqs16_0_71414, vqs16_cr );
        vqs16_tmp = vshrq_n_s16( vqs16_tmp2, 7 );

        uint8x8_t vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y, vqs16_tmp));
        uint8x8_t vdu8_r = vqmovun_s16(vaddq_s16(vshrq_n_s16(vmlaq_s16( vqs16_one_half, vqs16_1_402, vqs16_cr ), 7), vqs16_y));

        // Dithering
        vqFloodR = vmull_u8(vdu8_r, vdDCoff1);
        vqFloodG = vmull_u8(vdu8_g, vdDCoff2);
        vqFloodB = vmull_u8(vdu8_b, vdDCoff1);

        vdFracR = vshrn_n_u16(vqFloodR, 3);
        vdFracG = vshrn_n_u16(vqFloodG, 2);
        vdFracB = vshrn_n_u16(vqFloodB, 3);

        vdFracR = vcgt_u8(vdFracR, vdFracCmp);
        vdFracG = vcgt_u8(vdFracG, vdFracCmp);
        vdFracB = vcgt_u8(vdFracB, vdFracCmp);

        vdFracR = vshr_n_u8(vdFracR, 7);
        vdFracG = vshr_n_u8(vdFracG, 7);
        vdFracB = vshr_n_u8(vdFracB, 7);

        vqFloodR = vshrq_n_u16(vqFloodR, 11);
        vqFloodG = vshrq_n_u16(vqFloodG, 10);
        vqFloodB = vshrq_n_u16(vqFloodB, 11);

        vqFloodR = vaddw_u8(vqFloodR, vdFracR);
        vqFloodG = vaddw_u8(vqFloodG, vdFracG);
        vqFloodB = vaddw_u8(vqFloodB, vdFracB);

        vqFloodB = vsliq_n_u16(vqFloodB, vqFloodG, 5);
        vqFloodR = vsliq_n_u16(vqFloodB, vqFloodR, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqFloodR));
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
}

GLOBAL(void)
gray_rgb_convert_sub_neon (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
    register JSAMPROW inptr, outptr;
    register JDIMENSION col;
    JDIMENSION num_cols = cinfo->output_width;

    while (--num_rows >= 0) {
        inptr = input_buf[0][input_row++];
        outptr = *output_buf++;
        for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8) {

            uint8x8_t vdu8_inptr = vld1_u8(&(inptr[col]));
            uint8x8x3_t vdu8x3_outptr;
            vdu8x3_outptr.val[0] = vdu8_inptr;
            vdu8x3_outptr.val[1] = vdu8_inptr;
            vdu8x3_outptr.val[2] = vdu8_inptr;

            vst3_u8((uint8_t*)outptr, vdu8x3_outptr);
            outptr += RGB_PIXELSIZE * 8;
        }
        for (; col < num_cols; col++) {
            /* We can dispense with GETJSAMPLE() here */
            outptr[RGB_RED] = outptr[RGB_GREEN] = outptr[RGB_BLUE] = inptr[col];
            outptr += RGB_PIXELSIZE;
        }
    }
}

GLOBAL(void)
gray_rgba_8888_convert_sub_neon (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows)
{
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
}

#endif
