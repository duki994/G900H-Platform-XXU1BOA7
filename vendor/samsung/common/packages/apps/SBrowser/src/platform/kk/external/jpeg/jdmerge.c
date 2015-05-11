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
 *	R = Y           + K1 * Cr
 *	G = Y + K2 * Cb + K3 * Cr
 *	B = Y + K4 * Cb
 * only the Y term varies among the group of pixels corresponding to a pair
 * of chroma samples, so the rest of the terms can be calculated just once.
 * At typical sampling ratios, this eliminates half or three-quarters of the
 * multiplications needed for color conversion.
 *
 * This file currently provides implementations for the following cases:
 *	YCbCr => RGB color conversion only.
 *	Sampling ratios of 2h1v or 2h2v.
 *	No scaling needed at upsample time.
 *	Corner-aligned (non-CCIR601) sampling alignment.
 * Other special cases could be added, but in most applications these are
 * the only common cases.  (For uncommon cases we fall back on the more
 * general code in jdsample.c and jdcolor.c.)
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include <utils/Log.h>

#if __ARM_HAVE_NEON
#include "arm_neon.h"
#endif
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
  struct jpeg_upsampler pub;	/* public fields */

  /* Pointer to routine to do actual upsampling/conversion of one row group */
  JMETHOD(void, upmethod, (j_decompress_ptr cinfo,
			   JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
			   JSAMPARRAY output_buf));

  /* Private state for YCC->RGB conversion */
  int * Cr_r_tab;		/* => table for Cr to R conversion */
  int * Cb_b_tab;		/* => table for Cb to B conversion */
  INT32 * Cr_g_tab;		/* => table for Cr to G conversion */
  INT32 * Cb_g_tab;		/* => table for Cb to G conversion */

  /* For 2:1 vertical sampling, we produce two output rows at a time.
   * We need a "spare" row buffer to hold the second output row if the
   * application provides just a one-row buffer; we also use the spare
   * to discard the dummy last row if the image height is odd.
   */
  JSAMPROW spare_row;
  boolean spare_full;		/* T if spare buffer is occupied */

  JDIMENSION out_row_width;	/* samples per output row */
  JDIMENSION rows_to_go;	/* counts rows remaining in image */
} my_upsampler;

typedef my_upsampler * my_upsample_ptr;

#define SCALEBITS	16	/* speediest right-shift on some machines */
#define ONE_HALF	((INT32) 1 << (SCALEBITS-1))
#define FIX(x)		((INT32) ((x) * (1L<<SCALEBITS) + 0.5))

extern void
h2v1_merged_upsample_565_sub_16bit(j_decompress_ptr, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);
extern void
h2v2_merged_upsample_565_sub_16bit(j_decompress_ptr, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);
extern void
h2v1_merged_upsample_565D_sub_16bit(j_decompress_ptr, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);
extern void
h2v2_merged_upsample_565D_sub_16bit(j_decompress_ptr, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

/*
 * Initialize tables for YCC->RGB colorspace conversion.
 * This is taken directly from jdcolor.c; see that file for more info.
 */

LOCAL(void)
build_ycc_rgb_table (j_decompress_ptr cinfo)
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
  int i;
  INT32 x;
  SHIFT_TEMPS

  upsample->Cr_r_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(int));
  upsample->Cb_b_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(int));
  upsample->Cr_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(INT32));
  upsample->Cb_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(INT32));

  for (i = 0, x = -CENTERJSAMPLE; i <= MAXJSAMPLE; i++, x++) {
    /* i is the actual input pixel value, in the range 0..MAXJSAMPLE */
    /* The Cb or Cr value we are thinking of is x = i - CENTERJSAMPLE */
    /* Cr=>R value is nearest int to 1.40200 * x */
    upsample->Cr_r_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.40200) * x + ONE_HALF, SCALEBITS);
    /* Cb=>B value is nearest int to 1.77200 * x */
    upsample->Cb_b_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.77200) * x + ONE_HALF, SCALEBITS);
    /* Cr=>G value is scaled-up -0.71414 * x */
    upsample->Cr_g_tab[i] = (- FIX(0.71414)) * x;
    /* Cb=>G value is scaled-up -0.34414 * x */
    /* We also add in ONE_HALF so that need not do it in inner loop */
    upsample->Cb_g_tab[i] = (- FIX(0.34414)) * x + ONE_HALF;
  }
}


/*
 * Initialize for an upsampling pass.
 */

METHODDEF(void)
start_pass_merged_upsample (j_decompress_ptr cinfo)
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;

  /* Mark the spare buffer empty */
  upsample->spare_full = FALSE;
  /* Initialize total-height counter for detecting bottom of image */
  upsample->rows_to_go = cinfo->output_height;
}


