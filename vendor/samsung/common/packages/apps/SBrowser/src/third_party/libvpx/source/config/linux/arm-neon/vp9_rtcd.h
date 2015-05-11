#ifndef VP9_RTCD_H_
#define VP9_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * VP9
 */

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_enums.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct vp9_variance_vtable;

#define DEC_MVCOSTS int *mvjcost, int *mvcost[2]
struct mv;
union int_mv;
struct yv12_buffer_config;

void vp9_d207_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_4x4 vp9_d207_predictor_4x4_c

void vp9_d45_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_4x4 vp9_d45_predictor_4x4_c

void vp9_d63_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_4x4 vp9_d63_predictor_4x4_c

void vp9_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_4x4 vp9_h_predictor_4x4_neon

void vp9_d117_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_4x4 vp9_d117_predictor_4x4_c

void vp9_d135_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_4x4 vp9_d135_predictor_4x4_c

void vp9_d153_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_4x4 vp9_d153_predictor_4x4_c

void vp9_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_4x4 vp9_v_predictor_4x4_neon

void vp9_tm_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_tm_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_4x4 vp9_tm_predictor_4x4_neon

void vp9_dc_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_4x4 vp9_dc_predictor_4x4_c

void vp9_dc_top_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_4x4 vp9_dc_top_predictor_4x4_c

void vp9_dc_left_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_4x4 vp9_dc_left_predictor_4x4_c

void vp9_dc_128_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_4x4 vp9_dc_128_predictor_4x4_c

void vp9_d207_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_8x8 vp9_d207_predictor_8x8_c

void vp9_d45_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_8x8 vp9_d45_predictor_8x8_c

void vp9_d63_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_8x8 vp9_d63_predictor_8x8_c

void vp9_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_8x8 vp9_h_predictor_8x8_neon

void vp9_d117_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_8x8 vp9_d117_predictor_8x8_c

void vp9_d135_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_8x8 vp9_d135_predictor_8x8_c

void vp9_d153_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_8x8 vp9_d153_predictor_8x8_c

void vp9_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_8x8 vp9_v_predictor_8x8_neon

void vp9_tm_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_tm_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_8x8 vp9_tm_predictor_8x8_neon

void vp9_dc_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_8x8 vp9_dc_predictor_8x8_c

void vp9_dc_top_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_8x8 vp9_dc_top_predictor_8x8_c

void vp9_dc_left_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_8x8 vp9_dc_left_predictor_8x8_c

void vp9_dc_128_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_8x8 vp9_dc_128_predictor_8x8_c

void vp9_d207_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_16x16 vp9_d207_predictor_16x16_c

void vp9_d45_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_16x16 vp9_d45_predictor_16x16_c

void vp9_d63_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_16x16 vp9_d63_predictor_16x16_c

void vp9_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_16x16 vp9_h_predictor_16x16_neon

void vp9_d117_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_16x16 vp9_d117_predictor_16x16_c

void vp9_d135_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_16x16 vp9_d135_predictor_16x16_c

void vp9_d153_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_16x16 vp9_d153_predictor_16x16_c

void vp9_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_16x16 vp9_v_predictor_16x16_neon

void vp9_tm_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_tm_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_16x16 vp9_tm_predictor_16x16_neon

void vp9_dc_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_16x16 vp9_dc_predictor_16x16_c

void vp9_dc_top_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_16x16 vp9_dc_top_predictor_16x16_c

void vp9_dc_left_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_16x16 vp9_dc_left_predictor_16x16_c

void vp9_dc_128_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_16x16 vp9_dc_128_predictor_16x16_c

void vp9_d207_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_32x32 vp9_d207_predictor_32x32_c

void vp9_d45_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_32x32 vp9_d45_predictor_32x32_c

void vp9_d63_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_32x32 vp9_d63_predictor_32x32_c

void vp9_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_32x32 vp9_h_predictor_32x32_neon

void vp9_d117_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_32x32 vp9_d117_predictor_32x32_c

void vp9_d135_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_32x32 vp9_d135_predictor_32x32_c

void vp9_d153_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_32x32 vp9_d153_predictor_32x32_c

void vp9_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_32x32 vp9_v_predictor_32x32_neon

void vp9_tm_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_tm_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_32x32 vp9_tm_predictor_32x32_neon

void vp9_dc_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_32x32 vp9_dc_predictor_32x32_c

void vp9_dc_top_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_32x32 vp9_dc_top_predictor_32x32_c

void vp9_dc_left_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_32x32 vp9_dc_left_predictor_32x32_c

void vp9_dc_128_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_32x32 vp9_dc_128_predictor_32x32_c

