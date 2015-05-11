/*
 * jdmerge.c
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains code for merged upsampling/color conversion.
 *
 * This file combines functions from jdsample.c and jdcolor.c;
 * read those files first to understand what's going on.
 *
 * When the chroma components are to be upsampled by simple replication
 * (ie, box filtering), we can save some work in color conversion by
 * calculating all the output pixels corresponding to a pair of chroma
 * samples at one time.  In the conversion equations
 *    R = Y           + K1 * Cr
 *    G = Y + K2 * Cb + K3 * Cr
 *    B = Y + K4 * Cb
 * only the Y term varies among the group of pixels corresponding to a pair
 * of chroma samples, so the rest of the terms can be calculated just once.
 * At typical sampling ratios, this eliminates half or three-quarters of the
 * multiplications needed for color conversion.
 *
 * This file currently provides implementations for the following cases:
 *    YCbCr => RGB color conversion only.
 *    Sampling ratios of 2h1v or 2h2v.
 *    No scaling needed at upsample time.
 *    Corner-aligned (non-CCIR601) sampling alignment.
 * Other special cases could be added, but in most applications these are
 * the only common cases.  (For uncommon cases we fall back on the more
 * general code in jdsample.c and jdcolor.c.)
 */

#if (defined __ARM_HAVE_NEON) && (defined SIMD_16BIT || defined SIMD_32BIT)

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include <arm_neon.h>

#ifdef UPSAMPLE_MERGING_SUPPORTED

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

/* Private subobject */

typedef struct {
  struct jpeg_upsampler pub;    /* public fields */

  /* Pointer to routine to do actual upsampling/conversion of one row group */
  JMETHOD(void, upmethod, (j_decompress_ptr cinfo,
               JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
               JSAMPARRAY output_buf));

  /* Private state for YCC->RGB conversion */
  int * Cr_r_tab;        /* => table for Cr to R conversion */
  int * Cb_b_tab;        /* => table for Cb to B conversion */
  INT32 * Cr_g_tab;        /* => table for Cr to G conversion */
  INT32 * Cb_g_tab;        /* => table for Cb to G conversion */

  /* For 2:1 vertical sampling, we produce two output rows at a time.
   * We need a "spare" row buffer to hold the second output row if the
   * application provides just a one-row buffer; we also use the spare
   * to discard the dummy last row if the image height is odd.
   */
  JSAMPROW spare_row;
  boolean spare_full;        /* T if spare buffer is occupied */

  JDIMENSION out_row_width;    /* samples per output row */
  JDIMENSION rows_to_go;    /* counts rows remaining in image */
} my_upsampler;

typedef my_upsampler * my_upsample_ptr;

#define SCALEBITS    16    /* speediest right-shift on some machines */
#define ONE_HALF    ((INT32) 1 << (SCALEBITS-1))
#define FIX(x)        ((INT32) ((x) * (1L<<SCALEBITS) + 0.5))

/*
 * These are the routines invoked by the control routines to do
 * the actual upsampling/conversion.  One row group is processed per call.
 *
 * Note: since we may be writing directly into application-supplied buffers,
 * we have to be honest about the output width; we can't assume the buffer
 * has been rounded up to an even width.
 */


/*
 * Upsample and color convert for the case of 2:1 horizontal and 1:1 vertical.
 */
GLOBAL(void)
h2v1_merged_upsample_sub_32bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int32_t coef1[2] = {116130, 91881};
    int32x2_t vds32_coef1 = vld1_s32(coef1);
    int32_t coef2[2] = {-22554, -46802};
    int32x2_t vds32_coef2 = vld1_s32(coef2);
    int32x4_t vqs32_116130 = vdupq_lane_s32(vds32_coef1, 0);
    int32x4_t vqs32_91881 = vdupq_lane_s32(vds32_coef1, 1);
    int32x4_t vqs32_22554_  = vdupq_lane_s32(vds32_coef2, 0);
    int32x4_t vqs32_46802_ = vdupq_lane_s32(vds32_coef2, 1);

    int32_t coef3[2] = {-14831873, -11728001};
    int32x2_t vds32_coef3 = vld1_s32(coef3);
    int32_t coef4[2] = {2919679, 5990656};
    int32x2_t vds32_coef4 = vld1_s32(coef4);
    int32x4_t vqs32_14831873_ = vdupq_lane_s32(vds32_coef3, 0);
    int32x4_t vqs32_11728001_ = vdupq_lane_s32(vds32_coef3, 1);
    int32x4_t vqs32_2919679   = vdupq_lane_s32(vds32_coef4, 0);
    int32x4_t vqs32_5990656   = vdupq_lane_s32(vds32_coef4, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr;
    JSAMPROW inptr0, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    SHIFT_TEMPS

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x4_t vds16_U_l, vds16_U_h, vds16_V_l, vds16_V_h;
    int32x4_t vqs32_U_l, vqs32_U_h, vqs32_V_l, vqs32_V_h;
    int16x4_t vds16_r_l, vds16_r_h, vds16_g_l, vds16_g_h, vds16_b_l, vds16_b_h;
    int16x4x2_t vds16x2_r_l, vds16x2_r_h, vds16x2_g_l, vds16x2_g_h, vds16x2_b_l, vds16x2_b_h;
    uint8x8_t vdu8_r_l, vdu8_r_h, vdu8_g_l, vdu8_g_h, vdu8_b_l, vdu8_b_h;
    uint8x8x3_t vdu8x3_rgb_l, vdu8x3_rgb_h;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_Y1 = vld1_u8(inptr0);
        vdu8_Y2 = vld1_u8(inptr0+8);
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr0 += 16;
        inptr1 += 8;
        inptr2 += 8;

        //__pld(inptr0 + 64);
        //__pld(inptr1 + 32);
        //__pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        vds16_U_l = vget_low_s16(vqs16_U);
        vds16_U_h = vget_high_s16(vqs16_U);
        vds16_V_l = vget_low_s16(vqs16_V);
        vds16_V_h = vget_high_s16(vqs16_V);

        vqs32_U_l = vmovl_s16(vds16_U_l);
        vqs32_U_h = vmovl_s16(vds16_U_h);
        vqs32_V_l = vmovl_s16(vds16_V_l);
        vqs32_V_h = vmovl_s16(vds16_V_h);

        // r
        vds16_r_l = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_l), 16);
        vds16_r_h = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_h), 16);
        // b
        vds16_b_l = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_l), 16);
        vds16_b_h = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_h), 16);
        // g
        vds16_g_l = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_l), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_l)), 16);
        vds16_g_h = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_h), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_h)), 16);

        vds16x2_r_l = vzip_s16(vds16_r_l, vds16_r_l);
        vds16x2_r_h = vzip_s16(vds16_r_h, vds16_r_h);
        vds16x2_g_l = vzip_s16(vds16_g_l, vds16_g_l);
        vds16x2_g_h = vzip_s16(vds16_g_h, vds16_g_h);
        vds16x2_b_l = vzip_s16(vds16_b_l, vds16_b_l);
        vds16x2_b_h = vzip_s16(vds16_b_h, vds16_b_h);

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])));
        vdu8_r_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])));
        vdu8_g_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])));
        vdu8_g_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])));
        vdu8_b_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])));
        vdu8_b_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])));

        vdu8x3_rgb_l.val[0] = vdu8_r_l;
        vdu8x3_rgb_l.val[1] = vdu8_g_l;
        vdu8x3_rgb_l.val[2] = vdu8_b_l;

        vdu8x3_rgb_h.val[0] = vdu8_r_h;
        vdu8x3_rgb_h.val[1] = vdu8_g_h;
        vdu8x3_rgb_h.val[2] = vdu8_b_h;

        vst3_u8(&(outptr[RGB_RED]), vdu8x3_rgb_l);
        outptr += 3 * 8;
        vst3_u8(&(outptr[RGB_RED]), vdu8x3_rgb_h);
        outptr += 3 * 8;
    }
    for (; col < num_cols; col++)
    {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 2 Y values and emit 2 pixels */
        y  = GETJSAMPLE(*inptr0++);
        outptr[RGB_RED] = range_limit[y + cred];
        outptr[RGB_GREEN] = range_limit[y + cgreen];
        outptr[RGB_BLUE] = range_limit[y + cblue];
        outptr += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr0++);
        outptr[RGB_RED] = range_limit[y + cred];
        outptr[RGB_GREEN] = range_limit[y + cgreen];
        outptr += RGB_PIXELSIZE;
        outptr[RGB_BLUE] = range_limit[y + cblue];
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1)
    {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr0);
        outptr[RGB_RED] = range_limit[y + cred];
        outptr[RGB_GREEN] = range_limit[y + cgreen];
        outptr[RGB_BLUE] = range_limit[y + cblue];
    }
}

