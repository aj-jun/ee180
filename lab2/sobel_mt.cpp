#include <stdio.h>
#include <stdlib.h>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <iostream>
#include <fstream>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <sys/ioctl.h>
#include <err.h>

#include "sobel_alg.h"
#include "pc.h"

using namespace cv;

static ofstream results_file;

/// Define image mats to pass between function calls
static Mat src;
static Mat img_gray, img_sobel;
static float total_fps, total_ipc, total_epf;
static float gray_total, sobel_total, cap_total, disp_total;
static float sobel_ic_total, sobel_l1cm_total;

// signal for when both threads are done
static volatile int is_mt_done = 0;

/*******************************************
 * Model: runSobelMT
 * Input: None
 * Output: None
 * Desc: This method pulls in an image from the webcam, feeds it into the
 *   sobelCalc module, and displays the returned Sobel filtered image. This
 *   function processes NUM_ITER frames.
 ********************************************/
void *runSobelMT(void *ptr)
{
  string top = "Sobel Top";
  uint64_t cap_time = 0, gray_time = 0, sobel_time = 0, disp_time = 0, sobel_l1cm = 0, sobel_ic = 0;
  pthread_t myID = pthread_self();
  counters_t perf_counters;

  // Allow the threads to contest for thread0 (controller thread) status
  pthread_mutex_lock(&thread0);
  if (thread0_id == 0) {
    thread0_id = myID;
  }
  pthread_mutex_unlock(&thread0);

  bool isThread0 = (myID == thread0_id);

  // Determine row ranges for work splitting
  int gray_start, gray_end;
  int sobel_start, sobel_end;

  if (isThread0) {
    gray_start  = 0;
    gray_end    = IMG_HEIGHT / 2;
    sobel_start = 1;
    sobel_end   = IMG_HEIGHT / 2;
  } else {
    gray_start  = IMG_HEIGHT / 2;
    gray_end    = IMG_HEIGHT;
    sobel_start = IMG_HEIGHT / 2;
    sobel_end   = IMG_HEIGHT - 1;
  }

  CvCapture* video_cap = NULL;

  if (isThread0) {
    pc_init(&perf_counters, 0);

    if (opts.webcam) {
      video_cap = cvCreateCameraCapture(-1);
    } else {
      video_cap = cvCreateFileCapture(opts.videoFile);
    }
    cvSetCaptureProperty(video_cap, CV_CAP_PROP_FRAME_WIDTH, IMG_WIDTH);
    cvSetCaptureProperty(video_cap, CV_CAP_PROP_FRAME_HEIGHT, IMG_HEIGHT);

    // Allocate shared image buffers once
    img_gray = Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1);
    img_sobel = Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1);
  }

  int i = 0;

  while (1) {
    // ===== PHASE 1: CAPTURE (thread 0 only) =====
    if (isThread0) {
      pc_start(&perf_counters);
      src = cvQueryFrame(video_cap);
      pc_stop(&perf_counters);

      cap_time = perf_counters.cycles.count;
      sobel_l1cm = perf_counters.l1_misses.count;
      sobel_ic = perf_counters.ic.count;
    }

    // barrier to start grayscale
    pthread_barrier_wait(&barr_capture);

    // run grayscale
    if (isThread0) pc_start(&perf_counters);

    grayScale(src, img_gray, gray_start, gray_end);

    // barrier for completing grayscale
    pthread_barrier_wait(&barr_gray);

    if (isThread0) {
      pc_stop(&perf_counters);
      gray_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count;
    }

    // starting sobel
    if (isThread0) pc_start(&perf_counters);

    sobelCalc(img_gray, img_sobel, sobel_start, sobel_end);

    // barrier to complete sobel
    pthread_barrier_wait(&barr_sobel);

    if (isThread0) {
      pc_stop(&perf_counters);
      sobel_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count;

      // display using 1st thread
      pc_start(&perf_counters);
      namedWindow(top, CV_WINDOW_AUTOSIZE);
      imshow(top, img_sobel);
      pc_stop(&perf_counters);

      disp_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count;

      cap_total += cap_time;
      gray_total += gray_time;
      sobel_total += sobel_time;
      sobel_l1cm_total += sobel_l1cm;
      sobel_ic_total += sobel_ic;
      disp_total += disp_time;
      total_fps += PROC_FREQ / float(cap_time + disp_time + gray_time + sobel_time);
      total_ipc += float(sobel_ic / float(cap_time + disp_time + gray_time + sobel_time));
      i++;

      // Press q to exit
      char c = cvWaitKey(10);
      if (c == 'q' || i >= opts.numFrames) {
        is_mt_done = 1;
      }
    }

    // barrier for both threads to finish
    pthread_barrier_wait(&barr_display);

    if (is_mt_done) break;
  }

  // write and clean up report
  if (isThread0) {
    total_epf = PROC_EPC * 2 / (total_fps / i);
    float total_time = float(gray_total + sobel_total + cap_total + disp_total);

    results_file.open("mt_perf.csv", ios::out);
    results_file << "Percent of time per function" << endl;
    results_file << "Capture, " << (cap_total / total_time) * 100 << "%" << endl;
    results_file << "Grayscale, " << (gray_total / total_time) * 100 << "%" << endl;
    results_file << "Sobel, " << (sobel_total / total_time) * 100 << "%" << endl;
    results_file << "Display, " << (disp_total / total_time) * 100 << "%" << endl;
    results_file << "\nSummary" << endl;
    results_file << "Frames per second, " << total_fps / i << endl;
    results_file << "Cycles per frame, " << total_time / i << endl;
    results_file << "Energy per frames (mJ), " << total_epf * 1000 << endl;
    results_file << "Total frames, " << i << endl;
    results_file << "\nHardware Stats (Cap + Gray + Sobel + Display)" << endl;
    results_file << "Instructions per cycle, " << total_ipc / i << endl;
    results_file << "L1 misses per frame, " << sobel_l1cm_total / i << endl;
    results_file << "L1 misses per instruction, " << sobel_l1cm_total / sobel_ic_total << endl;
    results_file << "Instruction count per frame, " << sobel_ic_total / i << endl;

    cvReleaseCapture(&video_cap);
    results_file.close();
  }

  pthread_barrier_wait(&endSobel);
  return NULL;
}