/*
 * Control routine to do upsampling (and color conversion).
 *
 * The control routine just handles the row buffering considerations.
 */

METHODDEF(void)
merged_2v_upsample (j_decompress_ptr cinfo,
		    JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
		    JDIMENSION in_row_groups_avail,
		    JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
		    JDIMENSION out_rows_avail)
/* 2:1 vertical sampling case: may need a spare row. */
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
  JSAMPROW work_ptrs[2];
  JDIMENSION num_rows;		/* number of rows returned to caller */

  if (upsample->spare_full) {
    /* If we have a spare row saved from a previous cycle, just return it. */
      JDIMENSION size = upsample->out_row_width;
#ifdef ANDROID_RGB
    if (cinfo->out_color_space == JCS_RGB_565)
      size = cinfo->output_width*2;
#endif
    jcopy_sample_rows(& upsample->spare_row, 0, output_buf + *out_row_ctr, 0,
		      1, size);

    num_rows = 1;
    upsample->spare_full = FALSE;
  } else {
    /* Figure number of rows to return to caller. */
    num_rows = 2;
    /* Not more than the distance to the end of the image. */
    if (num_rows > upsample->rows_to_go)
      num_rows = upsample->rows_to_go;
    /* And not more than what the client can accept: */
    out_rows_avail -= *out_row_ctr;
    if (num_rows > out_rows_avail)
      num_rows = out_rows_avail;
    /* Create output pointer array for upsampler. */
    work_ptrs[0] = output_buf[*out_row_ctr];
    if (num_rows > 1) {
      work_ptrs[1] = output_buf[*out_row_ctr + 1];
    } else {
      work_ptrs[1] = upsample->spare_row;
      upsample->spare_full = TRUE;
    }
    /* Now do the upsampling. */
    (*upsample->upmethod) (cinfo, input_buf, *in_row_group_ctr, work_ptrs);
  }

  /* Adjust counts */
  *out_row_ctr += num_rows;
  upsample->rows_to_go -= num_rows;
  /* When the buffer is emptied, declare this input row group consumed */
  if (! upsample->spare_full)
    (*in_row_group_ctr)++;
}


METHODDEF(void)
merged_1v_upsample (j_decompress_ptr cinfo,
		    JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
		    JDIMENSION in_row_groups_avail,
		    JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
		    JDIMENSION out_rows_avail)
/* 1:1 vertical sampling case: much easier, never need a spare row. */
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;

  /* Just do the upsampling. */
  (*upsample->upmethod) (cinfo, input_buf, *in_row_group_ctr,
			 output_buf + *out_row_ctr);
  /* Adjust counts */
  (*out_row_ctr)++;
  (*in_row_group_ctr)++;
}


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