GLOBAL(void)
h2v1_merged_upsample_sub_16bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int16_t coef[4] = {227, 179, 44, 91};
    int16x4_t vds16_coef1 = vld1_s16(coef);

    int16x8_t vqs16_1_772 = vdupq_lane_s16(vds16_coef1, 0);
    int16x8_t vqs16_1_402 = vdupq_lane_s16(vds16_coef1, 1);
    int16x8_t vqs16_0_34414 = vdupq_lane_s16(vds16_coef1, 2);
    int16x8_t vqs16_0_71414 = vdupq_lane_s16(vds16_coef1, 3);
    int16x8_t vqs16_128 = vmovq_n_s16(128);
    int16x8_t vqs16_64 = vshrq_n_s16(vqs16_128, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr;
    JSAMPROW inptr0, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    SHIFT_TEMPS

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x8_t vqs16_r, vqs16_g, vqs16_b;
    int16x8x2_t vqs16x2_r, vqs16x2_g, vqs16x2_b;
    uint8x8_t vdu8_r, vdu8_g, vdu8_b;
    uint8x8x3_t vdu8x3_rgb;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_Y1 = vld1_u8(inptr0);
        vdu8_Y2 = vld1_u8(inptr0+8);
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr0 += 16;
        inptr1 += 8;
        inptr2 += 8;

        //__pld(inptr0 + 64);
        //__pld(inptr1 + 32);
        //__pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        // cb
        vqs16_U = vsubq_s16(vqs16_U, vqs16_128);
        // cr
        vqs16_V = vsubq_s16(vqs16_V, vqs16_128);

        // r
        vqs16_r = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_402, vqs16_V), 7);

        // b
        vqs16_b = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_772, vqs16_U), 7);

        // g
        vqs16_g = vshrq_n_s16(vmlaq_s16(vmlaq_s16(vqs16_64, vqs16_0_34414, vqs16_U), vqs16_0_71414, vqs16_V), 7);

        vqs16x2_r = vzipq_s16(vqs16_r, vqs16_r);
        vqs16x2_g = vzipq_s16(vqs16_g, vqs16_g);
        vqs16x2_b = vzipq_s16(vqs16_b, vqs16_b);


        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y1, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_b.val[0]));

        vdu8x3_rgb.val[0] = vdu8_r;
        vdu8x3_rgb.val[1] = vdu8_g;
        vdu8x3_rgb.val[2] = vdu8_b;

        vst3_u8(&(outptr[RGB_RED]), vdu8x3_rgb);
        outptr += 3 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y2, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_b.val[1]));

        vdu8x3_rgb.val[0] = vdu8_r;
        vdu8x3_rgb.val[1] = vdu8_g;
        vdu8x3_rgb.val[2] = vdu8_b;

        vst3_u8(&(outptr[RGB_RED]), vdu8x3_rgb);
        outptr += 3 * 8;
    }
    for (; col < num_cols; col++)
    {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 2 Y values and emit 2 pixels */
        y  = GETJSAMPLE(*inptr0++);
        outptr[RGB_RED] = range_limit[y + cred];
        outptr[RGB_GREEN] = range_limit[y + cgreen];
        outptr[RGB_BLUE] = range_limit[y + cblue];
        outptr += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr0++);
        outptr[RGB_RED] = range_limit[y + cred];
        outptr[RGB_GREEN] = range_limit[y + cgreen];
        outptr += RGB_PIXELSIZE;
        outptr[RGB_BLUE] = range_limit[y + cblue];
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1)
    {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr0);
        outptr[RGB_RED] = range_limit[y + cred];
        outptr[RGB_GREEN] = range_limit[y + cgreen];
        outptr[RGB_BLUE] = range_limit[y + cblue];
    }
}

// h2v1_merged_upsample_565_32bit simd code
GLOBAL(void)
h2v1_merged_upsample_565_sub_32bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int32_t coef1[2] = {116130, 91881};
    int32x2_t vds32_coef1 = vld1_s32(coef1);
    int32_t coef2[2] = {-22554, -46802};
    int32x2_t vds32_coef2 = vld1_s32(coef2);
    int32x4_t vqs32_116130 = vdupq_lane_s32(vds32_coef1, 0);
    int32x4_t vqs32_91881 = vdupq_lane_s32(vds32_coef1, 1);
    int32x4_t vqs32_22554_  = vdupq_lane_s32(vds32_coef2, 0);
    int32x4_t vqs32_46802_ = vdupq_lane_s32(vds32_coef2, 1);

    int32_t coef3[2] = {-14831873, -11728001};
    int32x2_t vds32_coef3 = vld1_s32(coef3);
    int32_t coef4[2] = {2919679, 5990656};
    int32x2_t vds32_coef4 = vld1_s32(coef4);
    int32x4_t vqs32_14831873_ = vdupq_lane_s32(vds32_coef3, 0);
    int32x4_t vqs32_11728001_ = vdupq_lane_s32(vds32_coef3, 1);
    int32x4_t vqs32_2919679   = vdupq_lane_s32(vds32_coef4, 0);
    int32x4_t vqs32_5990656   = vdupq_lane_s32(vds32_coef4, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr;
    JSAMPROW inptr0, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x4_t vds16_U_l, vds16_U_h, vds16_V_l, vds16_V_h;
    int32x4_t vqs32_U_l, vqs32_U_h, vqs32_V_l, vqs32_V_h;
    int16x4_t vds16_r_l, vds16_r_h, vds16_g_l, vds16_g_h, vds16_b_l, vds16_b_h;
    int16x4x2_t vds16x2_r_l, vds16x2_r_h, vds16x2_g_l, vds16x2_g_h, vds16x2_b_l, vds16x2_b_h;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_Y1 = vld1_u8(inptr0);
        vdu8_Y2 = vld1_u8(inptr0+8);
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr0 += 16;
        inptr1 += 8;
        inptr2 += 8;

//        __pld(inptr0 + 64);
//        __pld(inptr1 + 32);
//        __pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        vds16_U_l = vget_low_s16(vqs16_U);
        vds16_U_h = vget_high_s16(vqs16_U);
        vds16_V_l = vget_low_s16(vqs16_V);
        vds16_V_h = vget_high_s16(vqs16_V);

        vqs32_U_l = vmovl_s16(vds16_U_l);
        vqs32_U_h = vmovl_s16(vds16_U_h);
        vqs32_V_l = vmovl_s16(vds16_V_l);
        vqs32_V_h = vmovl_s16(vds16_V_h);

        // r
        vds16_r_l = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_l), 16);
        vds16_r_h = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_h), 16);
        // b
        vds16_b_l = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_l), 16);
        vds16_b_h = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_h), 16);
        // g
        vds16_g_l = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_l), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_l)), 16);
        vds16_g_h = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_h), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_h)), 16);

        vds16x2_r_l = vzip_s16(vds16_r_l, vds16_r_l);
        vds16x2_r_h = vzip_s16(vds16_r_h, vds16_r_h);
        vds16x2_g_l = vzip_s16(vds16_g_l, vds16_g_l);
        vds16x2_g_h = vzip_s16(vds16_g_h, vds16_g_h);
        vds16x2_b_l = vzip_s16(vds16_b_l, vds16_b_l);
        vds16x2_b_h = vzip_s16(vds16_b_h, vds16_b_h);

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        uint16x8_t vqu16_r_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])), 8);
        uint16x8_t vqu16_r_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])), 8);
        uint16x8_t vqu16_g_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])), 8);
        uint16x8_t vqu16_g_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])), 8);
        uint16x8_t vqu16_b_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])), 8);
        uint16x8_t vqu16_b_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])), 8);

        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_g_l, 5);
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_b_l, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_l));
        outptr += 2 * 8;

        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_g_h, 5);
        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_b_h, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_h));
        outptr += 2 * 8;
    }
    /* Loop for each pair of output pixels */
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 2 Y values and emit 2 pixels */
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr, rgb);
        outptr += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr0);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
    }
}

