#include "opencv2/imgproc/imgproc.hpp"
#include "sobel_alg.h"
#include <arm_neon.h>
using namespace cv;

/*******************************************
 * Model: grayScale
 * Input: Mat img
 * Output: None directly. Modifies a ref parameter img_gray_out
 * Desc: This module converts the image to grayscale
 ********************************************/
void grayScale(Mat& img, Mat& img_gray_out, int startRow, int endRow)
{
  unsigned char *img_data = img.data;
  unsigned char *gray_data = img_gray_out.data;

  // if both are 0 then this is single thread
  if (startRow == 0 && endRow == 0) {
    endRow = IMG_HEIGHT;
  }

  int startPx = startRow * IMG_WIDTH;
  int endPx = endRow * IMG_WIDTH;

  // Process 8 pixels at a time using neon float intrinsics
  int i = startPx;
  for (; i <= endPx - 8; i += 8) {
    // Load 8 RGB pixels into separate B, G, R channels
    uint8x8x3_t rgb = vld3_u8(&img_data[i * 3]);

    // Convert lower 4 pixels to float
    float32x4_t b_lo = vcvtq_f32_u32(vmovl_u16(vget_low_u16(vmovl_u8(rgb.val[0]))));
    float32x4_t g_lo = vcvtq_f32_u32(vmovl_u16(vget_low_u16(vmovl_u8(rgb.val[1]))));
    float32x4_t r_lo = vcvtq_f32_u32(vmovl_u16(vget_low_u16(vmovl_u8(rgb.val[2]))));

    // Convert upper 4 pixels to float
    float32x4_t b_hi = vcvtq_f32_u32(vmovl_u16(vget_high_u16(vmovl_u8(rgb.val[0]))));
    float32x4_t g_hi = vcvtq_f32_u32(vmovl_u16(vget_high_u16(vmovl_u8(rgb.val[1]))));
    float32x4_t r_hi = vcvtq_f32_u32(vmovl_u16(vget_high_u16(vmovl_u8(rgb.val[2]))));

    // gray = 0.114*B + 0.587*G + 0.299*R
    float32x4_t gray_lo = vmulq_n_f32(b_lo, 0.114f);
    gray_lo = vmlaq_n_f32(gray_lo, g_lo, 0.587f);
    gray_lo = vmlaq_n_f32(gray_lo, r_lo, 0.299f);

    float32x4_t gray_hi = vmulq_n_f32(b_hi, 0.114f);
    gray_hi = vmlaq_n_f32(gray_hi, g_hi, 0.587f);
    gray_hi = vmlaq_n_f32(gray_hi, r_hi, 0.299f);

    // Convert back to uint8 and store
    uint16x4_t lo_u16 = vmovn_u32(vcvtq_u32_f32(gray_lo));
    uint16x4_t hi_u16 = vmovn_u32(vcvtq_u32_f32(gray_hi));
    uint8x8_t result = vmovn_u16(vcombine_u16(lo_u16, hi_u16));
    vst1_u8(&gray_data[i], result);
  }

  // Handle remaining pixels
  for (; i < endPx; i++) {
    int index = i * 3;
    gray_data[i] = (unsigned char)(0.114f * img_data[index] + 0.587f * img_data[index + 1] + 0.299f * img_data[index + 2]);
  }
}

/*******************************************
 * Model: sobelCalc
 * Input: Mat img_in
 * Output: None directly. Modifies a ref parameter img_sobel_out
 * Desc: This module performs a sobel calculation on an image. It first
 *  converts the image to grayscale, calculates the gradient in the x
 *  direction, calculates the gradient in the y direction and sum it with Gx
 *  to finish the Sobel calculation
 ********************************************/