METHODDEF(void)
h2v1_merged_upsample (j_decompress_ptr cinfo,
		      JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
		      JSAMPARRAY output_buf)
{

#ifdef SIMD_16BIT

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
    JDIMENSION num_cols = cinfo->output_width >> 1;
    const short constVal[4] = { 227, 44, 179, 91};

    SHIFT_TEMPS
    asm volatile(
         "VMOV.I16 q4, #0x80  \n\t"
          "MOV r0, %[constVal]  \n\t"
          "VLD1.16 {d30}, [r0]  \n\t"
          "VDUP.16 q0, d30[0]   \n\t"
          "VDUP.16 q2, d30[1]   \n\t"
          "VDUP.16 q1, d30[2]  \n\t"
          "VDUP.16 q3, d30[3]  \n\t"

          ::[constVal] "r" (constVal)
        : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
        "s17", "s18", "s19", "d30", "d31", "r0"
          );


    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];


    for (col = 0; col < (num_cols - (num_cols & 0x7)); col= col + 8)
    {
    asm volatile (

                "ADD r0, %[inptr1], %[col]    \n\t"
                "VLD1.8      {d12}, [r0]         \n\t"
                "ADD r0, %[inptr2], %[col]    \n\t"
                "VLD1.8      {d13}, [r0]         \n\t"
                "MOV      r0, %[inptr0]        \n\t"
                "VLD1.8      {d10}, [r0]!         \n\t"
                "VLD1.8      {d11}, [r0]         \n\t"

                "VMOVL.U8 q7,d12            \n\t"
                "VMOVL.U8 q8,d13            \n\t"
                "VSUB.I16 q9,q7,q4             \n\t"
                "VSUB.I16 q10,q8,q4         \n\t"

                "VSHR.S16 q11,q4,#1            \n\t"
                "VMLA.I16 q11,q1,q10        \n\t"
                "VSHR.S16 q11,q11,#7          \n\t"

                "VSHR.S16 q12,q4,#1          \n\t"
                "VMLA.I16 q12,q0,q9            \n\t"
                "VSHR.S16 q12,q12,#7          \n\t"

                "VSHR.S16 q13,q4,#1            \n\t"
                "VMLA.I16 q13,q2,q9            \n\t"
                "VMLA.I16 q13,q3,q10        \n\t"
                "VSHR.S16 q13,q13,#7          \n\t"

                "VMOV     q6,q11            \n\t"
                "VZIP.16  q6,q11            \n\t"
                "VMOV     q7,q12            \n\t"
                "VZIP.16  q7,q12            \n\t"
                "VMOV     q8,q13            \n\t"
                "VZIP.16  q8,q13            \n\t"

                "VMOVL.U8 q9,d10            \n\t"

                "VMOVL.U8 q10,d11            \n\t"

                "VADD.I16 q14,q9,q6         \n\t"
                "VQMOVUN.S16 d10,q14         \n\t"

                "VSUB.I16 q15,q9,q8         \n\t"
                "VQMOVUN.S16 d11,q15         \n\t"

                "VADD.I16 q14,q9,q7         \n\t"
                "VQMOVUN.S16 d12,q14         \n\t"

                "MOV    r0, %[outptr]        \n\t"
                "VST3.8 {d10,d11,d12}, [r0]! \n\t"


                "VADD.I16 q14,q10,q11         \n\t"
                "VQMOVUN.S16 d22,q14         \n\t"

                "VSUB.I16 q15,q10,q13         \n\t"
                "VQMOVUN.S16 d23,q15         \n\t"

                "VADD.I16 q14,q10,q12         \n\t"
                "VQMOVUN.S16 d24,q14         \n\t"


                "VST3.8 {d22,d23,d24}, [r0]  \n\t"

                ::[outptr] "r" (outptr), [inptr0] "r" (inptr0), [inptr1] "r" (inptr1), [inptr2] "r" (inptr2), [col] "r" (col)
                : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
                "s17", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
                "d16", "d17", "d18", "d19", "d20", "d21", "d22",    "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "r0"
                );
                outptr += RGB_PIXELSIZE * 16;
                inptr0 = inptr0 + 16;

        }
        inptr1 = inptr1 + col;
        inptr2 = inptr2 + col;

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
        outptr[RGB_BLUE] = range_limit[y + cblue];
        outptr += RGB_PIXELSIZE;
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