// h2v1_merged_upsample_565_16bit simd code
GLOBAL(void)
h2v1_merged_upsample_565_sub_16bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int16_t coef[4] = {227, 179, 44, 91};
    int16x4_t vds16_coef1 = vld1_s16(coef);

    int16x8_t vqs16_1_772 = vdupq_lane_s16(vds16_coef1, 0);
    int16x8_t vqs16_1_402 = vdupq_lane_s16(vds16_coef1, 1);
    int16x8_t vqs16_0_34414 = vdupq_lane_s16(vds16_coef1, 2);
    int16x8_t vqs16_0_71414 = vdupq_lane_s16(vds16_coef1, 3);
    int16x8_t vqs16_128 = vmovq_n_s16(128);
    int16x8_t vqs16_64 = vshrq_n_s16(vqs16_128, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr;
    JSAMPROW inptr0, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x8_t vqs16_r, vqs16_g, vqs16_b;
    int16x8x2_t vqs16x2_r, vqs16x2_g, vqs16x2_b;
    uint8x8_t vdu8_r, vdu8_g, vdu8_b;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_Y1 = vld1_u8(inptr0);
        vdu8_Y2 = vld1_u8(inptr0+8);
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr0 += 16;
        inptr1 += 8;
        inptr2 += 8;

        //__pld(inptr0 + 64);
        //__pld(inptr1 + 32);
        //__pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        // cb
        vqs16_U = vsubq_s16(vqs16_U, vqs16_128);
        // cr
        vqs16_V = vsubq_s16(vqs16_V, vqs16_128);

        // r
        vqs16_r = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_402, vqs16_V), 7);

        // b
        vqs16_b = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_772, vqs16_U), 7);

        // g
        vqs16_g = vshrq_n_s16(vmlaq_s16(vmlaq_s16(vqs16_64, vqs16_0_34414, vqs16_U), vqs16_0_71414, vqs16_V), 7);

        vqs16x2_r = vzipq_s16(vqs16_r, vqs16_r);
        vqs16x2_g = vzipq_s16(vqs16_g, vqs16_g);
        vqs16x2_b = vzipq_s16(vqs16_b, vqs16_b);


        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y1, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_b.val[0]));

        uint16x8_t vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        uint16x8_t vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        uint16x8_t vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr += 2 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y2, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_b.val[1]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr += 2 * 8;
    }
    /* Loop for each pair of output pixels */
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 2 Y values and emit 2 pixels */
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr, rgb);
        outptr += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr0);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
    }
}

// h2v1_merged_upsample_565D_sub_32bit
GLOBAL(void)
h2v1_merged_upsample_565D_sub_32bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{

    int16_t dither_matrix_neon[4][8] = {{0x0a, 0x02, 0x08, 0x00, 0x0a, 0x02, 0x08, 0x00}
                                    ,{0x06, 0x0e, 0x04, 0x0c, 0x06, 0x0e, 0x04, 0x0c}
                                    ,{0x09, 0x01, 0x0b, 0x03, 0x09, 0x01, 0x0b, 0x03}
                                    ,{0x05, 0x0d, 0x07, 0x0f, 0x05, 0x0d, 0x07, 0x0f}};

    int32_t coef1[2] = {116130, 91881};
    int32x2_t vds32_coef1 = vld1_s32(coef1);
    int32_t coef2[2] = {-22554, -46802};
    int32x2_t vds32_coef2 = vld1_s32(coef2);
    int32x4_t vqs32_116130 = vdupq_lane_s32(vds32_coef1, 0);
    int32x4_t vqs32_91881 = vdupq_lane_s32(vds32_coef1, 1);
    int32x4_t vqs32_22554_  = vdupq_lane_s32(vds32_coef2, 0);
    int32x4_t vqs32_46802_ = vdupq_lane_s32(vds32_coef2, 1);

    int32_t coef3[2] = {-14831873, -11728001};
    int32x2_t vds32_coef3 = vld1_s32(coef3);
    int32_t coef4[2] = {2919679, 5990656};
    int32x2_t vds32_coef4 = vld1_s32(coef4);
    int32x4_t vqs32_14831873_ = vdupq_lane_s32(vds32_coef3, 0);
    int32x4_t vqs32_11728001_ = vdupq_lane_s32(vds32_coef3, 1);
    int32x4_t vqs32_2919679   = vdupq_lane_s32(vds32_coef4, 0);
    int32x4_t vqs32_5990656   = vdupq_lane_s32(vds32_coef4, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr;
    JSAMPROW inptr0, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    //JDIMENSION col_index = 0;
    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    int16x8_t vqs16_dither_matrix = vld1q_s16( dither_matrix_neon[0] + ((cinfo->output_scanline % 4) * 8));

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x4_t vds16_U_l, vds16_U_h, vds16_V_l, vds16_V_h;
    int32x4_t vqs32_U_l, vqs32_U_h, vqs32_V_l, vqs32_V_h;
    int16x4_t vds16_r_l, vds16_r_h, vds16_g_l, vds16_g_h, vds16_b_l, vds16_b_h;
    int16x4x2_t vds16x2_r_l, vds16x2_r_h, vds16x2_g_l, vds16x2_g_h, vds16x2_b_l, vds16x2_b_h;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_Y1 = vld1_u8(inptr0);
        vdu8_Y2 = vld1_u8(inptr0+8);
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr0 += 16;
        inptr1 += 8;
        inptr2 += 8;

//        __pld(inptr0 + 64);
//        __pld(inptr1 + 32);
//        __pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        vds16_U_l = vget_low_s16(vqs16_U);
        vds16_U_h = vget_high_s16(vqs16_U);
        vds16_V_l = vget_low_s16(vqs16_V);
        vds16_V_h = vget_high_s16(vqs16_V);

        vqs32_U_l = vmovl_s16(vds16_U_l);
        vqs32_U_h = vmovl_s16(vds16_U_h);
        vqs32_V_l = vmovl_s16(vds16_V_l);
        vqs32_V_h = vmovl_s16(vds16_V_h);

        // r
        vds16_r_l = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_l), 16);
        vds16_r_h = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_h), 16);
        // b
        vds16_b_l = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_l), 16);
        vds16_b_h = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_h), 16);
        // g
        vds16_g_l = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_l), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_l)), 16);
        vds16_g_h = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_h), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_h)), 16);

        vds16x2_r_l = vzip_s16(vds16_r_l, vds16_r_l);
        vds16x2_r_h = vzip_s16(vds16_r_h, vds16_r_h);
        vds16x2_g_l = vzip_s16(vds16_g_l, vds16_g_l);
        vds16x2_g_h = vzip_s16(vds16_g_h, vds16_g_h);
        vds16x2_b_l = vzip_s16(vds16_b_l, vds16_b_l);
        vds16x2_b_h = vzip_s16(vds16_b_h, vds16_b_h);

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        // dither
        int16x8_t vqs16_y1_rb = vaddq_s16(vqs16_Y1, vqs16_dither_matrix);
        int16x8_t vqs16_y1_g = vaddq_s16(vqs16_Y1, vshrq_n_s16(vqs16_dither_matrix, 1));
        int16x8_t vqs16_y2_rb = vaddq_s16(vqs16_Y2, vqs16_dither_matrix);
        int16x8_t vqs16_y2_g = vaddq_s16(vqs16_Y2, vshrq_n_s16(vqs16_dither_matrix, 1));

        uint16x8_t vqu16_r_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_rb, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])), 8);
        uint16x8_t vqu16_r_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_rb, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])), 8);
        uint16x8_t vqu16_g_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_g, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])), 8);
        uint16x8_t vqu16_g_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_g, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])), 8);
        uint16x8_t vqu16_b_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_rb, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])), 8);
        uint16x8_t vqu16_b_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_rb, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])), 8);

        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_g_l, 5);
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_b_l, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_l));
        outptr += 2 * 8;

        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_g_h, 5);
        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_b_h, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_h));
        outptr += 2 * 8;
    }
    /* Loop for each pair of output pixels */
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 2 Y values and emit 2 pixels */
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr, rgb);
        outptr += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr0);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
    }
}

