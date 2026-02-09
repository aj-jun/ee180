#include "opencv2/imgproc/imgproc.hpp"
#include "sobel_alg.h"
using namespace cv;

/*******************************************
 * Model: grayScale
 * Input: Mat img
 * Output: None directly. Modifies a ref parameter img_gray_out
 * Desc: This module converts the image to grayscale
 ********************************************/
void grayScale(Mat& img, Mat& img_gray_out)
{
  // double color;

  // // Convert to grayscale
  // for (int i=0; i<img.rows; i++) {
  //   for (int j=0; j<img.cols; j++) {
  //     color = .114*img.data[STEP0*i + STEP1*j] +
  //             .587*img.data[STEP0*i + STEP1*j + 1] +
  //             .299*img.data[STEP0*i + STEP1*j + 2];
  //     img_gray_out.data[IMG_WIDTH*i + j] = color;
  //   }
  // }
  int tot_pixels = IMG_HEIGHT * IMG_WIDTH;
  // where we want to put it in img_gray_out.data

  for (int i=0; i < tot_pixels; i++) {
    // 3 bytes per pixel
    int index = i * 3;

    // pixels : blue, green, red
    unsigned char blue = .114*img.data[index];
    unsigned char green = .587*img.data[index + 1];
    unsigned char red = .299*img.data[index + 2];
    // final placement
    img_gray_out.data[i] = (blue + green + red);


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
void sobelCalc(Mat& img_gray, Mat& img_sobel_out)
{

  // Gy and Gx together
  for (int i = 1; i < IMG_HEIGHT - 1; i++) {
    for (int j = 1; j < IMG_WIDTH - 1; j++) {
      int idx_top = IMG_WIDTH * (i-1) + j;
      int idx_mid = IMG_WIDTH * i + j;
      int idx_bot = IMG_WIDTH * (i+1) + j;

      // calcluate each of the 9 combinations from the pre-made 3 indices
      int p00 = img_gray.data[idx_top - 1];
      int p01 = img_gray.data[idx_top];
      int p02 = img_gray.data[idx_top + 1];
      int p10 = img_gray.data[idx_mid - 1];
      int p12 = img_gray.data[idx_mid + 1];
      int p20 = img_gray.data[idx_bot - 1];
      int p21 = img_gray.data[idx_bot];
      int p22 = img_gray.data[idx_bot + 1];

      // calculate gx and gy
      int gx = (p02 + (p12 << 1) + p22) - (p00 + (p10 << 1) + p20);
      int gy = (p20 + (p21 << 1) + p22) - (p00 + (p01 << 1) + p02);

      // combine the magnitutudes and check that it is < 255
      int magnitude = abs(gx) + abs(gy);
      img_sobel_out.data[idx_mid] = (mag > 255) ? 255 : magnitude;
    }
  }
}

  // // Calculate the x convolution
  // for (int i=1; i<img_gray.rows; i++) {
  //   for (int j=1; j<img_gray.cols; j++) {
  //     sobel = abs(img_gray.data[IMG_WIDTH*(i-1) + (j-1)] -
	// 	  img_gray.data[IMG_WIDTH*(i+1) + (j-1)] +
	// 	  2*img_gray.data[IMG_WIDTH*(i-1) + (j)] -
	// 	  2*img_gray.data[IMG_WIDTH*(i+1) + (j)] +
	// 	  img_gray.data[IMG_WIDTH*(i-1) + (j+1)] -
	// 	  img_gray.data[IMG_WIDTH*(i+1) + (j+1)]);

  //     sobel = (sobel > 255) ? 255 : sobel;
  //     img_outx.data[IMG_WIDTH*(i) + (j)] = sobel;
  //   }
  // }

  // // Calc the y convolution
  // for (int i=1; i<img_gray.rows; i++) {
  //   for (int j=1; j<img_gray.cols; j++) {
  //    sobel = abs(img_gray.data[IMG_WIDTH*(i-1) + (j-1)] -
	// 	   img_gray.data[IMG_WIDTH*(i-1) + (j+1)] +
	// 	   2*img_gray.data[IMG_WIDTH*(i) + (j-1)] -
	// 	   2*img_gray.data[IMG_WIDTH*(i) + (j+1)] +
	// 	   img_gray.data[IMG_WIDTH*(i+1) + (j-1)] -
	// 	   img_gray.data[IMG_WIDTH*(i+1) + (j+1)]);

  //    sobel = (sobel > 255) ? 255 : sobel;

  //    img_outy.data[IMG_WIDTH*(i) + j] = sobel;
  //   }
  // }

  // // Combine the two convolutions into the output image
  // for (int i=1; i<img_gray.rows; i++) {
  //   for (int j=1; j<img_gray.cols; j++) {
  //     sobel = img_outx.data[IMG_WIDTH*(i) + j] +
	// img_outy.data[IMG_WIDTH*(i) + j];
  //     sobel = (sobel > 255) ? 255 : sobel;
  //     img_sobel_out.data[IMG_WIDTH*(i) + j] = sobel;
  //   }
  // }