#elif defined SIMD_32BIT /* 32 bit code */

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
    JDIMENSION num_cols = cinfo->output_width >> 1;
    const int constVal1[4] = { 116130, 91881, -22554, -46802};
     const int constVal2[4] = {-14831873, -11728001,2919679, 5990656};
    SHIFT_TEMPS
     asm volatile(
         "MOV r0, %[constVal1]  \n\t"
          "VLD1.32 {q15}, [r0]  \n\t"
          "VDUP.32 q0, d30[0]   \n\t"
          "VDUP.32 q1, d30[1]   \n\t"
         "VDUP.32 q2, d31[0]  \n\t"
         "VDUP.32 q3, d31[1]  \n\t"
        "MOV r0, %[constVal2]  \n\t"
          "VLD1.32 {q4}, [r0]  \n\t"

          ::[constVal1] "r" (constVal1), [constVal2] "r" (constVal2)
        : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
        "s17", "s18", "s19", "d30", "d31", "r0"
          );

    inptr0 = input_buf[0][in_row_group_ctr];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr = output_buf[0];

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col= col + 8)
    {
        asm volatile (

            "ADD r0, %[inptr1], %[col]        \n\t"
            "VLD1.8      {d12}, [r0]             \n\t"
            "ADD r0, %[inptr2], %[col]        \n\t"
            "VLD1.8      {d13}, [r0]             \n\t"

            "VMOVL.U8 q7,d12                \n\t"
            "VMOVL.U8  q8,d13                \n\t"
            "VMOVL.S16 q9,d16                \n\t"
            "VMOVL.S16 q10,d17               \n\t"

            "VDUP.32 q11,d8[1]                 \n\t"
            "VMLA.I32  q11,q1,q9              \n\t"
            "VSHRN.I32 d10,q11,#16              \n\t"

            "VDUP.32 q11,d8[1]                 \n\t"
            "VMLA.I32  q11,q1,q10             \n\t"
            "VSHRN.I32 d11,q11,#16              \n\t"

            "VMOVL.S16 q11,d14                \n\t"
            "VMOVL.S16 q12,d15                \n\t"

            "VDUP.32 q13,d8[0]                  \n\t"
            "VMLA.I32  q13,q0,q11             \n\t"
            "VSHRN.I32 d12,q13,#16              \n\t"

            "VDUP.32 q13,d8[0]                  \n\t"
            "VMLA.I32  q13,q0,q12             \n\t"
            "VSHRN.I32 d13,q13,#16              \n\t"

            "VDUP.32 q14,d9[0]                  \n\t"
            "VDUP.32 q15,d9[1]                  \n\t"
            "VMLA.I32 q14,q2,q11              \n\t"
            "VMLA.I32 q15,q3,q9               \n\t"
            "VADD.I32 q15,q14,q15             \n\t"
            "VSHRN.I32 d14,q15,#16              \n\t"

            "VDUP.32 q14,d9[0]                  \n\t"
            "VDUP.32 q15,d9[1]                  \n\t"
            "VMLA.I32 q14,q2,q12              \n\t"
            "VMLA.I32 q15,q3,q10              \n\t"
            "VADD.I32 q15,q14,q15             \n\t"
            "VSHRN.I32 d15,q15,#16              \n\t"

            "VMOV.16  d16,d11                    \n\t"
            "VMOV.16  d11,d10                    \n\t"
            "VZIP.16  d10,d11                    \n\t"

            "VMOV.16  d17,d16                    \n\t"
            "VZIP.16  d16,d17                    \n\t"

            "VMOV.16  d18,d15                    \n\t"
            "VMOV.16  d15,d14                    \n\t"
            "VZIP.16  d14,d15                    \n\t"

            "VMOV.16  d19,d18                    \n\t"
            "VZIP.16  d18,d19                    \n\t"

            "VMOV.16  d20,d13                    \n\t"
            "VMOV.16  d13,d12                    \n\t"
            "VZIP.16  d12,d13                    \n\t"

            "VMOV.16  d21,d20                    \n\t"
            "VZIP.16  d20,d21                    \n\t"
            "MOV      r0, %[inptr0]                \n\t"
            "VLD1.8      {d24}, [r0]!                 \n\t"

            "VLD1.8      {d25}, [r0]                 \n\t"

            "VMOVL.U8  q11,d24                    \n\t"

            "VMOVL.U8  q12,d25                    \n\t"

            "VADD.I16 q13,q11,q5                 \n\t"
            "VQMOVUN.S16 d10,q13                \n\t"
            "VADD.I16 q14,q12,q8                 \n\t"
            "VQMOVUN.S16 d16,q14                \n\t"

            "VADD.I16 q13,q11,q7                 \n\t"
            "VQMOVUN.S16 d11,q13                \n\t"

            "VADD.I16 q14,q12,q9                 \n\t"
            "VQMOVUN.S16 d17,q14                \n\t"

            "VADD.I16 q13,q11,q6                 \n\t"
            "VQMOVUN.S16 d12,q13                \n\t"

            "VADD.I16 q14,q12,q10                 \n\t"
            "VQMOVUN.S16 d18,q14                \n\t"

            "MOV    r0, %[outptr]                \n\t"
            "VST3.8 {d10,d11,d12}, [r0]!         \n\t"
            "VST3.8   {d16,d17,d18},[r0]         \n\t"

            ::[outptr] "r" (outptr),[inptr0] "r" (inptr0), [inptr1] "r" (inptr1), [inptr2] "r" (inptr2), [col] "r" (col)
            : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
            "s17", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
            "d16", "d17", "d18", "d19", "d20", "d21", "d22",    "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "r0"


            );
        outptr += RGB_PIXELSIZE * 16;
        inptr0 = inptr0 + 16;

        }
        inptr1 = inptr1 + col;
        inptr2 = inptr2 + col;

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
        outptr[RGB_BLUE] = range_limit[y + cblue];
        outptr += RGB_PIXELSIZE;
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
#else
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
  /* Loop for each pair of output pixels */
  for (col = cinfo->output_width >> 1; col > 0; col--) {
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
    outptr[RGB_BLUE] = range_limit[y + cblue];
    outptr += RGB_PIXELSIZE;
  }
  /* If image width is odd, do the last output column separately */
  if (cinfo->output_width & 1) {
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
#endif

}


