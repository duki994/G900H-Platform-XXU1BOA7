
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkBlurMask.h"
#include "SkMath.h"
#include "SkTemplates.h"
#include "SkEndian.h"


SkScalar SkBlurMask::ConvertRadiusToSigma(SkScalar radius) {
    // This constant approximates the scaling done in the software path's
    // "high quality" mode, in SkBlurMask::Blur() (1 / sqrt(3)).
    // IMHO, it actually should be 1:  we blur "less" than we should do
    // according to the CSS and canvas specs, simply because Safari does the same.
    // Firefox used to do the same too, until 4.0 where they fixed it.  So at some
    // point we should probably get rid of these scaling constants and rebaseline
    // all the blur tests.
    static const SkScalar kBLUR_SIGMA_SCALE = 0.57735f;

    return radius ? kBLUR_SIGMA_SCALE * radius + 0.5f : 0.0f;
}

#define UNROLL_SEPARABLE_LOOPS

/**
 * This function performs a box blur in X, of the given radius.  If the
 * "transpose" parameter is true, it will transpose the pixels on write,
 * such that X and Y are swapped. Reads are always performed from contiguous
 * memory in X, for speed. The destination buffer (dst) must be at least
 * (width + leftRadius + rightRadius) * height bytes in size.
 *
 * This is what the inner loop looks like before unrolling, and with the two
 * cases broken out separately (width < diameter, width >= diameter):
 *
 *      if (width < diameter) {
 *          for (int x = 0; x < width; ++x) {
 *              sum += *right++;
 *              *dptr = (sum * scale + half) >> 24;
 *              dptr += dst_x_stride;
 *          }
 *          for (int x = width; x < diameter; ++x) {
 *              *dptr = (sum * scale + half) >> 24;
 *              dptr += dst_x_stride;
 *          }
 *          for (int x = 0; x < width; ++x) {
 *              *dptr = (sum * scale + half) >> 24;
 *              sum -= *left++;
 *              dptr += dst_x_stride;
 *          }
 *      } else {
 *          for (int x = 0; x < diameter; ++x) {
 *              sum += *right++;
 *              *dptr = (sum * scale + half) >> 24;
 *              dptr += dst_x_stride;
 *          }
 *          for (int x = diameter; x < width; ++x) {
 *              sum += *right++;
 *              *dptr = (sum * scale + half) >> 24;
 *              sum -= *left++;
 *              dptr += dst_x_stride;
 *          }
 *          for (int x = 0; x < diameter; ++x) {
 *              *dptr = (sum * scale + half) >> 24;
 *              sum -= *left++;
 *              dptr += dst_x_stride;
 *          }
 *      }
 */
static int boxBlur(const uint8_t* src, int src_y_stride, uint8_t* dst,
                   int leftRadius, int rightRadius, int width, int height,
                   bool transpose)
{
    int diameter = leftRadius + rightRadius;
    int kernelSize = diameter + 1;
    int border = SkMin32(width, diameter);
    uint32_t scale = (1 << 24) / kernelSize;
    int new_width = width + SkMax32(leftRadius, rightRadius) * 2;
    int dst_x_stride = transpose ? height : 1;
    int dst_y_stride = transpose ? 1 : new_width;
    uint32_t half = 1 << 23;
    for (int y = 0; y < height; ++y) {
        uint32_t sum = 0;
        uint8_t* dptr = dst + y * dst_y_stride;
        const uint8_t* right = src + y * src_y_stride;
        const uint8_t* left = right;
        for (int x = 0; x < rightRadius - leftRadius; x++) {
            *dptr = 0;
            dptr += dst_x_stride;
        }
#define LEFT_BORDER_ITER \
            sum += *right++; \
            *dptr = (sum * scale + half) >> 24; \
            dptr += dst_x_stride;

        int x = 0;
#ifdef UNROLL_SEPARABLE_LOOPS
        for (; x < border - 16; x += 16) {
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
        }
#endif
        for (; x < border; ++x) {
            LEFT_BORDER_ITER
        }
#undef LEFT_BORDER_ITER
#define TRIVIAL_ITER \
            *dptr = (sum * scale + half) >> 24; \
            dptr += dst_x_stride;
        x = width;
#ifdef UNROLL_SEPARABLE_LOOPS
        for (; x < diameter - 16; x += 16) {
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
            TRIVIAL_ITER
        }
#endif
        for (; x < diameter; ++x) {
            TRIVIAL_ITER
        }
#undef TRIVIAL_ITER
#define CENTER_ITER \
            sum += *right++; \
            *dptr = (sum * scale + half) >> 24; \
            sum -= *left++; \
            dptr += dst_x_stride;

        x = diameter;
#ifdef UNROLL_SEPARABLE_LOOPS
        for (; x < width - 16; x += 16) {
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
        }
#endif
        for (; x < width; ++x) {
            CENTER_ITER
        }
#undef CENTER_ITER
#define RIGHT_BORDER_ITER \
            *dptr = (sum * scale + half) >> 24; \
            sum -= *left++; \
            dptr += dst_x_stride;

        x = 0;
#ifdef UNROLL_SEPARABLE_LOOPS
        for (; x < border - 16; x += 16) {
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
        }
#endif
        for (; x < border; ++x) {
            RIGHT_BORDER_ITER
        }
#undef RIGHT_BORDER_ITER
        for (int x = 0; x < leftRadius - rightRadius; ++x) {
            *dptr = 0;
            dptr += dst_x_stride;
        }
        SkASSERT(sum == 0);
    }
    return new_width;
}

/**
 * This variant of the box blur handles blurring of non-integer radii.  It
 * keeps two running sums: an outer sum for the rounded-up kernel radius, and
 * an inner sum for the rounded-down kernel radius.  For each pixel, it linearly
 * interpolates between them.  In float this would be:
 *  outer_weight * outer_sum / kernelSize +
 *  (1.0 - outer_weight) * innerSum / (kernelSize - 2)
 *
 * This is what the inner loop looks like before unrolling, and with the two
 * cases broken out separately (width < diameter, width >= diameter):
 *
 *      if (width < diameter) {
 *          for (int x = 0; x < width; x++) {
 *              inner_sum = outer_sum;
 *              outer_sum += *right++;
 *              *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24;
 *              dptr += dst_x_stride;
 *          }
 *          for (int x = width; x < diameter; ++x) {
 *              *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24;
 *              dptr += dst_x_stride;
 *          }
 *          for (int x = 0; x < width; x++) {
 *              inner_sum = outer_sum - *left++;
 *              *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24;
 *              dptr += dst_x_stride;
 *              outer_sum = inner_sum;
 *          }
 *      } else {
 *          for (int x = 0; x < diameter; x++) {
 *              inner_sum = outer_sum;
 *              outer_sum += *right++;
 *              *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24;
 *              dptr += dst_x_stride;
 *          }
 *          for (int x = diameter; x < width; ++x) {
 *              inner_sum = outer_sum - *left;
 *              outer_sum += *right++;
 *              *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24;
 *              dptr += dst_x_stride;
 *              outer_sum -= *left++;
 *          }
 *          for (int x = 0; x < diameter; x++) {
 *              inner_sum = outer_sum - *left++;
 *              *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24;
 *              dptr += dst_x_stride;
 *              outer_sum = inner_sum;
 *          }
 *      }
 *  }
 *  return new_width;
 */
#define BOX_BLUR_INTERP_NEON_ENABLED
#if defined(__ARM_HAVE_NEON) && defined(BOX_BLUR_INTERP_NEON_ENABLED)

typedef struct
{
    int leftNbPixels;       //0
    int leftNbPixels2;      //4
    int centerNbPixels;     //8
    int rightNbPixels;      //12
    int new_h;              //16
    int dst_y_stride;       //20
    uint32_t inner_scale;   //24
    uint32_t outer_scale;   //28
    int src_y_stride;       //32
    const uint8_t* src;     //36
    const uint8_t* dst;     //40
    uint32_t half;          //44
    int dst_x_stride;       //48
}AsmConstant;