// h2v1_merged_upsample_565D_sub_16bit
GLOBAL(void)
h2v1_merged_upsample_565D_sub_16bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int16_t dither_matrix_neon[4][8] = {{0x0a, 0x02, 0x08, 0x00, 0x0a, 0x02, 0x08, 0x00}
                                    ,{0x06, 0x0e, 0x04, 0x0c, 0x06, 0x0e, 0x04, 0x0c}
                                    ,{0x09, 0x01, 0x0b, 0x03, 0x09, 0x01, 0x0b, 0x03}
                                    ,{0x05, 0x0d, 0x07, 0x0f, 0x05, 0x0d, 0x07, 0x0f}};

    int16_t coef[4] = {227, 179, 44, 91};
    int16x4_t vds16_coef1 = vld1_s16(coef);

    int16x8_t vqs16_1_772 = vdupq_lane_s16(vds16_coef1, 0);
    int16x8_t vqs16_1_402 = vdupq_lane_s16(vds16_coef1, 1);
    int16x8_t vqs16_0_34414 = vdupq_lane_s16(vds16_coef1, 2);
    int16x8_t vqs16_0_71414 = vdupq_lane_s16(vds16_coef1, 3);
    int16x8_t vqs16_128 = vmovq_n_s16(128);
    int16x8_t vqs16_64 = vshrq_n_s16(vqs16_128, 1);

    int16x8_t vqs16_dither_matrix = vld1q_s16( dither_matrix_neon[0] + ((cinfo->output_scanline % 4) * 8));

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr;
    JSAMPROW inptr0, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    //JDIMENSION col_index = 0;
    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x8_t vqs16_r, vqs16_g, vqs16_b;
    int16x8x2_t vqs16x2_r, vqs16x2_g, vqs16x2_b;
    uint8x8_t vdu8_r, vdu8_g, vdu8_b;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_Y1 = vld1_u8(inptr0);
        vdu8_Y2 = vld1_u8(inptr0+8);
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr0 += 16;
        inptr1 += 8;
        inptr2 += 8;

        //__pld(inptr0 + 64);
        //__pld(inptr1 + 32);
        //__pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        // cb
        vqs16_U = vsubq_s16(vqs16_U, vqs16_128);
        // cr
        vqs16_V = vsubq_s16(vqs16_V, vqs16_128);

        // r
        vqs16_r = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_402, vqs16_V), 7);

        // b
        vqs16_b = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_772, vqs16_U), 7);

        // g
        vqs16_g = vshrq_n_s16(vmlaq_s16(vmlaq_s16(vqs16_64, vqs16_0_34414, vqs16_U), vqs16_0_71414, vqs16_V), 7);

        vqs16x2_r = vzipq_s16(vqs16_r, vqs16_r);
        vqs16x2_g = vzipq_s16(vqs16_g, vqs16_g);
        vqs16x2_b = vzipq_s16(vqs16_b, vqs16_b);


        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        int16x8_t vqs16_y1_rb = vaddq_s16(vqs16_Y1, vqs16_dither_matrix);
        int16x8_t vqs16_y1_g = vaddq_s16(vqs16_Y1, vshrq_n_s16(vqs16_dither_matrix, 1));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_y1_rb, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y1_g, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_y1_rb, vqs16x2_b.val[0]));

        uint16x8_t vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        uint16x8_t vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        uint16x8_t vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr += 2 * 8;

        int16x8_t vqs16_y2_rb = vaddq_s16(vqs16_Y2, vqs16_dither_matrix);
        int16x8_t vqs16_y2_g = vaddq_s16(vqs16_Y2, vshrq_n_s16(vqs16_dither_matrix, 1));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_y2_rb, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y2_g, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_y2_rb, vqs16x2_b.val[1]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);
        vst1q_u8(&(outptr[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr += 2 * 8;
    }
    /* Loop for each pair of output pixels */
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 2 Y values and emit 2 pixels */
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr0++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr, rgb);
        outptr += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr0);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr = rgb;
    }
}

#ifdef ANDROID_RGB
// h2v2_merged_upsample_32bit
GLOBAL(void)
h2v2_merged_upsample_sub_32bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int32_t coef1[2] = {116130, 91881};
    int32x2_t vds32_coef1 = vld1_s32(coef1);
    int32_t coef2[2] = {-22554, -46802};
    int32x2_t vds32_coef2 = vld1_s32(coef2);
    int32x4_t vqs32_116130 = vdupq_lane_s32(vds32_coef1, 0);
    int32x4_t vqs32_91881 = vdupq_lane_s32(vds32_coef1, 1);
    int32x4_t vqs32_22554_  = vdupq_lane_s32(vds32_coef2, 0);
    int32x4_t vqs32_46802_ = vdupq_lane_s32(vds32_coef2, 1);

    int32_t coef3[2] = {-14831873, -11728001};
    int32x2_t vds32_coef3 = vld1_s32(coef3);
    int32_t coef4[2] = {2919679, 5990656};
    int32x2_t vds32_coef4 = vld1_s32(coef4);
    int32x4_t vqs32_14831873_ = vdupq_lane_s32(vds32_coef3, 0);
    int32x4_t vqs32_11728001_ = vdupq_lane_s32(vds32_coef3, 1);
    int32x4_t vqs32_2919679   = vdupq_lane_s32(vds32_coef4, 0);
    int32x4_t vqs32_5990656   = vdupq_lane_s32(vds32_coef4, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr0, outptr1;
    JSAMPROW inptr00, inptr01, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    SHIFT_TEMPS

    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x4_t vds16_U_l, vds16_U_h, vds16_V_l, vds16_V_h;
    int32x4_t vqs32_U_l, vqs32_U_h, vqs32_V_l, vqs32_V_h;
    int16x4_t vds16_r_l, vds16_r_h, vds16_g_l, vds16_g_h, vds16_b_l, vds16_b_h;
    int16x4x2_t vds16x2_r_l, vds16x2_r_h, vds16x2_g_l, vds16x2_g_h, vds16x2_b_l, vds16x2_b_h;
    uint8x8_t vdu8_r_l, vdu8_r_h, vdu8_g_l, vdu8_g_h, vdu8_b_l, vdu8_b_h;
    uint8x8x3_t vdu8x3_rgb_l, vdu8x3_rgb_h;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        //inptr0 += 16;
        inptr1 += 8;
        inptr2 += 8;

//        __pld(inptr0 + 64);
//        __pld(inptr1 + 32);
//        __pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        vds16_U_l = vget_low_s16(vqs16_U);
        vds16_U_h = vget_high_s16(vqs16_U);
        vds16_V_l = vget_low_s16(vqs16_V);
        vds16_V_h = vget_high_s16(vqs16_V);

        vqs32_U_l = vmovl_s16(vds16_U_l);
        vqs32_U_h = vmovl_s16(vds16_U_h);
        vqs32_V_l = vmovl_s16(vds16_V_l);
        vqs32_V_h = vmovl_s16(vds16_V_h);

        // r
        vds16_r_l = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_l), 16);
        vds16_r_h = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_h), 16);
        // b
        vds16_b_l = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_l), 16);
        vds16_b_h = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_h), 16);
        // g
        vds16_g_l = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_l), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_l)), 16);
        vds16_g_h = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_h), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_h)), 16);

        vds16x2_r_l = vzip_s16(vds16_r_l, vds16_r_l);
        vds16x2_r_h = vzip_s16(vds16_r_h, vds16_r_h);
        vds16x2_g_l = vzip_s16(vds16_g_l, vds16_g_l);
        vds16x2_g_h = vzip_s16(vds16_g_h, vds16_g_h);
        vds16x2_b_l = vzip_s16(vds16_b_l, vds16_b_l);
        vds16x2_b_h = vzip_s16(vds16_b_h, vds16_b_h);

        vdu8_Y1 = vld1_u8(inptr00);
        vdu8_Y2 = vld1_u8(inptr00+8);

        inptr00 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])));
        vdu8_r_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])));
        vdu8_g_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])));
        vdu8_g_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])));
        vdu8_b_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])));
        vdu8_b_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])));

        vdu8x3_rgb_l.val[0] = vdu8_r_l;
        vdu8x3_rgb_l.val[1] = vdu8_g_l;
        vdu8x3_rgb_l.val[2] = vdu8_b_l;

        vdu8x3_rgb_h.val[0] = vdu8_r_h;
        vdu8x3_rgb_h.val[1] = vdu8_g_h;
        vdu8x3_rgb_h.val[2] = vdu8_b_h;

        vst3_u8(&(outptr0[RGB_RED]), vdu8x3_rgb_l);
        outptr0 += 3 * 8;
        vst3_u8(&(outptr0[RGB_RED]), vdu8x3_rgb_h);
        outptr0 += 3 * 8;

        vdu8_Y1 = vld1_u8(inptr01);
        vdu8_Y2 = vld1_u8(inptr01+8);

        inptr01 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])));
        vdu8_r_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])));
        vdu8_g_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])));
        vdu8_g_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])));
        vdu8_b_l = vqmovun_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])));
        vdu8_b_h = vqmovun_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])));

        vdu8x3_rgb_l.val[0] = vdu8_r_l;
        vdu8x3_rgb_l.val[1] = vdu8_g_l;
        vdu8x3_rgb_l.val[2] = vdu8_b_l;

        vdu8x3_rgb_h.val[0] = vdu8_r_h;
        vdu8x3_rgb_h.val[1] = vdu8_g_h;
        vdu8x3_rgb_h.val[2] = vdu8_b_h;

        vst3_u8(&(outptr1[RGB_RED]), vdu8x3_rgb_l);
        outptr1 += 3 * 8;
        vst3_u8(&(outptr1[RGB_RED]), vdu8x3_rgb_h);
        outptr1 += 3 * 8;
    }
  /* Loop for each group of output pixels */
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 4 Y values and emit 4 pixels */
        y  = GETJSAMPLE(*inptr00++);
        outptr0[RGB_RED] = range_limit[y + cred];
        outptr0[RGB_GREEN] = range_limit[y + cgreen];
        outptr0[RGB_BLUE] = range_limit[y + cblue];
        outptr0 += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr00++);
        outptr0[RGB_RED] = range_limit[y + cred];
        outptr0[RGB_GREEN] = range_limit[y + cgreen];
        outptr0[RGB_BLUE] = range_limit[y + cblue];
        outptr0 += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr01++);
        outptr1[RGB_RED] = range_limit[y + cred];
        outptr1[RGB_GREEN] = range_limit[y + cgreen];
        outptr1[RGB_BLUE] = range_limit[y + cblue];
        outptr1 += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr01++);
        outptr1[RGB_RED] = range_limit[y + cred];
        outptr1[RGB_GREEN] = range_limit[y + cgreen];
        outptr1[RGB_BLUE] = range_limit[y + cblue];
        outptr1 += RGB_PIXELSIZE;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr00);
        outptr0[RGB_RED] = range_limit[y + cred];
        outptr0[RGB_GREEN] = range_limit[y + cgreen];
        outptr0[RGB_BLUE] = range_limit[y + cblue];
        y  = GETJSAMPLE(*inptr01);
        outptr1[RGB_RED] = range_limit[y + cred];
        outptr1[RGB_GREEN] = range_limit[y + cgreen];
        outptr1[RGB_BLUE] = range_limit[y + cblue];
    }
}

