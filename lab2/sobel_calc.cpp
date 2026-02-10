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
  unsigned char *img_data = img.data; // data at this pointer is BGR BGR BGR
  unsigned char *gray_data = img_gray_out.data;

  // if both are 0 then this is single thread
  if (startRow == 0 && endRow == 0) {
    endRow = IMG_HEIGHT;
  }

  // Convert row bounds into pixel index bounds - grayscalle buff is flat
  int startPx = startRow * IMG_WIDTH;
  int endPx = endRow * IMG_WIDTH;

  // Process 8 pixels at a time using neon intrinsics
  int i = startPx;
  for (; i <= endPx - 8; i += 8) {
    // Load 8 BGR (blue green red interleaved) pixels into separate B, G, R channels
    uint8x8x3_t rgb = vld3_u8(&img_data[i * 3]); //vector load three 8bit uints

    // Widen to 16-bit to avoid overflow
    uint16x8_t b = vmovl_u8(rgb.val[0]);
    uint16x8_t g = vmovl_u8(rgb.val[1]);
    uint16x8_t r = vmovl_u8(rgb.val[2]);

    // Fixed point grayscalle approximation:
    // gray = (7 Blue + 38 Green + 19 Red) / 64
    // 0.114 ~ 7/64, 0.587 ~ 38/64, 0.299 ~ 19/64

    // Multiply the B G R channels and accumulate into gray vector
    uint16x8_t gray = vmulq_n_u16(b, 7); // vector multiply qextended nconstant
    gray = vmlaq_n_u16(gray, g, 38); // vector multiply accumulate
    gray = vmlaq_n_u16(gray, r, 19);

    // Divide by 64
    gray = vshrq_n_u16(gray, 6); // vector shift right qextended nconstant

    // Narrow back to 8-bit and store
    vst1_u8(&gray_data[i], vmovn_u16(gray)); // vector move narrow
  }

  // individually handling pixels in case the total number of pixels don't split into 8 
  for (; i < endPx; i++) {
    int index = i * 3; // BGR index into color buffer is 3 bytes per pixel
    gray_data[i] = ( // manual multiply add into division by 2^6 = 64
      7 * img_data[index] + 
      38 * img_data[index + 1] + 
      19 * img_data[index + 2]) >> 6;
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
  unsigned char *gray = img_gray.data; // gray is 1 byte per pixel
  unsigned char *sobel = img_sobel_out.data; // likewise for sobel

  // If both 0, process the whole image
  if (startRow == 0 && endRow == 0) {
    startRow = 1;
    endRow = IMG_HEIGHT - 1;
  }

  for (int i = startRow; i < endRow; i++) {
    int j;

    // precompute for readability and efficient reuse
    int row = IMG_WIDTH * i;
    int row_above = row - IMG_WIDTH;
    int row_below = row + IMG_WIDTH;

    // Process 8 pixels at a time using neon intrinsics. j runs over columns
    // stops at IMG_WIDTH - 9 so that j+1 to j+9 are in bounds
    // ie instead of stopping at IMG_WIDTH-1 due to 0 indexing, we stop 8 before that
    for (j = 1; j <= IMG_WIDTH - 9; j += 8) {
      // Load 8 pixels from each of the 9 positions since we don't need the middle
      uint8x8_t top_l = vld1_u8(&gray[row_above+ j - 1]);
      uint8x8_t top_m = vld1_u8(&gray[row_above+ j]);
      uint8x8_t top_r = vld1_u8(&gray[row_above+ j + 1]);
      uint8x8_t mid_l = vld1_u8(&gray[row + j - 1]);
      uint8x8_t mid_r = vld1_u8(&gray[row + j + 1]);
      uint8x8_t bot_l = vld1_u8(&gray[row_below+ j - 1]);
      uint8x8_t bot_m = vld1_u8(&gray[row_below+ j]);
      uint8x8_t bot_r = vld1_u8(&gray[row_below+ j + 1]);

      // Widen to signed 16-bit for subtraction
      int16x8_t p00 = vreinterpretq_s16_u16(vmovl_u8(top_l));
      int16x8_t p01 = vreinterpretq_s16_u16(vmovl_u8(top_m));
      int16x8_t p02 = vreinterpretq_s16_u16(vmovl_u8(top_r));
      int16x8_t p10 = vreinterpretq_s16_u16(vmovl_u8(mid_l));
      int16x8_t p12 = vreinterpretq_s16_u16(vmovl_u8(mid_r));
      int16x8_t p20 = vreinterpretq_s16_u16(vmovl_u8(bot_l));
      int16x8_t p21 = vreinterpretq_s16_u16(vmovl_u8(bot_m));
      int16x8_t p22 = vreinterpretq_s16_u16(vmovl_u8(bot_r));

      // Sobel Gx kernel:
      //  [ -1  0 +1 ]
      //  [ -2  0 +2 ]
      //  [ -1  0 +1 ]
      // Implement as (right column weighted sum) - (left column weighted sum).
      // Gx = (p02 + 2*p12 + p22) - (p00 + 2*p10 + p20)
      int16x8_t gx = vsubq_s16(
          vaddq_s16(vaddq_s16(p02, vshlq_n_s16(p12, 1)), p22),
          vaddq_s16(vaddq_s16(p00, vshlq_n_s16(p10, 1)), p20));

      // Sobel Gy kernel:
      //  [ -1 -2 -1 ]
      //  [  0  0  0 ]
      //  [ +1 +2 +1 ]
      // Implement as (bottom row weighted sum) - (top row weighted sum).
      // Gy = (p20 + 2*p21 + p22) - (p00 + 2*p01 + p02)
      int16x8_t gy = vsubq_s16(
          vaddq_s16(vaddq_s16(p20, vshlq_n_s16(p21, 1)), p22),
          vaddq_s16(vaddq_s16(p00, vshlq_n_s16(p01, 1)), p02));

      // |gx| + |gy| to approximate magnitudes
      int16x8_t mag = vaddq_s16(vabsq_s16(gx), vabsq_s16(gy));

      // Saturate/narrow signed 16-bit -> unsigned 8-bit.
      // Negative becomes 0, >255 becomes 255.      
      uint8x8_t result = vqmovun_s16(mag);

      // Store 8 results
      vst1_u8(&sobel[row + j], result);
    }

    // individually handling pixels in case the total number of pixels don't split into 8 
    for (; j < IMG_WIDTH - 1; j++) {
      int idx_top = IMG_WIDTH * (i-1) + j;
      int idx_mid = IMG_WIDTH * i + j;
      int idx_bot = IMG_WIDTH * (i+1) + j;

      // local 3x3 grid of scalars instead of bytevectors
      int p00 = gray[idx_top - 1];
      int p01 = gray[idx_top];
      int p02 = gray[idx_top + 1];
      int p10 = gray[idx_mid - 1];
      int p12 = gray[idx_mid + 1];
      int p20 = gray[idx_bot - 1];
      int p21 = gray[idx_bot];
      int p22 = gray[idx_bot + 1];

      // same math process, magnitude and clamping
      int gx = (p02 + (p12 << 1) + p22) - (p00 + (p10 << 1) + p20);
      int gy = (p20 + (p21 << 1) + p22) - (p00 + (p01 << 1) + p02);

      int magnitude = abs(gx) + abs(gy);
      sobel[idx_mid] = (magnitude > 255) ? 255 : magnitude;
    }
  }
}