static int boxBlurInterpNeonTransposed(AsmConstant *asm_constant)
{
    int y = 0;
    // The neon version process 4 lines at a time.
    // Note that the original code has 4 loops where each does the same calculation with minor variations
    // So the neon version replicates those 4 loops using a common template combined with macro to avoid code repetition
    // TODO: batch pixel store instruction using spare Q registers (q4,q5) with vst4.8 for transpose case. This should further improve performance. Batching stores requires different code for transpose case
    // r0, r1: temp registers used for various operations: counter, load from memory
    // r2, r3, r4, r5: registers used to point to 4 lines in dst buffer permanently within line loop.
    // r6: right pointer
    // r7: left pointer
    // r8: src_y_stride
    // r9: half
    // r10: dst_x_stride
    // d0, d1, d2, d3: right column storing 8 pixels per line (each d is a line)
    // d4: index vector used for vtbl
    // d5: current right column of 4 pixels
    // d6: initial index vector
    // d26, d27, d28, d29: left column storing 8 pixels per line (each d is a line)
    // d30: curent left column of 4 pixels
    // d31: increment of 1 used for loading each column of 4 pixels via vtbl
    // q8: outer_sum
    // q9: inner_sum
    // q10: outer_scale
    // q11: inner_scale
    // q12: dst pixel value
    asm volatile (
        // MACRO definition to avoid code duplication
        // this macro calculates the value for the result pixel
        ".macro  calculate_dst_pixel%= result, outer_sum, outer_scale, inner_sum, inner_scale, half \n\t"
            "vdup.32   \\result, \\half                         \n\t"
            "vmla.u32  \\result, \\outer_sum, \\outer_scale     \n\t"   // outer_sum * outer_scale
            "vmla.u32  \\result, \\inner_sum, \\inner_scale     \n\t"   // outer_sum * outer_scale + (inner_sum * inner_scale)
            "vshr.u32  \\result, \\result, #24                  \n\t"   // outer_sum * outer_scale + (inner_sum * inner_scale) + half >> 24*/
        ".endm                                                  \n\t"   // TODO: vshr.u32 may not be necessary if store_1_pixel reference the relevant index!

        // this macro load n pixels for 4 lines
        ".macro load_n_pixels%= dst0, dst1, dst2, dst3, src, stride, nb_pixels \n\t"
            "vld1.8     {\\dst0}, [\\src], \\stride             \n\t"   // load 8 pixels at a time per line
            "vld1.8     {\\dst1}, [\\src], \\stride             \n\t"
            "vld1.8     {\\dst2}, [\\src], \\stride             \n\t"
            "vld1.8     {\\dst3}, [\\src], \\stride             \n\t"
            "sub        \\src, \\src, \\stride, LSL #2          \n\t"   // reset to top line offset
            "add        \\src, \\src, \\nb_pixels               \n\t"   // next n pixels
        ".endm                                                  \n\t"

        //TODO: write multiple pixels per line at same time !
        ".macro store_1_pixel%=   src0, src1, src2, src3, dst0, dst1, dst2, dst3, stride \n\t"
            "vst1.8  {\\src0}, [\\dst0], \\stride               \n\t"
            "vst1.8  {\\src1}, [\\dst1], \\stride               \n\t"
            "vst1.8  {\\src2}, [\\dst2], \\stride               \n\t"
            "vst1.8  {\\src3}, [\\dst3], \\stride               \n\t"
        ".endm                                                  \n\t"

        "ldr        r0, [%[asm_constant],#28]                   \n\t"   // load outer_scale
        "vdup.32    q10, r0                                     \n\t"   // q10 <- outer_scale
        "ldr        r0, [%[asm_constant],#24]                   \n\t"   // load inner_scale
        "vdup.32    q11, r0                                     \n\t"   // q11 <- inner_scale
        "vmov.u16   d31, #1                                     \n\t"   // set increment to 1

        // build index vector: [0, 8, 16, 24] for vector table look up
        // use FF to set out of range element to 0 as per vtbl instruction
        "movw       r0, #0xFF00                                 \n\t"   // 0x0000FF00
        "movt       r0, #0xFF08                                 \n\t"   // 0xFF08FF00
        "movw       r1, #0xFF10                                 \n\t"   // 0x0000FF10
        "movt       r1, #0xFF18                                 \n\t"   // 0xFF18FF10

        "vmov       d6, r0, r1                                  \n\t"   // 0xFF18FF10 0xFF08FF00

        "ldr    r8, [%[asm_constant], #32]                      \n\t"   // load 'src_y_stride'
        "ldr    r9, [%[asm_constant], #44]                      \n\t"   // load 'half'
        "ldr    r10, [%[asm_constant], #48]                     \n\t"   // load 'dst_x_stride'

        // process 4 lines at the same time
        "LineLoop%=:"

            "ldr    r0, [%[asm_constant], #16]      \n\t"   // load 'new_h'
            "cmp    %[y], r0                        \n\t"   // while (y < new_h)
            "bge    LineLoopEnd%=                     \n\t"

            "ldr    r0, [%[asm_constant], #20]      \n\t"   // load 'dst_y_stride'
            "ldr    r2, [%[asm_constant], #40]      \n\t"   // load 'dst'
            "ldr    r6, [%[asm_constant], #36]      \n\t"   // load 'src'

            // fill the empty gap (stall) caused for loading data from memory to r0/r2
            "vmov.u32   q8, #0                      \n\t"   // outer_sum = 0
            "vmov.u32   q9, #0                      \n\t"   // inner_sum = 0

            "mla    r2, %[y], r0, r2                \n\t"   // uint8_t* dst0 = dst + y * dst_y_stride;
            "add    r3, r2, r0                      \n\t"   // uint8_t* dst1 = dst0 + dst_y_stride;
            "add    r4, r3, r0                      \n\t"   // uint8_t* dst2 = dst1 + dst_y_stride;
            "add    r5, r4, r0                      \n\t"   // uint8_t* dst3 = dst2 + dst_y_stride;

            "mla    r6, %[y], r8, r6                \n\t"   // const uint8_t* right = src + y * src_y_stride;
            "mov    r7, r6                          \n\t"   // const uint8_t* left = right

            "add    %[y], %[y], #4                  \n\t"   // y+=4

            //LEFT border loop
            "ldr        r1, [%[asm_constant]]                       \n\t"   // nb_pixels = border

            "BlockLoop%=:                                             \n\t"

                "cmp        r1, #8                                  \n\t"   // while (nb_pixels >= 8)
                "blt        PixelLoopStart%=                          \n\t"

                "vmov       d4, d6                                  \n\t"   //restore index vector for vtbl
                "load_n_pixels%= d0, d1, d2, d3, r6, r8, #8 \n\t"
                "mov        r0, #8                                  \n\t"   //loop counter set to 8 pixels

            "ColumnLoop%=:                                            \n\t"

                "vtbl.8     d5, {d0,d1,d2,d3}, d4                   \n\t"   // extract column of 4 pixels
                "vadd.u16   d4, d4, d31                             \n\t"   // increment each index in index vector so to access next column of pixels for next iteration
                "vmov       q9, q8                                  \n\t"   // inner_sum = outer_sum
                "vaddw.u16  q8, q8, d5                              \n\t"   // outer_sum += *right_colum

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "subs   r0, r0, #1                                  \n\t"
                "bgt    ColumnLoop%=                                  \n\t"

            "sub   r1, r1, #8                                       \n\t"
            "b    BlockLoop%=                                         \n\t"

            "PixelLoopStart%=:                                        \n\t"

                "cmp    r1, #0                                      \n\t"   // while (nb_pixels > 0)
                "beq    End%=                                         \n\t"

            "PixelLoop%=:"

                //load 1 pixel at a time per line and build column of 4 pixels
                "vmov.u16   d0, #0                                  \n\t"

                "load_n_pixels%= d0[0], d0[2], d0[4], d0[6], r6, r8, #1 \n\t"

                "vmov       q9, q8                                  \n\t"   // inner_sum = outer_sum
                "vaddw.u16  q8, q8, d0                              \n\t"   // outer_sum += *right_column

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "subs    r1, r1, #1                                 \n\t"   // pixel counter decrement
                "bgt    PixelLoop%=                                   \n\t"

            "End%=:                                                   \n\t"

            //LEFT border loop 2nd part (2nd for loop in C version)
            "ldr    r1, [%[asm_constant], #4]          \n\t"   // nb_pixels = diameter - width;

            "cmp    r1, #0                      \n\t"
            "beq    End1%=                        \n\t"

            "PixelLoop1%=:"

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "subs    r1, r1, #1                 \n\t"
                "bgt    PixelLoop1%=                  \n\t"

            "End1%=:                                  \n\t"

            //CENTER border loop
            "ldr  r1, [%[asm_constant],#8]             \n\t"    // nb_pixels = width - diameter;

            "BlockLoop2%=:                            \n\t"

                "cmp    r1, #8                          \n\t"
                "blt    PixelLoopStart2%=                 \n\t"

                "vmov       d4, d6                                 \n\t"   //restore index vector for vtbl
                "load_n_pixels%= d26, d27, d28, d29, r7, r8, #8 \n\t"   // load 8 pixels at a time per line for left pointer
                "load_n_pixels%= d0, d1, d2, d3, r6, r8, #8 \n\t"      // load 8 pixels at a time per line for right pointer
                "mov        r0, #8                  \n\t"   //loop counter set to 8 pixels

            "ColumnLoop2%=:                           \n\t"

                "vtbl.8     d30, {d26,d27,d28,d29}, d4   \n\t"   // extract column of 4 pixels from left++ pointer
                "vtbl.8     d5, {d0,d1,d2,d3}, d4   \n\t"   // extract column of 4 pixels from right pointer
                "vadd.u16   d4, d4, d31             \n\t"   // increment each index in index vector so to access next column of pixels for next iteration
                "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left;
                "vaddw.u16  q8, q8, d5              \n\t"   // outer_sum += *right_colum

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "vsubw.u16  q8, q8, d30             \n\t"   // outer_sum -= *left++;

                "subs   r0, r0, #1                  \n\t"   // column counter decrement
                "bgt    ColumnLoop2%=                 \n\t"

            "sub   r1, r1, #8                       \n\t"
            "b    BlockLoop2%=                             \n\t"

            "PixelLoopStart2%=:                       \n\t"

                "cmp    r1, #0                      \n\t"
                "beq    End2%=                        \n\t"

            "PixelLoop2%=:"

                //load column of 4 pixels at a time for left pointer
                "vmov.u16   d30, #0                 \n\t"
                "load_n_pixels%= d30[0], d30[2], d30[4], d30[6], r7, r8, #1 \n\t"

                //load column of 4 pixels at a time for right pointer
                "vmov.u16   d0, #0                  \n\t"
                "load_n_pixels%= d0[0], d0[2], d0[4], d0[6], r6, r8, #1 \n\t"

                "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left;
                "vaddw.u16  q8, q8, d0              \n\t"   // outer_sum += *right_colum

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "vsubw.u16  q8, q8, d30             \n\t"   // outer_sum -= *left++;

                "subs    r1, r1, #1                 \n\t"
                "bgt    PixelLoop2%=                  \n\t"

            "End2%=:                                  \n\t"

            //RIGHT border loop
            "ldr  r1, [%[asm_constant],#12]            \n\t"   // nb_pixels = border

            "BlockLoop3%=:                            \n\t"

                "cmp    r1, #8                          \n\t"
                "blt    PixelLoopStart3%=                 \n\t"

                "vmov       d4, d6                  \n\t"   //restore index vector for vtbl
                "load_n_pixels%= d26, d27, d28, d29, r7, r8, #8 \n\t"   // load 8 pixels at a time per line for left pointer
                "mov        r0, #8                  \n\t"   //loop counter set to 8 pixels

            "ColumnLoop3%=:                           \n\t"

                "vtbl.8     d30, {d26,d27,d28,d29}, d4   \n\t"   // extract column of 4 pixels from left++ pointer
                "vadd.u16   d4, d4, d31             \n\t"   // increment each index in index vector so to access next column of pixels for next iteration
                "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left++;

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "vmov   q8, q9                      \n\t"   // outer_sum = inner_sum;

                "subs   r0, r0, #1                  \n\t"
                "bgt    ColumnLoop3%=                 \n\t"

            "sub   r1, r1, #8                       \n\t"
            "b    BlockLoop3%=                             \n\t"

            "PixelLoopStart3%=:                       \n\t"

                "cmp    r1, #0                      \n\t"
                "beq    End3%=                        \n\t"

            "PixelLoop3%=:"

                //load column of 4 pixels at a time for left pointer
                "vmov.u16   d30, #0                 \n\t"
                "load_n_pixels%= d30[0], d30[2], d30[4], d30[6], r7, r8, #1 \n\t"

                "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left++;

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "vmov   q8, q9                      \n\t"   // outer_sum = inner_sum;

                "subs    r1, r1, #1                 \n\t"
                "bgt    PixelLoop3%=                  \n\t"

            "End3%=:                                  \n\t"

            "b  LineLoop%=                            \n\t"

            "LineLoopEnd%=:                           \n\t"

            : [y] "+r" (y)
            : [asm_constant] "r" (asm_constant)
            : "cc", "memory",
              "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
              "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d8", "d9", "d10", "d11", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15");
    return y;
}


//non transpose case
static int boxBlurInterpNeon(AsmConstant *asm_constant)
{
    int y = 0;
    // The neon version process 4 lines at a time.
    // Note that the original code has 4 loops where each does the same calculation with minor variations
    // So the neon version replicates those 4 loops using a common template combined with macro to avoid code repetition
    // r0, r1: temp registers used for various operations: counter, load from memory
    // r2, r3, r4, r5: registers used to point to 4 lines in dst buffer permanently within line loop.
    // r6: right pointer
    // r7: left pointer
    // r8: src_y_stride
    // r9: half
    // r10: dst_x_stride
    // d0, d1, d2, d3: right column storing 8 pixels per line (each d is a line)
    // d4: index vector used for vtbl
    // d5: current right column of 4 pixels
    // d6: initial index vector
    // d26, d27, d28, d29: left column storing 8 pixels per line (each d is a line)
    // d30: curent left column of 4 pixels
    // d31: increment of 1 used for loading each column of 4 pixels via vtbl
    // q4, q5: batch of 4 * 8 pixels used by vst1.8
    // q8: outer_sum
    // q9: inner_sum
    // q10: outer_scale
    // q11: inner_scale
    // q12: dst pixel value
    asm volatile (
        // MACRO definition to avoid code duplication
        // this macro calculates the value for the result pixel
        ".macro  calculate_dst_pixel%= result, outer_sum, outer_scale, inner_sum, inner_scale, half \n\t"
            "vdup.32   \\result, \\half                         \n\t"
            "vmla.u32  \\result, \\outer_sum, \\outer_scale     \n\t"   // outer_sum * outer_scale
            "vmla.u32  \\result, \\inner_sum, \\inner_scale     \n\t"   // outer_sum * outer_scale + (inner_sum * inner_scale)
            "vshr.u32  \\result, \\result, #24                  \n\t"   // outer_sum * outer_scale + (inner_sum * inner_scale) + half >> 24*/
        ".endm                                                  \n\t"   // TODO: vshr.u32 may not be necessary!

        // this macro load n pixels for 4 lines
        ".macro load_n_pixels%= dst0, dst1, dst2, dst3, src, stride, nb_pixels \n\t"
            "vld1.8     {\\dst0}, [\\src], \\stride             \n\t"   // load 8 pixels at a time per line
            "vld1.8     {\\dst1}, [\\src], \\stride             \n\t"
            "vld1.8     {\\dst2}, [\\src], \\stride             \n\t"
            "vld1.8     {\\dst3}, [\\src], \\stride             \n\t"
            "sub        \\src, \\src, \\stride, LSL #2          \n\t"   // reset to top line offset
            "add        \\src, \\src, \\nb_pixels               \n\t"   // next n pixels
        ".endm                                                  \n\t"

        // used for transposed case
        ".macro store_1_pixel%=   src0, src1, src2, src3, dst0, dst1, dst2, dst3, stride \n\t"
            "vst1.8  {\\src0}, [\\dst0], \\stride               \n\t"
            "vst1.8  {\\src1}, [\\dst1], \\stride               \n\t"
            "vst1.8  {\\src2}, [\\dst2], \\stride               \n\t"
            "vst1.8  {\\src3}, [\\dst3], \\stride               \n\t"
        ".endm                                                  \n\t"

        // macro used to store 8 pixels per line at the same time for non transposed case
        ".macro store_n_pixels%=   src0, src1, src2, src3, dst0, dst1, dst2, dst3 \n\t"
            "vst1.8  {\\src0}, [\\dst0]!               \n\t"
            "vst1.8  {\\src1}, [\\dst1]!               \n\t"
            "vst1.8  {\\src2}, [\\dst2]!               \n\t"
            "vst1.8  {\\src3}, [\\dst3]!               \n\t"
        ".endm                                                  \n\t"

        ".macro calculate_left_pixel%=                       \n\t"
            "vtbl.8     d5, {d0,d1,d2,d3}, d4                   \n\t"   // extract column of 4 pixels
            "vadd.u16   d4, d4, d31                             \n\t"   // increment each index in index vector so to access next column of pixels for next iteration
            "vmov       q9, q8                                  \n\t"   // inner_sum = outer_sum
            "vaddw.u16  q8, q8, d5                              \n\t"   // outer_sum += *right_colum
            "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9     \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
        ".endm                                          \n\t"

        ".macro calculate_center_pixel%=                       \n\t"
            "vtbl.8     d30, {d26,d27,d28,d29}, d4   \n\t"   // extract column of 4 pixels from left++ pointer
            "vtbl.8     d5, {d0,d1,d2,d3}, d4   \n\t"   // extract column of 4 pixels from right pointer
            "vadd.u16   d4, d4, d31             \n\t"   // increment each index in index vector so to access next column of pixels for next iteration
            "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left;
            "vaddw.u16  q8, q8, d5              \n\t"   // outer_sum += *right_colum
            "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
            "vsubw.u16  q8, q8, d30             \n\t"   // outer_sum -= *left++;
        ".endm                                          \n\t"

        ".macro calculate_right_pixel%=                       \n\t"
            "vtbl.8     d30, {d26,d27,d28,d29}, d4   \n\t"   // extract column of 4 pixels from left++ pointer
            "vadd.u16   d4, d4, d31             \n\t"   // increment each index in index vector so to access next column of pixels for next iteration
            "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left++;
            "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
            "vmov   q8, q9                      \n\t"   // outer_sum = inner_sum;
        ".endm                                          \n\t"

        "ldr        r0, [%[asm_constant],#28]                   \n\t"   // load outer_scale
        "vdup.32    q10, r0                                     \n\t"   // q10 <- outer_scale
        "ldr        r0, [%[asm_constant],#24]                   \n\t"   // load inner_scale
        "vdup.32    q11, r0                                     \n\t"   // q11 <- inner_scale
        "vmov.u16   d31, #1                                     \n\t"   // set increment to 1

        // build index vector: [0, 8, 16, 24] for vector table look up
        // use FF to set out of range element to 0 as per vtbl instruction
        "movw       r0, #0xFF00                                 \n\t"   // 0x0000FF00
        "movt       r0, #0xFF08                                 \n\t"   // 0xFF08FF00
        "movw       r1, #0xFF10                                 \n\t"   // 0x0000FF10
        "movt       r1, #0xFF18                                 \n\t"   // 0xFF18FF10

        "vmov       d6, r0, r1                                  \n\t"   // 0xFF18FF10 0xFF08FF00

        "ldr    r8, [%[asm_constant], #32]                      \n\t"   // load 'src_y_stride'
        "ldr    r9, [%[asm_constant], #44]                      \n\t"   // load 'half'
        "ldr    r10, [%[asm_constant], #48]                     \n\t"   // load 'dst_x_stride'

        // process 4 lines at the same time
        "LineLoop%=:"

            "ldr    r0, [%[asm_constant], #16]      \n\t"   // load 'new_h'
            "cmp    %[y], r0                        \n\t"   // while (y < new_h)
            "bge    LineLoopEnd%=                     \n\t"

            "ldr    r0, [%[asm_constant], #20]      \n\t"   // load 'dst_y_stride'
            "ldr    r2, [%[asm_constant], #40]      \n\t"   // load 'dst'
            "ldr    r6, [%[asm_constant], #36]      \n\t"   // load 'src'

            // fill the empty gap (stall) caused for loading data from memory to r0/r2
            "vmov.u32   q8, #0                      \n\t"   // outer_sum = 0
            "vmov.u32   q9, #0                      \n\t"   // inner_sum = 0

            "mla    r2, %[y], r0, r2                \n\t"   // uint8_t* dst0 = dst + y * dst_y_stride;
            "add    r3, r2, r0                      \n\t"   // uint8_t* dst1 = dst0 + dst_y_stride;
            "add    r4, r3, r0                      \n\t"   // uint8_t* dst2 = dst1 + dst_y_stride;
            "add    r5, r4, r0                      \n\t"   // uint8_t* dst3 = dst2 + dst_y_stride;

            "mla    r6, %[y], r8, r6                \n\t"   // const uint8_t* right = src + y * src_y_stride;
            "mov    r7, r6                          \n\t"   // const uint8_t* left = right

            "add    %[y], %[y], #4                  \n\t"   // y+=4

            //LEFT border loop
            "ldr        r1, [%[asm_constant]]                       \n\t"   // nb_pixels = border

            "BlockLoop%=:                                             \n\t"

                "cmp        r1, #8                                  \n\t"   // while (nb_pixels >= 8)
                "blt        PixelLoopStart%=                          \n\t"

                "vmov       d4, d6                                  \n\t"   //restore index vector for vtbl
                "load_n_pixels%= d0, d1, d2, d3, r6, r8, #8 \n\t"

                //batch stores
                "vmov.u32    q4, #0                                 \n\t"
                "vmov.u32    q5, #0                                 \n\t"
                //1
                "calculate_left_pixel%=                             \n\t"
                "vorr       q4, q4, q12                             \n\t"
                //2
                "calculate_left_pixel%=                             \n\t"
                "vshl.u32   q12, q12, #8                              \n\t"   // << 1 byte
                "vorr       q4, q4, q12                             \n\t"
                //3
                "calculate_left_pixel%=                             \n\t"
                "vshl.u32   q12, q12, #16                              \n\t"   // << 2 bytes
                "vorr       q4, q4, q12                             \n\t"
                //4
                "calculate_left_pixel%=                             \n\t"
                "vshl.u32   q12, q12, #24                              \n\t"   // << 3 bytes
                "vorr       q4, q4, q12                             \n\t"
                //5
                "calculate_left_pixel%=                             \n\t"
                "vorr       q5, q5, q12                             \n\t"
                //6
                "calculate_left_pixel%=                             \n\t"
                "vshl.u32   q12, q12, #8                              \n\t"   // << 1 byte
                "vorr       q5, q5, q12                             \n\t"
                //7
                "calculate_left_pixel%=                             \n\t"
                "vshl.u32   q12, q12, #16                              \n\t"   // << 2 bytes
                "vorr       q5, q5, q12                             \n\t"
                //8
                "calculate_left_pixel%=                             \n\t"
                "vshl.u32   q12, q12, #24                              \n\t"   // << 3 bytes
                "vorr       q5, q5, q12                             \n\t"

                "vzip.32    q4, q5                                  \n\t"
                "store_n_pixels%= d8, d9, d10, d11, r2, r3, r4, r5    \n\t"

            "sub   r1, r1, #8                                       \n\t"
            "b    BlockLoop%=                                         \n\t"

            "PixelLoopStart%=:                                        \n\t"

                "cmp    r1, #0                                      \n\t"   // while (nb_pixels > 0)
                "beq    End%=                                         \n\t"

            "PixelLoop%=:"

                //load 1 pixel at a time per line and build column of 4 pixels
                "vmov.u16   d0, #0                                  \n\t"

                "load_n_pixels%= d0[0], d0[2], d0[4], d0[6], r6, r8, #1 \n\t"

                "vmov       q9, q8                                  \n\t"   // inner_sum = outer_sum
                "vaddw.u16  q8, q8, d0                              \n\t"   // outer_sum += *right_column

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "subs    r1, r1, #1                                 \n\t"   // pixel counter decrement
                "bgt    PixelLoop%=                                   \n\t"

            "End%=:                                                   \n\t"

            //LEFT border loop 2nd part (2nd for loop in C version)
            "ldr    r1, [%[asm_constant], #4]          \n\t"   // nb_pixels = diameter - width;

            "cmp    r1, #0                      \n\t"
            "beq    End1%=                        \n\t"

            "PixelLoop1%=:"

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "subs    r1, r1, #1                 \n\t"
                "bgt    PixelLoop1%=                  \n\t"

            "End1%=:                                  \n\t"

            //CENTER border loop
            "ldr  r1, [%[asm_constant],#8]             \n\t"    // nb_pixels = width - diameter;

            "BlockLoop2%=:                            \n\t"

                "cmp    r1, #8                          \n\t"
                "blt    PixelLoopStart2%=                 \n\t"

                "vmov       d4, d6                                 \n\t"   //restore index vector for vtbl
                "load_n_pixels%= d26, d27, d28, d29, r7, r8, #8 \n\t"   // load 8 pixels at a time per line for left pointer
                "load_n_pixels%= d0, d1, d2, d3, r6, r8, #8 \n\t"      // load 8 pixels at a time per line for right pointer

                //batch stores
                "vmov.u32    q4, #0                                 \n\t"
                "vmov.u32    q5, #0                                 \n\t"
                //1
                "calculate_center_pixel%=               \n\t"
                "vorr       q4, q4, q12                             \n\t"   //
                //2
                "calculate_center_pixel%=               \n\t"
                "vshl.u32   q12, q12, #8                              \n\t"   // << 1 byte
                "vorr       q4, q4, q12                             \n\t"   //
                //3
                "calculate_center_pixel%=               \n\t"
                "vshl.u32   q12, q12, #16                              \n\t"   // << 2 byte
                "vorr       q4, q4, q12                             \n\t"   //
                //4
                "calculate_center_pixel%=               \n\t"
                "vshl.u32   q12, q12, #24                              \n\t"   // << 3 byte
                "vorr       q4, q4, q12                             \n\t"   //
                //5
                "calculate_center_pixel%=               \n\t"
                "vorr       q5, q5, q12                             \n\t"   //
                //6
                "calculate_center_pixel%=               \n\t"
                "vshl.u32   q12, q12, #8                              \n\t"   // << 1 byte
                "vorr       q5, q5, q12                             \n\t"   //
                //7
                "calculate_center_pixel%=               \n\t"
                "vshl.u32   q12, q12, #16                              \n\t"   // << 2 byte
                "vorr       q5, q5, q12                             \n\t"   //
                //8
                "calculate_center_pixel%=               \n\t"
                "vshl.u32   q12, q12, #24                              \n\t"   // << 3 byte
                "vorr       q5, q5, q12                             \n\t"   //

                "vzip.32    q4, q5                                  \n\t"
                "store_n_pixels%= d8, d9, d10, d11, r2, r3, r4, r5    \n\t"

            "sub   r1, r1, #8                       \n\t"
            "b    BlockLoop2%=                             \n\t"

            "PixelLoopStart2%=:                       \n\t"

                "cmp    r1, #0                      \n\t"
                "beq    End2%=                        \n\t"

            "PixelLoop2%=:"

                //load column of 4 pixels at a time for left pointer
                "vmov.u16   d30, #0                 \n\t"
                "load_n_pixels%= d30[0], d30[2], d30[4], d30[6], r7, r8, #1 \n\t"

                //load column of 4 pixels at a time for right pointer
                "vmov.u16   d0, #0                  \n\t"
                "load_n_pixels%= d0[0], d0[2], d0[4], d0[6], r6, r8, #1 \n\t"

                "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left;
                "vaddw.u16  q8, q8, d0              \n\t"   // outer_sum += *right_colum

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "vsubw.u16  q8, q8, d30             \n\t"   // outer_sum -= *left++;

                "subs    r1, r1, #1                 \n\t"
                "bgt    PixelLoop2%=                  \n\t"

            "End2%=:                                  \n\t"

            //RIGHT border loop
            "ldr  r1, [%[asm_constant],#12]            \n\t"   // nb_pixels = border

            "BlockLoop3%=:                            \n\t"

                "cmp    r1, #8                          \n\t"
                "blt    PixelLoopStart3%=                 \n\t"

                "vmov       d4, d6                  \n\t"   //restore index vector for vtbl
                "load_n_pixels%= d26, d27, d28, d29, r7, r8, #8 \n\t"   // load 8 pixels at a time per line for left pointer

                //batch stores
                "vmov.u32    q4, #0                                 \n\t"
                "vmov.u32    q5, #0                                 \n\t"

                //1
                "calculate_right_pixel%=               \n\t"
                "vorr       q4, q4, q12                             \n\t"   //
                //2
                "calculate_right_pixel%=               \n\t"
                "vshl.u32   q12, q12, #8                              \n\t"   // << 1 byte
                "vorr       q4, q4, q12                             \n\t"   //
                //3
                "calculate_right_pixel%=               \n\t"
                "vshl.u32   q12, q12, #16                              \n\t"   // << 2 byte
                "vorr       q4, q4, q12                             \n\t"   //
                //4
                "calculate_right_pixel%=               \n\t"
                "vshl.u32   q12, q12, #24                              \n\t"   // << 3 byte
                "vorr       q4, q4, q12                             \n\t"   //
                //5
                "calculate_right_pixel%=               \n\t"
                "vorr       q5, q5, q12                             \n\t"   //
                //6
                "calculate_right_pixel%=               \n\t"
                "vshl.u32   q12, q12, #8                              \n\t"   // << 1 byte
                "vorr       q5, q5, q12                             \n\t"   //
                //7
                "calculate_right_pixel%=               \n\t"
                "vshl.u32   q12, q12, #16                              \n\t"   // << 2 byte
                "vorr       q5, q5, q12                             \n\t"   //
                //8
                "calculate_right_pixel%=               \n\t"
                "vshl.u32   q12, q12, #24                              \n\t"   // << 3 byte
                "vorr       q5, q5, q12                             \n\t"   //

                "vzip.32    q4, q5                                  \n\t"
                "store_n_pixels%= d8, d9, d10, d11, r2, r3, r4, r5    \n\t"

            "sub   r1, r1, #8                       \n\t"
            "b    BlockLoop3%=                             \n\t"

            "PixelLoopStart3%=:                       \n\t"

                "cmp    r1, #0                      \n\t"
                "beq    End3%=                        \n\t"

            "PixelLoop3%=:"

                //load column of 4 pixels at a time for left pointer
                "vmov.u16   d30, #0                 \n\t"
                "load_n_pixels%= d30[0], d30[2], d30[4], d30[6], r7, r8, #1 \n\t"

                "vsubw.u16  q9, q8, d30             \n\t"   // inner_sum = outer_sum - *left++;

                "calculate_dst_pixel%= q12, q8, q10, q9, q11, r9                \n\t"   // (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24
                "store_1_pixel%=   d24[0], d24[4], d25[0], d25[4], r2, r3, r4, r5, r10 \n\t"   //store column of 4 pixels

                "vmov   q8, q9                      \n\t"   // outer_sum = inner_sum;

                "subs    r1, r1, #1                 \n\t"
                "bgt    PixelLoop3%=                  \n\t"

            "End3%=:                                  \n\t"

            "b  LineLoop%=                            \n\t"

            "LineLoopEnd%=:                           \n\t"

            : [y] "+r" (y)
            : [asm_constant] "r" (asm_constant)
            : "cc", "memory",
              "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
              "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d8", "d9", "d10", "d11", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31", "q4", "q5", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15");
    return y;
}
#endif //BOX_BLUR_INTERP_NEON_ENABLED

static int boxBlurInterp(const uint8_t* src, int src_y_stride, uint8_t* dst,
                         int radius, int width, int height,
                         bool transpose, uint8_t outer_weight)
{
    int diameter = radius * 2;
    int kernelSize = diameter + 1;
    int border = SkMin32(width, diameter);
    int inner_weight = 255 - outer_weight;
    outer_weight += outer_weight >> 7;
    inner_weight += inner_weight >> 7;
    uint32_t outer_scale = (outer_weight << 16) / kernelSize;
    uint32_t inner_scale = (inner_weight << 16) / (kernelSize - 2);
    uint32_t half = 1 << 23;
    int new_width = width + diameter;
    int dst_x_stride = transpose ? height : 1;
    int dst_y_stride = transpose ? 1 : new_width;

#if defined(__ARM_HAVE_NEON) && defined(BOX_BLUR_INTERP_NEON_ENABLED)
    int new_h =  height & ~0x00000003;//get multiple of 4
    int diff = diameter - width;
    AsmConstant asm_constant = {border,                 //0
                                diff > 0 ? diff : 0,    //4
                                diff < 0 ? -diff : 0,   //8
                                border,                 //12
                                new_h,                  //16
                                dst_y_stride,           //20
                                inner_scale,            //24
                                outer_scale,            //28
                                src_y_stride,           //32
                                src,                    //36
                                dst,                    //40
                                half,                   //44
                                dst_x_stride};          //48

    int y;
    if (transpose) {
        y = boxBlurInterpNeonTransposed(&asm_constant);
    }
    else {
        y = boxBlurInterpNeon(&asm_constant);
    }
    //Do the remaining lines using existing code
    for (; y < height; ++y) {
#else
    for (int y = 0; y < height; ++y) {
#endif  // endif BOX_BLUR_INTERP_NEON_ENABLED
        uint32_t outer_sum = 0, inner_sum = 0;
        uint8_t* dptr = dst + y * dst_y_stride;
        const uint8_t* right = src + y * src_y_stride;
        const uint8_t* left = right;
        int x = 0;

#define LEFT_BORDER_ITER \
            inner_sum = outer_sum; \
            outer_sum += *right++; \
            *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24; \
            dptr += dst_x_stride;

#ifdef UNROLL_SEPARABLE_LOOPS
        for (;x < border - 16; x += 16) {
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
            LEFT_BORDER_ITER
        }
#endif

        for (;x < border; ++x) {
            LEFT_BORDER_ITER
        }
#undef LEFT_BORDER_ITER
        for (int x = width; x < diameter; ++x) {
            *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24;
            dptr += dst_x_stride;
        }
        x = diameter;

#define CENTER_ITER \
            inner_sum = outer_sum - *left; \
            outer_sum += *right++; \
            *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24; \
            dptr += dst_x_stride; \
            outer_sum -= *left++;

#ifdef UNROLL_SEPARABLE_LOOPS
        for (; x < width - 16; x += 16) {
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
            CENTER_ITER
        }
#endif
        for (; x < width; ++x) {
            CENTER_ITER
        }
#undef CENTER_ITER

        #define RIGHT_BORDER_ITER \
            inner_sum = outer_sum - *left++; \
            *dptr = (outer_sum * outer_scale + inner_sum * inner_scale + half) >> 24; \
            dptr += dst_x_stride; \
            outer_sum = inner_sum;

        x = 0;
#ifdef UNROLL_SEPARABLE_LOOPS
        for (; x < border - 16; x += 16) {
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
            RIGHT_BORDER_ITER
        }
#endif
        for (; x < border; ++x) {
            RIGHT_BORDER_ITER
        }
#undef RIGHT_BORDER_ITER
        SkASSERT(outer_sum == 0 && inner_sum == 0);
    }
    return new_width;
}

static void get_adjusted_radii(SkScalar passRadius, int *loRadius, int *hiRadius)
{
    *loRadius = *hiRadius = SkScalarCeilToInt(passRadius);
    if (SkIntToScalar(*hiRadius) - passRadius > 0.5f) {
        *loRadius = *hiRadius - 1;
    }
}

#include "SkColorPriv.h"

static void merge_src_with_blur(uint8_t dst[], int dstRB,
                                const uint8_t src[], int srcRB,
                                const uint8_t blur[], int blurRB,
                                int sw, int sh) {
    dstRB -= sw;
    srcRB -= sw;
    blurRB -= sw;
    while (--sh >= 0) {
        for (int x = sw - 1; x >= 0; --x) {
            *dst = SkToU8(SkAlphaMul(*blur, SkAlpha255To256(*src)));
            dst += 1;
            src += 1;
            blur += 1;
        }
        dst += dstRB;
        src += srcRB;
        blur += blurRB;
    }
}

static void clamp_with_orig(uint8_t dst[], int dstRowBytes,
                            const uint8_t src[], int srcRowBytes,
                            int sw, int sh,
                            SkBlurMask::Style style) {
    int x;
    while (--sh >= 0) {
        switch (style) {
        case SkBlurMask::kSolid_Style:
            for (x = sw - 1; x >= 0; --x) {
                int s = *src;
                int d = *dst;
                *dst = SkToU8(s + d - SkMulDiv255Round(s, d));
                dst += 1;
                src += 1;
            }
            break;
        case SkBlurMask::kOuter_Style:
            for (x = sw - 1; x >= 0; --x) {
                if (*src) {
                    *dst = SkToU8(SkAlphaMul(*dst, SkAlpha255To256(255 - *src)));
                }
                dst += 1;
                src += 1;
            }
            break;
        default:
            SkDEBUGFAIL("Unexpected blur style here");
            break;
        }
        dst += dstRowBytes - sw;
        src += srcRowBytes - sw;
    }
}

///////////////////////////////////////////////////////////////////////////////

// we use a local function to wrap the class static method to work around
// a bug in gcc98
void SkMask_FreeImage(uint8_t* image);
void SkMask_FreeImage(uint8_t* image) {
    SkMask::FreeImage(image);
}

bool SkBlurMask::Blur(SkMask* dst, const SkMask& src,
                      SkScalar radius, Style style, Quality quality,
                      SkIPoint* margin) {
    return SkBlurMask::BoxBlur(dst, src,
                               SkBlurMask::ConvertRadiusToSigma(radius),
                               style, quality, margin);
}

bool SkBlurMask::BoxBlur(SkMask* dst, const SkMask& src,
                         SkScalar sigma, Style style, Quality quality,
                         SkIPoint* margin) {

    if (src.fFormat != SkMask::kA8_Format) {
        return false;
    }

    // Force high quality off for small radii (performance)
    if (sigma <= SkIntToScalar(2)) {
        quality = kLow_Quality;
    }

    SkScalar passRadius;
    if (kHigh_Quality == quality) {
        // For the high quality path the 3 pass box blur kernel width is
        // 6*rad+1 while the full Gaussian width is 6*sigma.
        passRadius = sigma - (1/6.0f);
    } else {
        // For the low quality path we only attempt to cover 3*sigma of the
        // Gaussian blur area (1.5*sigma on each side). The single pass box
        // blur's kernel size is 2*rad+1.
        passRadius = 1.5f*sigma - 0.5f;
    }

    // highQuality: use three box blur passes as a cheap way
    // to approximate a Gaussian blur
    int passCount = (kHigh_Quality == quality) ? 3 : 1;

    int rx = SkScalarCeilToInt(passRadius);
    int outerWeight = 255 - SkScalarRoundToInt((SkIntToScalar(rx) - passRadius) * 255);

    SkASSERT(rx >= 0);
    SkASSERT((unsigned)outerWeight <= 255);
    if (rx <= 0) {
        return false;
    }

    int ry = rx;    // only do square blur for now

    int padx = passCount * rx;
    int pady = passCount * ry;

    if (margin) {
        margin->set(padx, pady);
    }
    dst->fBounds.set(src.fBounds.fLeft - padx, src.fBounds.fTop - pady,
                     src.fBounds.fRight + padx, src.fBounds.fBottom + pady);

    dst->fRowBytes = dst->fBounds.width();
    dst->fFormat = SkMask::kA8_Format;
    dst->fImage = NULL;

    if (src.fImage) {
        size_t dstSize = dst->computeImageSize();
        if (0 == dstSize) {
            return false;   // too big to allocate, abort
        }

        int             sw = src.fBounds.width();
        int             sh = src.fBounds.height();
        const uint8_t*  sp = src.fImage;
        uint8_t*        dp = SkMask::AllocImage(dstSize);
        SkAutoTCallVProc<uint8_t, SkMask_FreeImage> autoCall(dp);

        // build the blurry destination
        SkAutoTMalloc<uint8_t>  tmpBuffer(dstSize);
        uint8_t*                tp = tmpBuffer.get();
        int w = sw, h = sh;

        if (outerWeight == 255) {
            int loRadius, hiRadius;
            get_adjusted_radii(passRadius, &loRadius, &hiRadius);
            if (kHigh_Quality == quality) {
                // Do three X blurs, with a transpose on the final one.
                w = boxBlur(sp, src.fRowBytes, tp, loRadius, hiRadius, w, h, false);
                w = boxBlur(tp, w,             dp, hiRadius, loRadius, w, h, false);
                w = boxBlur(dp, w,             tp, hiRadius, hiRadius, w, h, true);
                // Do three Y blurs, with a transpose on the final one.
                h = boxBlur(tp, h,             dp, loRadius, hiRadius, h, w, false);
                h = boxBlur(dp, h,             tp, hiRadius, loRadius, h, w, false);
                h = boxBlur(tp, h,             dp, hiRadius, hiRadius, h, w, true);
            } else {
                w = boxBlur(sp, src.fRowBytes, tp, rx, rx, w, h, true);
                h = boxBlur(tp, h,             dp, ry, ry, h, w, true);
            }
        } else {
            if (kHigh_Quality == quality) {
                // Do three X blurs, with a transpose on the final one.
                w = boxBlurInterp(sp, src.fRowBytes, tp, rx, w, h, false, outerWeight);
                w = boxBlurInterp(tp, w,             dp, rx, w, h, false, outerWeight);
                w = boxBlurInterp(dp, w,             tp, rx, w, h, true, outerWeight);
                // Do three Y blurs, with a transpose on the final one.
                h = boxBlurInterp(tp, h,             dp, ry, h, w, false, outerWeight);
                h = boxBlurInterp(dp, h,             tp, ry, h, w, false, outerWeight);
                h = boxBlurInterp(tp, h,             dp, ry, h, w, true, outerWeight);
            } else {
                w = boxBlurInterp(sp, src.fRowBytes, tp, rx, w, h, true, outerWeight);
                h = boxBlurInterp(tp, h,             dp, ry, h, w, true, outerWeight);
            }
        }

        dst->fImage = dp;
        // if need be, alloc the "real" dst (same size as src) and copy/merge
        // the blur into it (applying the src)
        if (style == kInner_Style) {
            // now we allocate the "real" dst, mirror the size of src
            size_t srcSize = src.computeImageSize();
            if (0 == srcSize) {
                return false;   // too big to allocate, abort
            }
            dst->fImage = SkMask::AllocImage(srcSize);
            merge_src_with_blur(dst->fImage, src.fRowBytes,
                                sp, src.fRowBytes,
                                dp + passCount * (rx + ry * dst->fRowBytes),
                                dst->fRowBytes, sw, sh);
            SkMask::FreeImage(dp);
        } else if (style != kNormal_Style) {
            clamp_with_orig(dp + passCount * (rx + ry * dst->fRowBytes),
                            dst->fRowBytes, sp, src.fRowBytes, sw, sh, style);
        }
        (void)autoCall.detach();
    }

    if (style == kInner_Style) {
        dst->fBounds = src.fBounds; // restore trimmed bounds
        dst->fRowBytes = src.fRowBytes;
    }

    return true;
}

/* Convolving a box with itself three times results in a piecewise
   quadratic function:

   0                              x <= -1.5
   9/8 + 3/2 x + 1/2 x^2   -1.5 < x <= -.5
   3/4 - x^2                -.5 < x <= .5
   9/8 - 3/2 x + 1/2 x^2    0.5 < x <= 1.5
   0                        1.5 < x

   Mathematica:

   g[x_] := Piecewise [ {
     {9/8 + 3/2 x + 1/2 x^2 ,  -1.5 < x <= -.5},
     {3/4 - x^2             ,   -.5 < x <= .5},
     {9/8 - 3/2 x + 1/2 x^2 ,   0.5 < x <= 1.5}
   }, 0]

   To get the profile curve of the blurred step function at the rectangle
   edge, we evaluate the indefinite integral, which is piecewise cubic:

   0                                        x <= -1.5
   9/16 + 9/8 x + 3/4 x^2 + 1/6 x^3   -1.5 < x <= -0.5
   1/2 + 3/4 x - 1/3 x^3              -.5 < x <= .5
   7/16 + 9/8 x - 3/4 x^2 + 1/6 x^3     .5 < x <= 1.5
   1                                  1.5 < x

   in Mathematica code:

   gi[x_] := Piecewise[ {
     { 0 , x <= -1.5 },
     { 9/16 + 9/8 x + 3/4 x^2 + 1/6 x^3, -1.5 < x <= -0.5 },
     { 1/2 + 3/4 x - 1/3 x^3          ,  -.5 < x <= .5},
     { 7/16 + 9/8 x - 3/4 x^2 + 1/6 x^3,   .5 < x <= 1.5}
   },1]
*/

static float gaussianIntegral(float x) {
    if (x > 1.5f) {
        return 0.0f;
    }
    if (x < -1.5f) {
        return 1.0f;
    }

    float x2 = x*x;
    float x3 = x2*x;

    if ( x > 0.5f ) {
        return 0.5625f - (x3 / 6.0f - 3.0f * x2 * 0.25f + 1.125f * x);
    }
    if ( x > -0.5f ) {
        return 0.5f - (0.75f * x - x3 / 3.0f);
    }
    return 0.4375f + (-x3 / 6.0f - 3.0f * x2 * 0.25f - 1.125f * x);
}

/*  ComputeBlurProfile allocates and fills in an array of floating
    point values between 0 and 255 for the profile signature of
    a blurred half-plane with the given blur radius.  Since we're
    going to be doing screened multiplications (i.e., 1 - (1-x)(1-y))
    all the time, we actually fill in the profile pre-inverted
    (already done 255-x).

    It's the responsibility of the caller to delete the
    memory returned in profile_out.
*/

void SkBlurMask::ComputeBlurProfile(SkScalar sigma, uint8_t **profile_out) {
    int size = SkScalarCeilToInt(6*sigma);

    int center = size >> 1;
    uint8_t *profile = SkNEW_ARRAY(uint8_t, size);

    float invr = 1.f/(2*sigma);

    profile[0] = 255;
    for (int x = 1 ; x < size ; ++x) {
        float scaled_x = (center - x - .5f) * invr;
        float gi = gaussianIntegral(scaled_x);
        profile[x] = 255 - (uint8_t) (255.f * gi);
    }

    *profile_out = profile;
}

// TODO MAYBE: Maintain a profile cache to avoid recomputing this for
// commonly used radii.  Consider baking some of the most common blur radii
// directly in as static data?

// Implementation adapted from Michael Herf's approach:
// http://stereopsis.com/shadowrect/

uint8_t SkBlurMask::ProfileLookup(const uint8_t *profile, int loc, int blurred_width, int sharp_width) {
    int dx = SkAbs32(((loc << 1) + 1) - blurred_width) - sharp_width; // how far are we from the original edge?
    int ox = dx >> 1;
    if (ox < 0) {
        ox = 0;
    }

    return profile[ox];
}

void SkBlurMask::ComputeBlurredScanline(uint8_t *pixels, const uint8_t *profile,
                                        unsigned int width, SkScalar sigma) {

    unsigned int profile_size = SkScalarCeilToInt(6*sigma);
    SkAutoTMalloc<uint8_t> horizontalScanline(width);

    unsigned int sw = width - profile_size;
    // nearest odd number less than the profile size represents the center
    // of the (2x scaled) profile
    int center = ( profile_size & ~1 ) - 1;

    int w = sw - center;

    for (unsigned int x = 0 ; x < width ; ++x) {
       if (profile_size <= sw) {
           pixels[x] = ProfileLookup(profile, x, width, w);
       } else {
           float span = float(sw)/(2*sigma);
           float giX = 1.5f - (x+.5f)/(2*sigma);
           pixels[x] = (uint8_t) (255 * (gaussianIntegral(giX) - gaussianIntegral(giX + span)));
       }
    }
}

bool SkBlurMask::BlurRect(SkMask *dst, const SkRect &src,
                          SkScalar radius, Style style,
                          SkIPoint *margin, SkMask::CreateMode createMode) {
    return SkBlurMask::BlurRect(SkBlurMask::ConvertRadiusToSigma(radius),
                                dst, src,
                                style, margin, createMode);
}

bool SkBlurMask::BlurRect(SkScalar sigma, SkMask *dst,
                          const SkRect &src, Style style,
                          SkIPoint *margin, SkMask::CreateMode createMode) {
    int profile_size = SkScalarCeilToInt(6*sigma);

    int pad = profile_size/2;
    if (margin) {
        margin->set( pad, pad );
    }

    dst->fBounds.set(SkScalarRoundToInt(src.fLeft - pad),
                     SkScalarRoundToInt(src.fTop - pad),
                     SkScalarRoundToInt(src.fRight + pad),
                     SkScalarRoundToInt(src.fBottom + pad));

    dst->fRowBytes = dst->fBounds.width();
    dst->fFormat = SkMask::kA8_Format;
    dst->fImage = NULL;

    int             sw = SkScalarFloorToInt(src.width());
    int             sh = SkScalarFloorToInt(src.height());

    if (createMode == SkMask::kJustComputeBounds_CreateMode) {
        if (style == kInner_Style) {
            dst->fBounds.set(SkScalarRoundToInt(src.fLeft),
                             SkScalarRoundToInt(src.fTop),
                             SkScalarRoundToInt(src.fRight),
                             SkScalarRoundToInt(src.fBottom)); // restore trimmed bounds
            dst->fRowBytes = sw;
        }
        return true;
    }
    uint8_t *profile = NULL;

    ComputeBlurProfile(sigma, &profile);
    SkAutoTDeleteArray<uint8_t> ada(profile);

    size_t dstSize = dst->computeImageSize();
    if (0 == dstSize) {
        return false;   // too big to allocate, abort
    }

    uint8_t*        dp = SkMask::AllocImage(dstSize);

    dst->fImage = dp;

    int dstHeight = dst->fBounds.height();
    int dstWidth = dst->fBounds.width();

    uint8_t *outptr = dp;

    SkAutoTMalloc<uint8_t> horizontalScanline(dstWidth);
    SkAutoTMalloc<uint8_t> verticalScanline(dstHeight);

    ComputeBlurredScanline(horizontalScanline, profile, dstWidth, sigma);
    ComputeBlurredScanline(verticalScanline, profile, dstHeight, sigma);

    for (int y = 0 ; y < dstHeight ; ++y) {
        for (int x = 0 ; x < dstWidth ; x++) {
            unsigned int maskval = SkMulDiv255Round(horizontalScanline[x], verticalScanline[y]);
            *(outptr++) = maskval;
        }
    }

    if (style == kInner_Style) {
        // now we allocate the "real" dst, mirror the size of src
        size_t srcSize = (size_t)(src.width() * src.height());
        if (0 == srcSize) {
            return false;   // too big to allocate, abort
        }
        dst->fImage = SkMask::AllocImage(srcSize);
        for (int y = 0 ; y < sh ; y++) {
            uint8_t *blur_scanline = dp + (y+pad)*dstWidth + pad;
            uint8_t *inner_scanline = dst->fImage + y*sw;
            memcpy(inner_scanline, blur_scanline, sw);
        }
        SkMask::FreeImage(dp);

        dst->fBounds.set(SkScalarRoundToInt(src.fLeft),
                         SkScalarRoundToInt(src.fTop),
                         SkScalarRoundToInt(src.fRight),
                         SkScalarRoundToInt(src.fBottom)); // restore trimmed bounds
        dst->fRowBytes = sw;

    } else if (style == kOuter_Style) {
        for (int y = pad ; y < dstHeight-pad ; y++) {
            uint8_t *dst_scanline = dp + y*dstWidth + pad;
            memset(dst_scanline, 0, sw);
        }
    } else if (style == kSolid_Style) {
        for (int y = pad ; y < dstHeight-pad ; y++) {
            uint8_t *dst_scanline = dp + y*dstWidth + pad;
            memset(dst_scanline, 0xff, sw);
        }
    }
    // normal and solid styles are the same for analytic rect blurs, so don't
    // need to handle solid specially.

    return true;
}

bool SkBlurMask::BlurGroundTruth(SkMask* dst, const SkMask& src, SkScalar radius,
                                 Style style, SkIPoint* margin) {
    return BlurGroundTruth(ConvertRadiusToSigma(radius), dst, src, style, margin);
}
// The "simple" blur is a direct implementation of separable convolution with a discrete
// gaussian kernel.  It's "ground truth" in a sense; too slow to be used, but very
// useful for correctness comparisons.

bool SkBlurMask::BlurGroundTruth(SkScalar sigma, SkMask* dst, const SkMask& src,
                                 Style style, SkIPoint* margin) {

    if (src.fFormat != SkMask::kA8_Format) {
        return false;
    }

    float variance = sigma * sigma;

    int windowSize = SkScalarCeilToInt(sigma*6);
    // round window size up to nearest odd number
    windowSize |= 1;

    SkAutoTMalloc<float> gaussWindow(windowSize);

    int halfWindow = windowSize >> 1;

    gaussWindow[halfWindow] = 1;

    float windowSum = 1;
    for (int x = 1 ; x <= halfWindow ; ++x) {
        float gaussian = expf(-x*x / (2*variance));
        gaussWindow[halfWindow + x] = gaussWindow[halfWindow-x] = gaussian;
        windowSum += 2*gaussian;
    }

    // leave the filter un-normalized for now; we will divide by the normalization
    // sum later;

    int pad = halfWindow;
    if (margin) {
        margin->set( pad, pad );
    }

    dst->fBounds = src.fBounds;
    dst->fBounds.outset(pad, pad);

    dst->fRowBytes = dst->fBounds.width();
    dst->fFormat = SkMask::kA8_Format;
    dst->fImage = NULL;

    if (src.fImage) {

        size_t dstSize = dst->computeImageSize();
        if (0 == dstSize) {
            return false;   // too big to allocate, abort
        }

        int             srcWidth = src.fBounds.width();
        int             srcHeight = src.fBounds.height();
        int             dstWidth = dst->fBounds.width();

        const uint8_t*  srcPixels = src.fImage;
        uint8_t*        dstPixels = SkMask::AllocImage(dstSize);
        SkAutoTCallVProc<uint8_t, SkMask_FreeImage> autoCall(dstPixels);

        // do the actual blur.  First, make a padded copy of the source.
        // use double pad so we never have to check if we're outside anything

        int padWidth = srcWidth + 4*pad;
        int padHeight = srcHeight;
        int padSize = padWidth * padHeight;

        SkAutoTMalloc<uint8_t> padPixels(padSize);
        memset(padPixels, 0, padSize);

        for (int y = 0 ; y < srcHeight; ++y) {
            uint8_t* padptr = padPixels + y * padWidth + 2*pad;
            const uint8_t* srcptr = srcPixels + y * srcWidth;
            memcpy(padptr, srcptr, srcWidth);
        }

        // blur in X, transposing the result into a temporary floating point buffer.
        // also double-pad the intermediate result so that the second blur doesn't
        // have to do extra conditionals.

        int tmpWidth = padHeight + 4*pad;
        int tmpHeight = padWidth - 2*pad;
        int tmpSize = tmpWidth * tmpHeight;

        SkAutoTMalloc<float> tmpImage(tmpSize);
        memset(tmpImage, 0, tmpSize*sizeof(tmpImage[0]));

        for (int y = 0 ; y < padHeight ; ++y) {
            uint8_t *srcScanline = padPixels + y*padWidth;
            for (int x = pad ; x < padWidth - pad ; ++x) {
                float *outPixel = tmpImage + (x-pad)*tmpWidth + y + 2*pad; // transposed output
                uint8_t *windowCenter = srcScanline + x;
                for (int i = -pad ; i <= pad ; ++i) {
                    *outPixel += gaussWindow[pad+i]*windowCenter[i];
                }
                *outPixel /= windowSum;
            }
        }

        // blur in Y; now filling in the actual desired destination.  We have to do
        // the transpose again; these transposes guarantee that we read memory in
        // linear order.

        for (int y = 0 ; y < tmpHeight ; ++y) {
            float *srcScanline = tmpImage + y*tmpWidth;
            for (int x = pad ; x < tmpWidth - pad ; ++x) {
                float *windowCenter = srcScanline + x;
                float finalValue = 0;
                for (int i = -pad ; i <= pad ; ++i) {
                    finalValue += gaussWindow[pad+i]*windowCenter[i];
                }
                finalValue /= windowSum;
                uint8_t *outPixel = dstPixels + (x-pad)*dstWidth + y; // transposed output
                int integerPixel = int(finalValue + 0.5f);
                *outPixel = SkClampMax( SkClampPos(integerPixel), 255 );
            }
        }

        dst->fImage = dstPixels;
        // if need be, alloc the "real" dst (same size as src) and copy/merge
        // the blur into it (applying the src)
        if (style == kInner_Style) {
            // now we allocate the "real" dst, mirror the size of src
            size_t srcSize = src.computeImageSize();
            if (0 == srcSize) {
                return false;   // too big to allocate, abort
            }
            dst->fImage = SkMask::AllocImage(srcSize);
            merge_src_with_blur(dst->fImage, src.fRowBytes,
                srcPixels, src.fRowBytes,
                dstPixels + pad*dst->fRowBytes + pad,
                dst->fRowBytes, srcWidth, srcHeight);
            SkMask::FreeImage(dstPixels);
        } else if (style != kNormal_Style) {
            clamp_with_orig(dstPixels + pad*dst->fRowBytes + pad,
                dst->fRowBytes, srcPixels, src.fRowBytes, srcWidth, srcHeight, style);
        }
        (void)autoCall.detach();
    }

    if (style == kInner_Style) {
        dst->fBounds = src.fBounds; // restore trimmed bounds
        dst->fRowBytes = src.fRowBytes;
    }

    return true;
}