// h2v2_merged_upsample_16bit
GLOBAL(void)
h2v2_merged_upsample_sub_16bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int16_t coef[4] = {227, 179, 44, 91};
    int16x4_t vds16_coef1 = vld1_s16(coef);

    int16x8_t vqs16_1_772 = vdupq_lane_s16(vds16_coef1, 0);
    int16x8_t vqs16_1_402 = vdupq_lane_s16(vds16_coef1, 1);
    int16x8_t vqs16_0_34414 = vdupq_lane_s16(vds16_coef1, 2);
    int16x8_t vqs16_0_71414 = vdupq_lane_s16(vds16_coef1, 3);
    int16x8_t vqs16_128 = vmovq_n_s16(128);
    int16x8_t vqs16_64 = vshrq_n_s16(vqs16_128, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr0, outptr1;
    JSAMPROW inptr00, inptr01, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    SHIFT_TEMPS

    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];
    /* Loop for each group of output pixels */

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x8_t vqs16_r, vqs16_g, vqs16_b;
    int16x8x2_t vqs16x2_r, vqs16x2_g, vqs16x2_b;
    uint8x8_t vdu8_r, vdu8_g, vdu8_b;
    uint8x8x3_t vdu8x3_rgb;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr1 += 8;
        inptr2 += 8;

        //__pld(inptr0 + 64);
        //__pld(inptr1 + 32);
        //__pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        // cb
        vqs16_U = vsubq_s16(vqs16_U, vqs16_128);
        // cr
        vqs16_V = vsubq_s16(vqs16_V, vqs16_128);

        // r
        vqs16_r = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_402, vqs16_V), 7);

        // b
        vqs16_b = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_772, vqs16_U), 7);

        // g
        vqs16_g = vshrq_n_s16(vmlaq_s16(vmlaq_s16(vqs16_64, vqs16_0_34414, vqs16_U), vqs16_0_71414, vqs16_V), 7);

        vqs16x2_r = vzipq_s16(vqs16_r, vqs16_r);
        vqs16x2_g = vzipq_s16(vqs16_g, vqs16_g);
        vqs16x2_b = vzipq_s16(vqs16_b, vqs16_b);

        vdu8_Y1 = vld1_u8(inptr00);
        vdu8_Y2 = vld1_u8(inptr00+8);

        inptr00 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y1, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_b.val[0]));

        vdu8x3_rgb.val[0] = vdu8_r;
        vdu8x3_rgb.val[1] = vdu8_g;
        vdu8x3_rgb.val[2] = vdu8_b;


        vst3_u8(&(outptr0[RGB_RED]), vdu8x3_rgb);
        outptr0 += 3 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y2, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_b.val[1]));

        vdu8x3_rgb.val[0] = vdu8_r;
        vdu8x3_rgb.val[1] = vdu8_g;
        vdu8x3_rgb.val[2] = vdu8_b;

        vst3_u8(&(outptr0[RGB_RED]), vdu8x3_rgb);
        outptr0 += 3 * 8;

        vdu8_Y1 = vld1_u8(inptr01);
        vdu8_Y2 = vld1_u8(inptr01+8);

        inptr01 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y1, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_b.val[0]));

        vdu8x3_rgb.val[0] = vdu8_r;
        vdu8x3_rgb.val[1] = vdu8_g;
        vdu8x3_rgb.val[2] = vdu8_b;

        vst3_u8(&(outptr1[RGB_RED]), vdu8x3_rgb);
        outptr1 += 3 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y2, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_b.val[1]));

        vdu8x3_rgb.val[0] = vdu8_r;
        vdu8x3_rgb.val[1] = vdu8_g;
        vdu8x3_rgb.val[2] = vdu8_b;

        vst3_u8(&(outptr1[RGB_RED]), vdu8x3_rgb);
        outptr1 += 3 * 8;
    }
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 4 Y values and emit 4 pixels */
        y  = GETJSAMPLE(*inptr00++);
        outptr0[RGB_RED] = range_limit[y + cred];
        outptr0[RGB_GREEN] = range_limit[y + cgreen];
        outptr0[RGB_BLUE] = range_limit[y + cblue];
        outptr0 += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr00++);
        outptr0[RGB_RED] = range_limit[y + cred];
        outptr0[RGB_GREEN] = range_limit[y + cgreen];
        outptr0[RGB_BLUE] = range_limit[y + cblue];
        outptr0 += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr01++);
        outptr1[RGB_RED] = range_limit[y + cred];
        outptr1[RGB_GREEN] = range_limit[y + cgreen];
        outptr1[RGB_BLUE] = range_limit[y + cblue];
        outptr1 += RGB_PIXELSIZE;
        y  = GETJSAMPLE(*inptr01++);
        outptr1[RGB_RED] = range_limit[y + cred];
        outptr1[RGB_GREEN] = range_limit[y + cgreen];
        outptr1[RGB_BLUE] = range_limit[y + cblue];
        outptr1 += RGB_PIXELSIZE;
    }
  /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr00);
        outptr0[RGB_RED] = range_limit[y + cred];
        outptr0[RGB_GREEN] = range_limit[y + cgreen];
        outptr0[RGB_BLUE] = range_limit[y + cblue];
        y  = GETJSAMPLE(*inptr01);
        outptr1[RGB_RED] = range_limit[y + cred];
        outptr1[RGB_GREEN] = range_limit[y + cgreen];
        outptr1[RGB_BLUE] = range_limit[y + cblue];
    }
}



// h2v2_merged_upsample_565_32bit

GLOBAL(void)
h2v2_merged_upsample_565_sub_32bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int32_t coef1[2] = {116130, 91881};
    int32x2_t vds32_coef1 = vld1_s32(coef1);
    int32_t coef2[2] = {-22554, -46802};
    int32x2_t vds32_coef2 = vld1_s32(coef2);
    int32x4_t vqs32_116130 = vdupq_lane_s32(vds32_coef1, 0);
    int32x4_t vqs32_91881 = vdupq_lane_s32(vds32_coef1, 1);
    int32x4_t vqs32_22554_  = vdupq_lane_s32(vds32_coef2, 0);
    int32x4_t vqs32_46802_ = vdupq_lane_s32(vds32_coef2, 1);

    int32_t coef3[2] = {-14831873, -11728001};
    int32x2_t vds32_coef3 = vld1_s32(coef3);
    int32_t coef4[2] = {2919679, 5990656};
    int32x2_t vds32_coef4 = vld1_s32(coef4);
    int32x4_t vqs32_14831873_ = vdupq_lane_s32(vds32_coef3, 0);
    int32x4_t vqs32_11728001_ = vdupq_lane_s32(vds32_coef3, 1);
    int32x4_t vqs32_2919679   = vdupq_lane_s32(vds32_coef4, 0);
    int32x4_t vqs32_5990656   = vdupq_lane_s32(vds32_coef4, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr0, outptr1;
    JSAMPROW inptr00, inptr01, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];
    /* Loop for each group of output pixels */

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x4_t vds16_U_l, vds16_U_h, vds16_V_l, vds16_V_h;
    int32x4_t vqs32_U_l, vqs32_U_h, vqs32_V_l, vqs32_V_h;
    int16x4_t vds16_r_l, vds16_r_h, vds16_g_l, vds16_g_h, vds16_b_l, vds16_b_h;
    int16x4x2_t vds16x2_r_l, vds16x2_r_h, vds16x2_g_l, vds16x2_g_h, vds16x2_b_l, vds16x2_b_h;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr1 += 8;
        inptr2 += 8;

