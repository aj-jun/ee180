#include "opencv2/imgproc/imgproc.hpp"
#include "sobel_alg.h"
using namespace cv;

/*******************************************
 * Model: grayScale - OPTIMIZED VERSION
 * Input: Mat img
 * Output: None directly. Modifies a ref parameter img_gray_out
 * Desc: This module converts the image to grayscale
 *
 * OPTIMIZATIONS APPLIED:
 * 1. Integer arithmetic instead of floating point
 * 2. Pointer arithmetic for faster memory access
 * 3. Loop unrolling hints for compiler
 * 4. Better cache locality with sequential access
 * 5. Pre-computed index offsets
 ********************************************/
void grayScale(Mat& img, Mat& img_gray_out)
{
  // Use pointers for direct memory access (faster than array indexing)
  unsigned char *img_data = img.data;
  unsigned char *gray_data = img_gray_out.data;

  // Process entire image with optimized loop
  // IMG_WIDTH * IMG_HEIGHT = 640 * 480 = 307200 pixels
  int total_pixels = IMG_WIDTH * IMG_HEIGHT;

  // Use integer arithmetic with fixed-point math
  // Original: 0.114*B + 0.587*G + 0.299*R
  // Convert to integer: (114*B + 587*G + 299*R) / 1000
  // Further optimize: (29*B + 150*G + 77*R) / 256 (divide by 256 is bit shift)
  // Best approximation: (7*B + 38*G + 19*R) / 64

  for (int i = 0; i < total_pixels; i++) {
    int idx = i * 3;  // RGB has 3 bytes per pixel

    // Fixed-point arithmetic: multiply then shift right by 6 (divide by 64)
    // This is much faster than floating point on ARM
    unsigned char blue = img_data[idx];
    unsigned char green = img_data[idx + 1];
    unsigned char red = img_data[idx + 2];

    gray_data[i] = (7*blue + 38*green + 19*red) >> 6;
  }
}

/*******************************************
 * Model: sobelCalc - OPTIMIZED VERSION
 * Input: Mat img_gray
 * Output: None directly. Modifies a ref parameter img_sobel_out
 * Desc: This module performs a sobel calculation on an image
 *
 * OPTIMIZATIONS APPLIED:
 * 1. Eliminated unnecessary Mat cloning (saves memory allocation)
 * 2. Combined Gx and Gy calculation in single loop (reduces passes)
 * 3. Direct pointer arithmetic for memory access
 * 4. Simplified convolution math (fewer operations)
 * 5. Better cache locality - compute Gx and Gy together
 * 6. Removed intermediate buffers
 * 7. Loop unrolling friendly structure
 ********************************************/
/*******************************************
 * Model: grayScale (row-range) - MULTI-THREADED VERSION
 * Input: Mat img, start/end rows
 * Output: None directly. Modifies a ref parameter img_gray_out
 * Desc: Converts a row range to grayscale (for parallel processing)
 *
 * PART 2: Each thread calls this with its assigned row range
 *   Thread 0: rows [0, IMG_HEIGHT/2)
 *   Thread 1: rows [IMG_HEIGHT/2, IMG_HEIGHT)
 ********************************************/
void grayScale(Mat& img, Mat& img_gray_out, int startRow, int endRow)
{
  unsigned char *img_data = img.data;
  unsigned char *gray_data = img_gray_out.data;

  int startPx = startRow * IMG_WIDTH;
  int endPx = endRow * IMG_WIDTH;

  for (int i = startPx; i < endPx; i++) {
    int idx = i * 3;
    unsigned char blue = img_data[idx];
    unsigned char green = img_data[idx + 1];
    unsigned char red = img_data[idx + 2];
    gray_data[i] = (7*blue + 38*green + 19*red) >> 6;
  }
}

/*******************************************
 * Model: sobelCalc (row-range) - MULTI-THREADED VERSION
 * Input: Mat img_gray, start/end rows
 * Output: None directly. Modifies a ref parameter img_sobel_out
 * Desc: Performs sobel on a row range (for parallel processing)
 *
 * PART 2: Each thread calls this with its assigned row range
 *   Thread 0: rows [1, IMG_HEIGHT/2)
 *   Thread 1: rows [IMG_HEIGHT/2, IMG_HEIGHT-1)
 * Note: Grayscale barrier must complete before calling this,
 *       since the 3x3 kernel reads across the thread boundary.
 ********************************************/
void sobelCalc(Mat& img_gray, Mat& img_sobel_out, int startRow, int endRow)
{
  unsigned char *gray = img_gray.data;
  unsigned char *sobel = img_sobel_out.data;

  for (int i = startRow; i < endRow; i++) {
    for (int j = 1; j < IMG_WIDTH - 1; j++) {
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

      int mag = abs(gx) + abs(gy);
      sobel[idx_mid] = (mag > 255) ? 255 : mag;
    }
  }
}

void sobelCalc(Mat& img_gray, Mat& img_sobel_out)
{
  // Use pointers for direct memory access
  unsigned char *gray = img_gray.data;
  unsigned char *sobel = img_sobel_out.data;

  // Sobel kernels:
  // Gx = [-1  0  1]      Gy = [-1 -2 -1]
  //      [-2  0  2]           [ 0  0  0]
  //      [-1  0  1]           [ 1  2  1]

  // Process image starting from (1,1) to avoid border
  for (int i = 1; i < IMG_HEIGHT - 1; i++) {
    for (int j = 1; j < IMG_WIDTH - 1; j++) {
      // Calculate base indices for 3x3 neighborhood
      int idx_top = IMG_WIDTH * (i-1) + j;
      int idx_mid = IMG_WIDTH * i + j;
      int idx_bot = IMG_WIDTH * (i+1) + j;

      // Load 3x3 neighborhood pixels
      // Top row: [i-1, j-1], [i-1, j], [i-1, j+1]
      int p00 = gray[idx_top - 1];
      int p01 = gray[idx_top];
      int p02 = gray[idx_top + 1];

      // Middle row: [i, j-1], [i, j], [i, j+1]
      int p10 = gray[idx_mid - 1];
      // int p11 = gray[idx_mid];  // Center pixel not used in Sobel
      int p12 = gray[idx_mid + 1];

      // Bottom row: [i+1, j-1], [i+1, j], [i+1, j+1]
      int p20 = gray[idx_bot - 1];
      int p21 = gray[idx_bot];
      int p22 = gray[idx_bot + 1];

      // Calculate Gx (horizontal gradient)
      // Gx = (p02 + 2*p12 + p22) - (p00 + 2*p10 + p20)
      int gx = (p02 + (p12 << 1) + p22) - (p00 + (p10 << 1) + p20);

      // Calculate Gy (vertical gradient)
      // Gy = (p20 + 2*p21 + p22) - (p00 + 2*p01 + p02)
      int gy = (p20 + (p21 << 1) + p22) - (p00 + (p01 << 1) + p02);

      // Compute magnitude: |Gx| + |Gy| (L1 norm, faster than L2)
      int mag = abs(gx) + abs(gy);

      // Clamp to [0, 255]
      sobel[idx_mid] = (mag > 255) ? 255 : mag;
    }
  }
}