void sobelCalc(Mat& img_gray, Mat& img_sobel_out, int startRow, int endRow)
{
  unsigned char *gray = img_gray.data;
  unsigned char *sobel = img_sobel_out.data;

  // If both 0, process the whole image
  if (startRow == 0 && endRow == 0) {
    startRow = 1;
    endRow = IMG_HEIGHT - 1;
  }

  for (int i = startRow; i < endRow; i++) {
    int j;

    // Process 8 pixels at a time using NEON
    for (j = 1; j <= IMG_WIDTH - 9; j += 8) {
      // Load 8 pixels from each of the 9 neighbor positions
      uint8x8_t top_l = vld1_u8(&gray[IMG_WIDTH*(i-1) + j - 1]);
      uint8x8_t top_m = vld1_u8(&gray[IMG_WIDTH*(i-1) + j]);
      uint8x8_t top_r = vld1_u8(&gray[IMG_WIDTH*(i-1) + j + 1]);
      uint8x8_t mid_l = vld1_u8(&gray[IMG_WIDTH*i + j - 1]);
      uint8x8_t mid_r = vld1_u8(&gray[IMG_WIDTH*i + j + 1]);
      uint8x8_t bot_l = vld1_u8(&gray[IMG_WIDTH*(i+1) + j - 1]);
      uint8x8_t bot_m = vld1_u8(&gray[IMG_WIDTH*(i+1) + j]);
      uint8x8_t bot_r = vld1_u8(&gray[IMG_WIDTH*(i+1) + j + 1]);

      // Widen to signed 16-bit for subtraction
      int16x8_t p00 = vreinterpretq_s16_u16(vmovl_u8(top_l));
      int16x8_t p01 = vreinterpretq_s16_u16(vmovl_u8(top_m));
      int16x8_t p02 = vreinterpretq_s16_u16(vmovl_u8(top_r));
      int16x8_t p10 = vreinterpretq_s16_u16(vmovl_u8(mid_l));
      int16x8_t p12 = vreinterpretq_s16_u16(vmovl_u8(mid_r));
      int16x8_t p20 = vreinterpretq_s16_u16(vmovl_u8(bot_l));
      int16x8_t p21 = vreinterpretq_s16_u16(vmovl_u8(bot_m));
      int16x8_t p22 = vreinterpretq_s16_u16(vmovl_u8(bot_r));

      // Gx = (p02 + 2*p12 + p22) - (p00 + 2*p10 + p20)
      int16x8_t gx = vsubq_s16(
          vaddq_s16(vaddq_s16(p02, vshlq_n_s16(p12, 1)), p22),
          vaddq_s16(vaddq_s16(p00, vshlq_n_s16(p10, 1)), p20));

      // Gy = (p20 + 2*p21 + p22) - (p00 + 2*p01 + p02)
      int16x8_t gy = vsubq_s16(
          vaddq_s16(vaddq_s16(p20, vshlq_n_s16(p21, 1)), p22),
          vaddq_s16(vaddq_s16(p00, vshlq_n_s16(p01, 1)), p02));

      // |gx| + |gy|
      int16x8_t mag = vaddq_s16(vabsq_s16(gx), vabsq_s16(gy));

      // Saturating narrow to uint8 — clamps to [0, 255]
      uint8x8_t result = vqmovun_s16(mag);

      // Store 8 results
      vst1_u8(&sobel[IMG_WIDTH*i + j], result);
    }

    // Handle remaining pixels with scalar code
    for (; j < IMG_WIDTH - 1; j++) {
      int idx_top = IMG_WIDTH * (i-1) + j;
      int idx_mid = IMG_WIDTH * i + j;
      int idx_bot = IMG_WIDTH * (i+1) + j;

      int p00 = gray[idx_top - 1];
      int p01 = gray[idx_top];
      int p02 = gray[idx_top + 1];
      int p10 = gray[idx_mid - 1];
      int p12 = gray[idx_mid + 1];
      int p20 = gray[idx_bot - 1];
      int p21 = gray[idx_bot];
      int p22 = gray[idx_bot + 1];

      int gx = (p02 + (p12 << 1) + p22) - (p00 + (p10 << 1) + p20);
      int gy = (p20 + (p21 << 1) + p22) - (p00 + (p01 << 1) + p02);

      int magnitude = abs(gx) + abs(gy);
      sobel[idx_mid] = (magnitude > 255) ? 255 : magnitude;
    }
  }
}