//        __pld(inptr0 + 64);
//        __pld(inptr1 + 32);
//        __pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        vds16_U_l = vget_low_s16(vqs16_U);
        vds16_U_h = vget_high_s16(vqs16_U);
        vds16_V_l = vget_low_s16(vqs16_V);
        vds16_V_h = vget_high_s16(vqs16_V);

        vqs32_U_l = vmovl_s16(vds16_U_l);
        vqs32_U_h = vmovl_s16(vds16_U_h);
        vqs32_V_l = vmovl_s16(vds16_V_l);
        vqs32_V_h = vmovl_s16(vds16_V_h);

        // r
        vds16_r_l = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_l), 16);
        vds16_r_h = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_h), 16);
        // b
        vds16_b_l = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_l), 16);
        vds16_b_h = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_h), 16);
        // g
        vds16_g_l = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_l), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_l)), 16);
        vds16_g_h = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_h), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_h)), 16);

        vds16x2_r_l = vzip_s16(vds16_r_l, vds16_r_l);
        vds16x2_r_h = vzip_s16(vds16_r_h, vds16_r_h);
        vds16x2_g_l = vzip_s16(vds16_g_l, vds16_g_l);
        vds16x2_g_h = vzip_s16(vds16_g_h, vds16_g_h);
        vds16x2_b_l = vzip_s16(vds16_b_l, vds16_b_l);
        vds16x2_b_h = vzip_s16(vds16_b_h, vds16_b_h);

        vdu8_Y1 = vld1_u8(inptr00);
        vdu8_Y2 = vld1_u8(inptr00+8);

        inptr00 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        uint16x8_t vqu16_r_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])), 8);
        uint16x8_t vqu16_r_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])), 8);
        uint16x8_t vqu16_g_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])), 8);
        uint16x8_t vqu16_g_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])), 8);
        uint16x8_t vqu16_b_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])), 8);
        uint16x8_t vqu16_b_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])), 8);

        // Packing RGB565
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_g_l, 5);
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_b_l, 11);
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_l));
        outptr0 += 2 * 8;

        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_g_h, 5);
        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_b_h, 11);
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_h));
        outptr0 += 2 * 8;


        vdu8_Y1 = vld1_u8(inptr01);
        vdu8_Y2 = vld1_u8(inptr01+8);

        inptr01 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vqu16_r_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])), 8);
        vqu16_r_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])), 8);
        vqu16_g_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])), 8);
        vqu16_g_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])), 8);
        vqu16_b_l = vqshluq_n_s16(vaddq_s16(vqs16_Y1, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])), 8);
        vqu16_b_h = vqshluq_n_s16(vaddq_s16(vqs16_Y2, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])), 8);

        // Packing RGB565
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_g_l, 5);
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_b_l, 11);
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_l));
        outptr1 += 2 * 8;

        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_g_h, 5);
        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_b_h, 11);
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_h));
        outptr1 += 2 * 8;
    }
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 4 Y values and emit 4 pixels */
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr0, rgb);
        outptr0 += 4;
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr1, rgb);
        outptr1 += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr00);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr0 = rgb;
        y  = GETJSAMPLE(*inptr01);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr1 = rgb;
    }
}


// h2v2_merged_upsample_565_16bit

GLOBAL(void)
h2v2_merged_upsample_565_sub_16bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int16_t coef[4] = {227, 179, 44, 91};
    int16x4_t vds16_coef1 = vld1_s16(coef);

    int16x8_t vqs16_1_772 = vdupq_lane_s16(vds16_coef1, 0);
    int16x8_t vqs16_1_402 = vdupq_lane_s16(vds16_coef1, 1);
    int16x8_t vqs16_0_34414 = vdupq_lane_s16(vds16_coef1, 2);
    int16x8_t vqs16_0_71414 = vdupq_lane_s16(vds16_coef1, 3);
    int16x8_t vqs16_128 = vmovq_n_s16(128);
    int16x8_t vqs16_64 = vshrq_n_s16(vqs16_128, 1);

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr0, outptr1;
    JSAMPROW inptr00, inptr01, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];
    /* Loop for each group of output pixels */
    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x8_t vqs16_r, vqs16_g, vqs16_b;
    int16x8x2_t vqs16x2_r, vqs16x2_g, vqs16x2_b;
    uint8x8_t vdu8_r, vdu8_g, vdu8_b;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr1 += 8;
        inptr2 += 8;

        //__pld(inptr0 + 64);
        //__pld(inptr1 + 32);
        //__pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        // cb
        vqs16_U = vsubq_s16(vqs16_U, vqs16_128);
        // cr
        vqs16_V = vsubq_s16(vqs16_V, vqs16_128);

        // r
        vqs16_r = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_402, vqs16_V), 7);

        // b
        vqs16_b = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_772, vqs16_U), 7);

        // g
        vqs16_g = vshrq_n_s16(vmlaq_s16(vmlaq_s16(vqs16_64, vqs16_0_34414, vqs16_U), vqs16_0_71414, vqs16_V), 7);

        vqs16x2_r = vzipq_s16(vqs16_r, vqs16_r);
        vqs16x2_g = vzipq_s16(vqs16_g, vqs16_g);
        vqs16x2_b = vzipq_s16(vqs16_b, vqs16_b);

        vdu8_Y1 = vld1_u8(inptr00);
        vdu8_Y2 = vld1_u8(inptr00+8);

        inptr00 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y1, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_b.val[0]));

        uint16x8_t vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        uint16x8_t vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        uint16x8_t vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr0 += 2 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y2, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_b.val[1]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr0 += 2 * 8;

        vdu8_Y1 = vld1_u8(inptr01);
        vdu8_Y2 = vld1_u8(inptr01+8);

        inptr01 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y1, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y1, vqs16x2_b.val[0]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr1 += 2 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_Y2, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_Y2, vqs16x2_b.val[1]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr1 += 2 * 8;
    }
    for (; col < num_cols; col++) {
        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 4 Y values and emit 4 pixels */
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr0, rgb);
        outptr0 += 4;
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr1, rgb);
        outptr1 += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr00);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr0 = rgb;
        y  = GETJSAMPLE(*inptr01);
        r = range_limit[y + cred];
        g = range_limit[y + cgreen];
        b = range_limit[y + cblue];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr1 = rgb;
    }
}