void vp9_lpf_vertical_16_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void vp9_lpf_vertical_16_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define vp9_lpf_vertical_16 vp9_lpf_vertical_16_neon

void vp9_lpf_vertical_16_dual_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void vp9_lpf_vertical_16_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define vp9_lpf_vertical_16_dual vp9_lpf_vertical_16_dual_neon

void vp9_lpf_vertical_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_vertical_8_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_lpf_vertical_8 vp9_lpf_vertical_8_neon

void vp9_lpf_vertical_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_vertical_8_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define vp9_lpf_vertical_8_dual vp9_lpf_vertical_8_dual_neon

void vp9_lpf_vertical_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_vertical_4_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_lpf_vertical_4 vp9_lpf_vertical_4_neon

void vp9_lpf_vertical_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_vertical_4_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define vp9_lpf_vertical_4_dual vp9_lpf_vertical_4_dual_neon

void vp9_lpf_horizontal_16_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_horizontal_16_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_lpf_horizontal_16 vp9_lpf_horizontal_16_neon

void vp9_lpf_horizontal_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_horizontal_8_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_lpf_horizontal_8 vp9_lpf_horizontal_8_neon

void vp9_lpf_horizontal_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_horizontal_8_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define vp9_lpf_horizontal_8_dual vp9_lpf_horizontal_8_dual_neon

void vp9_lpf_horizontal_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_horizontal_4_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_lpf_horizontal_4 vp9_lpf_horizontal_4_neon

void vp9_lpf_horizontal_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_horizontal_4_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define vp9_lpf_horizontal_4_dual vp9_lpf_horizontal_4_dual_neon

void vp9_blend_mb_inner_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_inner vp9_blend_mb_inner_c

void vp9_blend_mb_outer_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_outer vp9_blend_mb_outer_c

void vp9_blend_b_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_b vp9_blend_b_c

void vp9_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve_copy_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve_copy vp9_convolve_copy_neon

void vp9_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve_avg_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve_avg vp9_convolve_avg_neon

void vp9_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8 vp9_convolve8_neon

void vp9_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_horiz_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_horiz vp9_convolve8_horiz_neon

void vp9_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_vert_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_vert vp9_convolve8_vert_neon

void vp9_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg vp9_convolve8_avg_neon

void vp9_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_horiz_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_horiz vp9_convolve8_avg_horiz_neon

void vp9_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_vert_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_vert vp9_convolve8_avg_vert_neon

void vp9_idct4x4_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct4x4_1_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct4x4_1_add vp9_idct4x4_1_add_neon

void vp9_idct4x4_16_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct4x4_16_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct4x4_16_add vp9_idct4x4_16_add_neon

void vp9_idct8x8_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_1_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct8x8_1_add vp9_idct8x8_1_add_neon

void vp9_idct8x8_64_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_64_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct8x8_64_add vp9_idct8x8_64_add_neon

void vp9_idct8x8_10_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_10_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct8x8_10_add vp9_idct8x8_10_add_neon

void vp9_idct16x16_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_1_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct16x16_1_add vp9_idct16x16_1_add_neon

void vp9_idct16x16_256_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_256_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct16x16_256_add vp9_idct16x16_256_add_neon

void vp9_idct16x16_10_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_10_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct16x16_10_add vp9_idct16x16_10_add_neon

void vp9_idct32x32_1024_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_1024_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct32x32_1024_add vp9_idct32x32_1024_add_neon

void vp9_idct32x32_34_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_1024_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct32x32_34_add vp9_idct32x32_1024_add_neon

void vp9_idct32x32_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_1_add_neon(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_idct32x32_1_add vp9_idct32x32_1_add_neon

void vp9_iht4x4_16_add_c(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
void vp9_iht4x4_16_add_neon(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp9_iht4x4_16_add vp9_iht4x4_16_add_neon

void vp9_iht8x8_64_add_c(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
void vp9_iht8x8_64_add_neon(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp9_iht8x8_64_add vp9_iht8x8_64_add_neon

void vp9_iht16x16_256_add_c(const int16_t *input, uint8_t *output, int pitch, int tx_type);
#define vp9_iht16x16_256_add vp9_iht16x16_256_add_c

void vp9_iwht4x4_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_iwht4x4_1_add vp9_iwht4x4_1_add_c

void vp9_iwht4x4_16_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_iwht4x4_16_add vp9_iwht4x4_16_add_c

void vp9_rtcd(void);
#include "vpx_config.h"

#ifdef RTCD_C
#include "vpx_ports/arm.h"
static void setup_rtcd_internal(void)
{
    int flags = arm_cpu_caps();

    (void)flags;


}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