#ifdef ANDROID_RGB
METHODDEF(void)
h2v1_merged_upsample_565 (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
#if defined SIMD_16BIT // sub function call
    h2v1_merged_upsample_565_sub_16bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#elif defined SIMD_32BIT
    h2v1_merged_upsample_565_sub_32bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#else
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
  /* Loop for each pair of output pixels */
  for (col = cinfo->output_width >> 1; col > 0; col--) {
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
#endif
}


METHODDEF(void)
h2v1_merged_upsample_565D (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
#if defined SIMD_16BIT // sub function call
    h2v1_merged_upsample_565D_sub_16bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#elif defined SIMD_32BIT
    h2v1_merg ed_upsample_565D_sub_32bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#else
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
  JDIMENSION col_index = 0;
  INT32 d0 = dither_matrix[cinfo->output_scanline & DITHER_MASK];
  unsigned int r, g, b;
  INT32 rgb;
  SHIFT_TEMPS

  inptr0 = input_buf[0][in_row_group_ctr];
  inptr1 = input_buf[1][in_row_group_ctr];
  inptr2 = input_buf[2][in_row_group_ctr];
  outptr = output_buf[0];
  /* Loop for each pair of output pixels */
  for (col = cinfo->output_width >> 1; col > 0; col--) {
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
#endif
}


#endif

/*
 * Upsample and color convert for the case of 2:1 horizontal and 2:1 vertical.
 */

METHODDEF(void)
h2v2_merged_upsample (j_decompress_ptr cinfo,
		      JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
		      JSAMPARRAY output_buf)
{

#ifdef SIMD_16BIT

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
    /* Loop for each group of output pixels */
    JDIMENSION num_cols = cinfo->output_width >> 1;
    const short constVal[4] = { 227, 44, 179, 91};
    SHIFT_TEMPS
    asm volatile(
        "VMOV.I16 q4, #0x80  \n\t"
          "MOV r0, %[constVal]  \n\t"
          "VLD1.16 {d30}, [r0]  \n\t"
          "VDUP.16 q0, d30[0]   \n\t"
          "VDUP.16 q2, d30[1]   \n\t"
          "VDUP.16 q1, d30[2]  \n\t"
          "VDUP.16 q3, d30[3]  \n\t"
          ::[constVal] "r" (constVal)
        : "cc", "memory",  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
        "s17", "s18", "s19", "d30", "d31", "r0"
          );


    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];
    for (col = 0; col < (num_cols - (num_cols & 0x7)); col= col + 8)
    {
        asm volatile (

            "ADD r0, %[inptr1], %[col]        \n\t"
            "VLD1.8      {d12}, [r0]             \n\t"
            "ADD r0, %[inptr2], %[col]        \n\t"
            "VLD1.8      {d13}, [r0]             \n\t"
            "VMOVL.U8 q7,d12                \n\t"
            "VMOVL.U8 q8,d13                \n\t"
            "VSUB.I16 q9,q7,q4                 \n\t"
            "VSUB.I16 q10,q8,q4             \n\t"

            "VSHR.S16 q11,q4,#1                \n\t"
            "VMLA.I16 q11,q1,q10            \n\t"
            "VSHR.S16 q11,q11,#7              \n\t"

            "VSHR.S16 q12,q4,#1                \n\t"
            "VMLA.I16 q12,q0,q9                \n\t"
            "VSHR.S16 q12,q12,#7              \n\t"

            "VSHR.S16 q13,q4,#1                \n\t"
            "VMLA.I16 q13,q2,q9                \n\t"
            "VMLA.I16 q13,q3,q10            \n\t"
            "VSHR.S16 q13,q13,#7              \n\t"
            "MOV      r0, %[inptr00]        \n\t"
            "VLD1.8      {d12}, [r0]!             \n\t"
            "VLD1.8      {d13}, [r0]             \n\t"
            "VMOVL.U8 q14,d12                \n\t"
            "VMOVL.U8 q15,d13                \n\t"

            "VMOV     q5,q11                \n\t"
            "VZIP.16  q5,q11                \n\t"
            "VMOV     q6,q12                \n\t"
            "VZIP.16  q6,q12                \n\t"
            "VMOV     q9,q13                \n\t"
            "VZIP.16  q9,q13                \n\t"
            "VADD.I16 q7,q14,q5             \n\t"
            "VQMOVUN.S16 d14,q7             \n\t"
            "VSUB.I16 q10,q14,q9             \n\t"
            "VQMOVUN.S16 d15,q10             \n\t"
            "VADD.I16 q10,q14,q6             \n\t"
            "VQMOVUN.S16 d16,q10             \n\t"
            "MOV    r0, %[outptr0]            \n\t"
            "VST3.8 {d14,d15,d16}, [r0]!    \n\t"
            "VADD.I16 q7,q15,q11             \n\t"
            "VQMOVUN.S16 d14,q7             \n\t"
            "VSUB.I16 q10,q15,q13             \n\t"
            "VQMOVUN.S16 d15,q10            \n\t"
            "VADD.I16 q10,q15,q12             \n\t"
            "VQMOVUN.S16 d16,q10            \n\t"
            "VST3.8 {d14,d15,d16}, [r0]        \n\t"
            "MOV      r0, %[inptr01]        \n\t"
            "VLD1.8      {d14}, [r0]!             \n\t"
            "VLD1.8      {d15}, [r0]             \n\t"

            "VMOVL.U8 q14,d14                \n\t"
            "VMOVL.U8 q15,d15                \n\t"

            "VADD.I16 q7,q14,q5             \n\t"
            "VQMOVUN.S16 d14,q7             \n\t"

            "VSUB.I16 q10,q14,q9             \n\t"
            "VQMOVUN.S16 d15,q10             \n\t"

            "VADD.I16 q10,q14,q6             \n\t"
            "VQMOVUN.S16 d16,q10             \n\t"
            "MOV    r0, %[outptr1]            \n\t"
            "VST3.8 {d14,d15,d16}, [r0]!    \n\t"

            "VADD.I16 q7,q15,q11             \n\t"
            "VQMOVUN.S16 d14,q7             \n\t"
            "VSUB.I16 q10,q15,q13             \n\t"
            "VQMOVUN.S16 d15,q10            \n\t"
            "VADD.I16 q10,q15,q12             \n\t"
            "VQMOVUN.S16 d16,q10            \n\t"
            "VST3.8 {d14,d15,d16}, [r0]      \n\t"


         ::[outptr0] "r" (outptr0),[outptr1] "r" (outptr1), [inptr00] "r" (inptr00), [inptr01] "r" (inptr01), [inptr1] "r" (inptr1), [inptr2] "r" (inptr2), [col] "r" (col)
         : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
        "s17", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
        "d16", "d17", "d18", "d19", "d20", "d21", "d22",    "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "r0"


            );
                outptr0 += RGB_PIXELSIZE * 16;
                outptr1 += RGB_PIXELSIZE * 16;
                inptr01 = inptr01 + 16;
                inptr00 = inptr00 + 16;

    }
            inptr1 = inptr1 + col;
            inptr2 = inptr2 + col;


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

#elif defined SIMD_32BIT  /* 32 Bit */
    my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
    register int y, cred, cgreen, cblue;
    int cb, cr, tmp;
    register JSAMPROW outptr0, outptr1;
    JSAMPROW inptr00, inptr01, inptr1, inptr2;
    JDIMENSION col;
    /* copy these pointers into registers if possible */
    register JSAMPLE * range_limit = cinfo->sample_range_limit;
    int * Crrtab = upsample->Cr_r_tab;
    int * Cbbtab = upsample->Cb_b_tab;
    INT32 * Crgtab = upsample->Cr_g_tab;
    INT32 * Cbgtab = upsample->Cb_g_tab;
    JDIMENSION num_cols = cinfo->output_width >> 1;
    const int constVal1[4] = { 116130, 91881, -22554, -46802};
     const int constVal2[4] = {-14831873, -11728001,2919679, 5990656};
    SHIFT_TEMPS
    asm volatile(
        "MOV r0, %[constVal1]  \n\t"
        "VLD1.32 {q15}, [r0]  \n\t"
        "VDUP.32 q0, d30[0]   \n\t"
        "VDUP.32 q1, d30[1]   \n\t"
        "VDUP.32 q2, d31[0]  \n\t"
        "VDUP.32 q3, d31[1]  \n\t"
        "MOV r0, %[constVal2]  \n\t"
        "VLD1.32 {q4}, [r0]  \n\t"

        ::[constVal1] "r" (constVal1), [constVal2] "r" (constVal2)
        : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
        "s17", "s18", "s19", "d30", "d31", "r0"
    );

    inptr00 = input_buf[0][in_row_group_ctr*2];
    inptr01 = input_buf[0][in_row_group_ctr*2 + 1];
    inptr1 = input_buf[1][in_row_group_ctr];
    inptr2 = input_buf[2][in_row_group_ctr];
    outptr0 = output_buf[0];
    outptr1 = output_buf[1];

    for (col = 0; col < (num_cols - (num_cols & 0x7)); col= col + 8)
    {
        asm volatile (

            "ADD r0, %[inptr1], %[col]    \n\t"
            "VLD1.8      {d16}, [r0]         \n\t"
            "ADD r0, %[inptr2], %[col]    \n\t"
            "VLD1.8      {d17}, [r0]         \n\t"
            "VMOVL.U8 q9,d16            \n\t"
            "VMOVL.U8 q10,d17           \n\t"
            "VMOVL.S16 q11,d20          \n\t"
            "VMOVL.S16 q12,d21          \n\t"
            "VDUP.32 q13,d8[1]            \n\t"
            "VMLA.I32  q13,q1,q11       \n\t"
            "VDUP.32 q14,d8[1]            \n\t"
            "VMLA.I32  q14,q1,q12       \n\t"
            "VSHRN.I32 d20,q13,#16        \n\t"
            "VSHRN.I32 d21,q14,#16        \n\t"
            "VMOVL.S16 q13,d18          \n\t"
            "VMOVL.S16 q14,d19          \n\t"
            "VDUP.32   q9,d8[0]            \n\t"
            "VMLA.I32  q9,q0,q13        \n\t"
            "VDUP.32     q7,d8[0]        \n\t"
            "VMLA.I32  q7,q0,q14        \n\t"
            "VSHRN.I32 d18,q9,#16        \n\t"
            "VSHRN.I32 d19,q7,#16        \n\t"

            "VDUP.32 q15,d9[0]            \n\t"
            "VMLA.I32 q15,q2,q13        \n\t"
            "VDUP.32 q7,d9[1]            \n\t"
            "VMLA.I32 q7,q3,q11         \n\t"
            "VADD.I32 q15,q15,q7        \n\t"
            "VSHRN.I32 d30,q15,#16        \n\t"

            "VDUP.32     q13,d9[0]        \n\t"
            "VMLA.I32 q13,q2,q14        \n\t"
            "VDUP.32 q7,d9[1]            \n\t"
            "VMLA.I32 q7,q3,q12         \n\t"
            "VADD.I32 q13,q13,q7        \n\t"
            "VSHRN.I32 d31,q13,#16        \n\t"
            "MOV      r0, %[inptr00]    \n\t"
            "VLD1.8      {d16}, [r0]!         \n\t"
            "VLD1.8      {d17}, [r0]         \n\t"
            "VMOVL.U8 q11,d16           \n\t"
            "VMOVL.U8 q6,d17            \n\t"
            "VMOV.16  d26,d21           \n\t"
            "VMOV.16  d21,d20           \n\t"
            "VZIP.16  d20,d21           \n\t"
            "VADD.I16 q5,q11,q10         \n\t"
            "VQMOVUN.S16 d16,q5         \n\t"

            "VMOV.16  d27,d26          \n\t"
            "VZIP.16  d26,d27           \n\t"

            "VADD.I16 q12,q6,q13         \n\t"
            "VQMOVUN.S16 d24,q12        \n\t"

            "VMOV.16  d14,d31           \n\t"
            "VMOV.16  d31,d30           \n\t"
            "VZIP.16  d30,d31           \n\t"
            "VADD.I16 q5,q11,q15         \n\t"
            "VQMOVUN.S16 d17,q5         \n\t"

            "VMOV.16  d15,d14           \n\t"
            "VZIP.16  d14,d15           \n\t"

            "VADD.I16 q5,q6,q7             \n\t"
            "VQMOVUN.S16 d25,q5         \n\t"

            "VMOV.16  d28,d19           \n\t"
            "VMOV.16  d19,d18           \n\t"
            "VZIP.16  d18,d19           \n\t"
            "VADD.I16 q11,q11,q9         \n\t"
            "VQMOVUN.S16 d22,q11        \n\t"
            "VMOV.16  d29,d28           \n\t"
            "VZIP.16  d28,d29           \n\t"
            "VADD.I16 q6,q6,q14         \n\t"
            "VQMOVUN.S16 d12,q6         \n\t"
            "VSWP.16  d22,d18           \n\t"
            "VSWP.16  d12,d26           \n\t"
            "MOV    r0, %[outptr0]        \n\t"
            "VST3.8 {d16,d17,d18}, [r0]!\n\t"
            "VST3.8 {d24,d25,d26}, [r0] \n\t"
            "VSWP.16  d22,d18           \n\t"
            "VSWP.16  d12,d26           \n\t"
            "MOV      r0, %[inptr01]    \n\t"
            "VLD1.8      {d16}, [r0]!         \n\t"
            "VLD1.8      {d17}, [r0]         \n\t"
            "VMOVL.U8 q11,d16           \n\t"
            "VMOVL.U8 q6,d17            \n\t"

            "VADD.I16 q5,q11,q10         \n\t"
            "VQMOVUN.S16 d16,q5         \n\t"

            "VADD.I16 q12,q6,q13         \n\t"
            "VQMOVUN.S16 d24,q12        \n\t"

            "VADD.I16 q5,q11,q15         \n\t"
            "VQMOVUN.S16 d17,q5         \n\t"

            "VADD.I16 q5,q6,q7             \n\t"
            "VQMOVUN.S16 d25,q5         \n\t"

            "VADD.I16 q11,q11,q9         \n\t"
            "VQMOVUN.S16 d18,q11        \n\t"

            "VADD.I16 q6,q6,q14         \n\t"
            "VQMOVUN.S16 d26,q6          \n\t"
            "MOV    r0, %[outptr1]         \n\t"
            "VST3.8 {d16,d17,d18}, [r0]! \n\t"
            "VST3.8 {d24,d25,d26}, [r0]  \n\t"

            ::[outptr0] "r" (outptr0),[outptr1] "r" (outptr1), [inptr00] "r" (inptr00), [inptr01] "r" (inptr01), [inptr1] "r" (inptr1), [inptr2] "r" (inptr2), [col] "r" (col)
            : "cc", "memory", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16",
            "s17", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31",
            "d16", "d17", "d18", "d19", "d20", "d21", "d22",    "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "r0"
            );
            outptr0 += RGB_PIXELSIZE * 16;
            outptr1 += RGB_PIXELSIZE * 16;
            inptr01 = inptr01 + 16;
            inptr00 = inptr00 + 16;

    }
    inptr1 = inptr1 + col;
    inptr2 = inptr2 + col;
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
#else

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
  for (col = cinfo->output_width >> 1; col > 0; col--) {
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

#endif

}


#ifdef ANDROID_RGB

METHODDEF(void)
h2v2_merged_upsample_565 (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
#if defined SIMD_16BIT // sub function call
    h2v2_merged_upsample_565_sub_16bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#elif defined SIMD_32BIT
    h2v2_merged_upsample_565_sub_32bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#else
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
  for (col = cinfo->output_width >> 1; col > 0; col--) {
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
#endif
}



METHODDEF(void)
h2v2_merged_upsample_565D (j_decompress_ptr cinfo,
              JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
              JSAMPARRAY output_buf)
{
#if defined SIMD_16BIT // sub function call
    h2v2_merged_upsample_565D_sub_16bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#elif defined SIMD_32BIT
    h2v2_merged_upsample_565D_sub_32bit( cinfo, input_buf, in_row_group_ctr, output_buf );
#else
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
  JDIMENSION col_index = 0;
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
  for (col = cinfo->output_width >> 1; col > 0; col--) {
    
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
#endif
}

#endif

/*
 * Module initialization routine for merged upsampling/color conversion.
 *
 * NB: this is called under the conditions determined by use_merged_upsample()
 * in jdmaster.c.  That routine MUST correspond to the actual capabilities
 * of this module; no safety checks are made here.
 */

GLOBAL(void)
jinit_merged_upsampler (j_decompress_ptr cinfo)
{
  my_upsample_ptr upsample;

  upsample = (my_upsample_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(my_upsampler));
  cinfo->upsample = (struct jpeg_upsampler *) upsample;
  upsample->pub.start_pass = start_pass_merged_upsample;
  upsample->pub.need_context_rows = FALSE;

  upsample->out_row_width = cinfo->output_width * cinfo->out_color_components;
  
  if (cinfo->max_v_samp_factor == 2) {
    upsample->pub.upsample = merged_2v_upsample;
    upsample->upmethod = h2v2_merged_upsample;
#ifdef ANDROID_RGB
    if (cinfo->out_color_space == JCS_RGB_565) {
        if (cinfo->dither_mode == JDITHER_NONE) {
            upsample->upmethod = h2v2_merged_upsample_565;
        } else {
            upsample->upmethod = h2v2_merged_upsample_565D;
        }
    }
#endif
    /* Allocate a spare row buffer */
    upsample->spare_row = (JSAMPROW)
      (*cinfo->mem->alloc_large) ((j_common_ptr) cinfo, JPOOL_IMAGE,
		(size_t) (upsample->out_row_width * SIZEOF(JSAMPLE)));
  } else {
    upsample->pub.upsample = merged_1v_upsample;
    upsample->upmethod = h2v1_merged_upsample;
#ifdef ANDROID_RGB
    if (cinfo->out_color_space == JCS_RGB_565) {
        if (cinfo->dither_mode == JDITHER_NONE) {
            upsample->upmethod = h2v1_merged_upsample_565;
        } else {
            upsample->upmethod = h2v1_merged_upsample_565D;
        }
    }
#endif
    /* No spare row needed */
    upsample->spare_row = NULL;
  }

  build_ycc_rgb_table(cinfo);
}

#endif /* UPSAMPLE_MERGING_SUPPORTED */