// h2v2_merged_upsample_565D_32bit
GLOBAL(void)
h2v2_merged_upsample_565D_sub_32bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int32_t coef1[2] = {116130, 91881};
    int32x2_t vds32_coef1 = vld1_s32(coef1);
    int32_t coef2[2] = {-22554, -46802};
    int32x2_t vds32_coef2 = vld1_s32(coef2);
    int32x4_t vqs32_116130 = vdupq_lane_s32(vds32_coef1, 0);
    int32x4_t vqs32_91881 = vdupq_lane_s32(vds32_coef1, 1);
    int32x4_t vqs32_22554_  = vdupq_lane_s32(vds32_coef2, 0);
    int32x4_t vqs32_46802_ = vdupq_lane_s32(vds32_coef2, 1);

    int32_t coef3[2] = {-14831873, -11728001};
    int32x2_t vds32_coef3 = vld1_s32(coef3);
    int32_t coef4[2] = {2919679, 5990656};
    int32x2_t vds32_coef4 = vld1_s32(coef4);
    int32x4_t vqs32_14831873_ = vdupq_lane_s32(vds32_coef3, 0);
    int32x4_t vqs32_11728001_ = vdupq_lane_s32(vds32_coef3, 1);
    int32x4_t vqs32_2919679   = vdupq_lane_s32(vds32_coef4, 0);
    int32x4_t vqs32_5990656   = vdupq_lane_s32(vds32_coef4, 1);

    int16_t dither_matrix_neon[4][8] = {{0x0a, 0x02, 0x08, 0x00, 0x0a, 0x02, 0x08, 0x00}
                                ,{0x06, 0x0e, 0x04, 0x0c, 0x06, 0x0e, 0x04, 0x0c}
                                ,{0x09, 0x01, 0x0b, 0x03, 0x09, 0x01, 0x0b, 0x03}
                                ,{0x05, 0x0d, 0x07, 0x0f, 0x05, 0x0d, 0x07, 0x0f}};

    int16x8_t vqs16_dither_matrix0 = vld1q_s16( dither_matrix_neon[0] + ((cinfo->output_scanline % 4) * 8));
    int16x8_t vqs16_dither_matrix1 = vld1q_s16( dither_matrix_neon[0] + (((cinfo->output_scanline+1) % 4) * 8));

    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr0, outptr1;
    JSAMPROW inptr00, inptr01, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    //JDIMENSION col_index = 0;
    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    INT32 d1 = dither_matrix[(cinfo->output_scanline+1) & DITHER_MASK];
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];
    /* Loop for each group of output pixels */

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x4_t vds16_U_l, vds16_U_h, vds16_V_l, vds16_V_h;
    int32x4_t vqs32_U_l, vqs32_U_h, vqs32_V_l, vqs32_V_h;
    int16x4_t vds16_r_l, vds16_r_h, vds16_g_l, vds16_g_h, vds16_b_l, vds16_b_h;
    int16x4x2_t vds16x2_r_l, vds16x2_r_h, vds16x2_g_l, vds16x2_g_h, vds16x2_b_l, vds16x2_b_h;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr1 += 8;
        inptr2 += 8;

//        __pld(inptr0 + 64);
//        __pld(inptr1 + 32);
//        __pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        vds16_U_l = vget_low_s16(vqs16_U);
        vds16_U_h = vget_high_s16(vqs16_U);
        vds16_V_l = vget_low_s16(vqs16_V);
        vds16_V_h = vget_high_s16(vqs16_V);

        vqs32_U_l = vmovl_s16(vds16_U_l);
        vqs32_U_h = vmovl_s16(vds16_U_h);
        vqs32_V_l = vmovl_s16(vds16_V_l);
        vqs32_V_h = vmovl_s16(vds16_V_h);

        // r
        vds16_r_l = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_l), 16);
        vds16_r_h = vshrn_n_s32(vmlaq_s32(vqs32_11728001_, vqs32_91881, vqs32_V_h), 16);
        // b
        vds16_b_l = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_l), 16);
        vds16_b_h = vshrn_n_s32(vmlaq_s32(vqs32_14831873_, vqs32_116130, vqs32_U_h), 16);
        // g
        vds16_g_l = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_l), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_l)), 16);
        vds16_g_h = vshrn_n_s32(vaddq_s32(vmlaq_s32(vqs32_2919679, vqs32_22554_, vqs32_U_h), vmlaq_s32(vqs32_5990656, vqs32_46802_, vqs32_V_h)), 16);

        vds16x2_r_l = vzip_s16(vds16_r_l, vds16_r_l);
        vds16x2_r_h = vzip_s16(vds16_r_h, vds16_r_h);
        vds16x2_g_l = vzip_s16(vds16_g_l, vds16_g_l);
        vds16x2_g_h = vzip_s16(vds16_g_h, vds16_g_h);
        vds16x2_b_l = vzip_s16(vds16_b_l, vds16_b_l);
        vds16x2_b_h = vzip_s16(vds16_b_h, vds16_b_h);

        vdu8_Y1 = vld1_u8(inptr00);
        vdu8_Y2 = vld1_u8(inptr00+8);

        inptr00 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        // dither
        int16x8_t vqs16_y1_rb = vaddq_s16(vqs16_Y1, vqs16_dither_matrix0);
        int16x8_t vqs16_y1_g = vaddq_s16(vqs16_Y1, vshrq_n_s16(vqs16_dither_matrix0, 1));
        int16x8_t vqs16_y2_rb = vaddq_s16(vqs16_Y2, vqs16_dither_matrix0);
        int16x8_t vqs16_y2_g = vaddq_s16(vqs16_Y2, vshrq_n_s16(vqs16_dither_matrix0, 1));

        uint16x8_t vqu16_r_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_rb, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])), 8);
        uint16x8_t vqu16_r_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_rb, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])), 8);
        uint16x8_t vqu16_g_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_g, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])), 8);
        uint16x8_t vqu16_g_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_g, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])), 8);
        uint16x8_t vqu16_b_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_rb, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])), 8);
        uint16x8_t vqu16_b_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_rb, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])), 8);

        // Packing RGB565
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_g_l, 5);
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_b_l, 11);
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_l));
        outptr0 += 2 * 8;

        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_g_h, 5);
        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_b_h, 11);
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_h));
        outptr0 += 2 * 8;


        vdu8_Y1 = vld1_u8(inptr01);
        vdu8_Y2 = vld1_u8(inptr01+8);

        inptr01 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        // dither
        vqs16_y1_rb = vaddq_s16(vqs16_Y1, vqs16_dither_matrix1);
        vqs16_y1_g = vaddq_s16(vqs16_Y1, vshrq_n_s16(vqs16_dither_matrix1, 1));
        vqs16_y2_rb = vaddq_s16(vqs16_Y2, vqs16_dither_matrix1);
        vqs16_y2_g = vaddq_s16(vqs16_Y2, vshrq_n_s16(vqs16_dither_matrix1, 1));

        vqu16_r_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_rb, vcombine_s16(vds16x2_r_l.val[0], vds16x2_r_l.val[1])), 8);
        vqu16_r_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_rb, vcombine_s16(vds16x2_r_h.val[0], vds16x2_r_h.val[1])), 8);
        vqu16_g_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_g, vcombine_s16(vds16x2_g_l.val[0], vds16x2_g_l.val[1])), 8);
        vqu16_g_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_g, vcombine_s16(vds16x2_g_h.val[0], vds16x2_g_h.val[1])), 8);
        vqu16_b_l = vqshluq_n_s16(vaddq_s16(vqs16_y1_rb, vcombine_s16(vds16x2_b_l.val[0], vds16x2_b_l.val[1])), 8);
        vqu16_b_h = vqshluq_n_s16(vaddq_s16(vqs16_y2_rb, vcombine_s16(vds16x2_b_h.val[0], vds16x2_b_h.val[1])), 8);

        // Packing RGB565
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_g_l, 5);
        vqu16_r_l = vsriq_n_u16(vqu16_r_l, vqu16_b_l, 11);
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_l));
        outptr1 += 2 * 8;

        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_g_h, 5);
        vqu16_r_h = vsriq_n_u16(vqu16_r_h, vqu16_b_h, 11);
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r_h));
        outptr1 += 2 * 8;
    }
    for (; col < num_cols; col++) {

        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 4 Y values and emit 4 pixels */
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[DITHER_565_R(y + cred, d1)];
        g = range_limit[DITHER_565_G(y + cgreen, d1)];
        b = range_limit[DITHER_565_B(y + cblue, d1)];
        d1 = DITHER_ROTATE(d1);
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr0, rgb);
        outptr0 += 4;
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[DITHER_565_R(y + cred, d1)];
        g = range_limit[DITHER_565_G(y + cgreen, d1)];
        b = range_limit[DITHER_565_B(y + cblue, d1)];
        d1 = DITHER_ROTATE(d1);
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr1, rgb);
        outptr1 += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr00);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr0 = rgb;
        y  = GETJSAMPLE(*inptr01);
        r = range_limit[DITHER_565_R(y + cred, d1)];
        g = range_limit[DITHER_565_G(y + cgreen, d1)];
        b = range_limit[DITHER_565_B(y + cblue, d1)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr1 = rgb;
    }
}

// h2v2_merged_upsample_565D_16bit
GLOBAL(void)
h2v2_merged_upsample_565D_sub_16bit (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
    int16_t coef[4] = {227, 179, 44, 91};
    int16x4_t vds16_coef1 = vld1_s16(coef);

    int16x8_t vqs16_1_772 = vdupq_lane_s16(vds16_coef1, 0);
    int16x8_t vqs16_1_402 = vdupq_lane_s16(vds16_coef1, 1);
    int16x8_t vqs16_0_34414 = vdupq_lane_s16(vds16_coef1, 2);
    int16x8_t vqs16_0_71414 = vdupq_lane_s16(vds16_coef1, 3);
    int16x8_t vqs16_128 = vmovq_n_s16(128);
    int16x8_t vqs16_64 = vshrq_n_s16(vqs16_128, 1);

    int16_t dither_matrix_neon[4][8] = {{0x0a, 0x02, 0x08, 0x00, 0x0a, 0x02, 0x08, 0x00}
                                    ,{0x06, 0x0e, 0x04, 0x0c, 0x06, 0x0e, 0x04, 0x0c}
                                    ,{0x09, 0x01, 0x0b, 0x03, 0x09, 0x01, 0x0b, 0x03}
                                    ,{0x05, 0x0d, 0x07, 0x0f, 0x05, 0x0d, 0x07, 0x0f}};


    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr;
    register JSAMPROW outptr0, outptr1;
    JSAMPROW inptr00, inptr01, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    //JDIMENSION col_index = 0;
    INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
    INT32 d1 = dither_matrix[(cinfo->output_scanline+1) & DITHER_MASK];
    unsigned int r, g, b;
    INT32 rgb;
    SHIFT_TEMPS

    int16x8_t vqs16_dither_matrix0 = vld1q_s16( dither_matrix_neon[0] + ((cinfo->output_scanline % 4) * 8));
    int16x8_t vqs16_dither_matrix1 = vld1q_s16( dither_matrix_neon[0] + (((cinfo->output_scanline+1) % 4) * 8));

    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];
    /* Loop for each group of output pixels */

    uint8x8_t vdu8_Y1, vdu8_Y2, vdu8_U, vdu8_V;
    int16x8_t vqs16_Y1, vqs16_Y2, vqs16_U, vqs16_V;
    int16x8_t vqs16_r, vqs16_g, vqs16_b;
    int16x8x2_t vqs16x2_r, vqs16x2_g, vqs16x2_b;
    uint8x8_t vdu8_r, vdu8_g, vdu8_b;

    JDIMENSION num_cols = cinfo->output_width >> 1;

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col+=8)
    {
        vdu8_U = vld1_u8(inptr1);
        vdu8_V = vld1_u8(inptr2);

        inptr1 += 8;
        inptr2 += 8;

        //__pld(inptr0 + 64);
        //__pld(inptr1 + 32);
        //__pld(inptr2 + 32);

        vqs16_U = vreinterpretq_s16_u16(vmovl_u8(vdu8_U));
        vqs16_V = vreinterpretq_s16_u16(vmovl_u8(vdu8_V));

        // cb
        vqs16_U = vsubq_s16(vqs16_U, vqs16_128);
        // cr
        vqs16_V = vsubq_s16(vqs16_V, vqs16_128);

        // r
        vqs16_r = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_402, vqs16_V), 7);

        // b
        vqs16_b = vshrq_n_s16(vmlaq_s16(vqs16_64, vqs16_1_772, vqs16_U), 7);

        // g
        vqs16_g = vshrq_n_s16(vmlaq_s16(vmlaq_s16(vqs16_64, vqs16_0_34414, vqs16_U), vqs16_0_71414, vqs16_V), 7);

        vqs16x2_r = vzipq_s16(vqs16_r, vqs16_r);
        vqs16x2_g = vzipq_s16(vqs16_g, vqs16_g);
        vqs16x2_b = vzipq_s16(vqs16_b, vqs16_b);

        vdu8_Y1 = vld1_u8(inptr00);
        vdu8_Y2 = vld1_u8(inptr00+8);

        inptr00 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        // dither
        int16x8_t vqs16_y1_rb = vaddq_s16(vqs16_Y1, vqs16_dither_matrix0);
        int16x8_t vqs16_y1_g = vaddq_s16(vqs16_Y1, vshrq_n_s16(vqs16_dither_matrix0, 1));
        int16x8_t vqs16_y2_rb = vaddq_s16(vqs16_Y2, vqs16_dither_matrix0);
        int16x8_t vqs16_y2_g = vaddq_s16(vqs16_Y2, vshrq_n_s16(vqs16_dither_matrix0, 1));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_y1_rb, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y1_g, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_y1_rb, vqs16x2_b.val[0]));

        uint16x8_t vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        uint16x8_t vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        uint16x8_t vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr0 += 2 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_y2_rb, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y2_g, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_y2_rb, vqs16x2_b.val[1]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr0[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr0 += 2 * 8;

        vdu8_Y1 = vld1_u8(inptr01);
        vdu8_Y2 = vld1_u8(inptr01+8);

        inptr01 += 16;

        vqs16_Y1 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y1));
        vqs16_Y2 = vreinterpretq_s16_u16(vmovl_u8(vdu8_Y2));

        // dither
        vqs16_y1_rb = vaddq_s16(vqs16_Y1, vqs16_dither_matrix1);
        vqs16_y1_g = vaddq_s16(vqs16_Y1, vshrq_n_s16(vqs16_dither_matrix1, 1));
        vqs16_y2_rb = vaddq_s16(vqs16_Y2, vqs16_dither_matrix1);
        vqs16_y2_g = vaddq_s16(vqs16_Y2, vshrq_n_s16(vqs16_dither_matrix1, 1));

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_y1_rb, vqs16x2_r.val[0]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y1_g, vqs16x2_g.val[0]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_y1_rb, vqs16x2_b.val[0]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr1 += 2 * 8;

        vdu8_r = vqmovun_s16(vaddq_s16(vqs16_y2_rb, vqs16x2_r.val[1]));
        vdu8_g = vqmovun_s16(vsubq_s16(vqs16_y2_g, vqs16x2_g.val[1]));
        vdu8_b = vqmovun_s16(vaddq_s16(vqs16_y2_rb, vqs16x2_b.val[1]));

        vqu16_r = vshlq_n_u16(vmovl_u8(vdu8_r), 8);
        vqu16_g = vshlq_n_u16(vmovl_u8(vdu8_g), 8);
        vqu16_b = vshlq_n_u16(vmovl_u8(vdu8_b), 8);

        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_g, 5);
        vqu16_r = vsriq_n_u16(vqu16_r, vqu16_b, 11);

        // Store RGB565 and Increase Each Destination Pointers
        vst1q_u8(&(outptr1[RGB_RED]), vreinterpretq_u8_u16(vqu16_r));

        outptr1 += 2 * 8;
    }
    for (; col < num_cols; col++) {

        /* Do the chroma part of the calculation */
        cb = GETJSAMPLE(*inptr1++);
        cr = GETJSAMPLE(*inptr2++);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        /* Fetch 4 Y values and emit 4 pixels */
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr00++);
        r = range_limit[DITHER_565_R(y + cred, d1)];
        g = range_limit[DITHER_565_G(y + cgreen, d1)];
        b = range_limit[DITHER_565_B(y + cblue, d1)];
        d1 = DITHER_ROTATE(d1);
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr0, rgb);
        outptr0 += 4;
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        d0 = DITHER_ROTATE(d0);
        rgb = PACK_SHORT_565(r,g,b);
        y  = GETJSAMPLE(*inptr01++);
        r = range_limit[DITHER_565_R(y + cred, d1)];
        g = range_limit[DITHER_565_G(y + cgreen, d1)];
        b = range_limit[DITHER_565_B(y + cblue, d1)];
        d1 = DITHER_ROTATE(d1);
        rgb = PACK_TWO_PIXELS(rgb, PACK_SHORT_565(r,g,b));
        WRITE_TWO_PIXELS(outptr1, rgb);
        outptr1 += 4;
    }
    /* If image width is odd, do the last output column separately */
    if (cinfo->output_width & 1) {
        cb = GETJSAMPLE(*inptr1);
        cr = GETJSAMPLE(*inptr2);
        cred = Crrtab[cr];
        cgreen = (int) RIGHT_SHIFT(Cbgtab[cb] + Crgtab[cr], SCALEBITS);
        cblue = Cbbtab[cb];
        y  = GETJSAMPLE(*inptr00);
        r = range_limit[DITHER_565_R(y + cred, d0)];
        g = range_limit[DITHER_565_G(y + cgreen, d0)];
        b = range_limit[DITHER_565_B(y + cblue, d0)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr0 = rgb;
        y  = GETJSAMPLE(*inptr01);
        r = range_limit[DITHER_565_R(y + cred, d1)];
        g = range_limit[DITHER_565_G(y + cgreen, d1)];
        b = range_limit[DITHER_565_B(y + cblue, d1)];
        rgb = PACK_SHORT_565(r,g,b);
        *(INT16*)outptr1 = rgb;
    }
}

#endif

#endif /* UPSAMPLE_MERGING_SUPPORTED */

#endif
