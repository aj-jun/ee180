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

// Shared data between threads
static Mat src;
static Mat img_gray, img_sobel;
static float total_fps, total_ipc, total_epf;
static float gray_total, sobel_total, cap_total, disp_total;
static float sobel_ic_total, sobel_l1cm_total;

// Flag to signal both threads to exit
static volatile int mt_done = 0;

/*******************************************
 * Model: runSobelMT
 * Input: None
 * Output: None
 * Desc: Multi-threaded Sobel filter implementation.
 *   Two threads split the image processing work:
 *   - Thread 0 (controller): captures frame, processes top half, displays result
 *   - Thread 1 (worker): processes bottom half
 *   Synchronization uses 4 barriers per frame:
 *     barr_capture: frame captured, data ready
 *     barr_gray:    grayscale complete on both halves
 *     barr_sobel:   sobel complete on both halves
 *     barr_display: iteration done, safe to loop
 ********************************************/
void *runSobelMT(void *ptr)
{
  string top = "Sobel Top";
  uint64_t cap_time, gray_time, sobel_time, disp_time, sobel_l1cm, sobel_ic;
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
  // Grayscale: each thread converts its half of the image
  // Sobel: each thread computes its half (avoiding border rows 0 and IMG_HEIGHT-1)
  int gray_start, gray_end;
  int sobel_start, sobel_end;

  if (isThread0) {
    gray_start  = 0;
    gray_end    = IMG_HEIGHT / 2;     // rows [0, 240)
    sobel_start = 1;                  // skip top border
    sobel_end   = IMG_HEIGHT / 2;     // rows [1, 240)
  } else {
    gray_start  = IMG_HEIGHT / 2;
    gray_end    = IMG_HEIGHT;         // rows [240, 480)
    sobel_start = IMG_HEIGHT / 2;
    sobel_end   = IMG_HEIGHT - 1;     // rows [240, 479), skip bottom border
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

    // Allocate shared image buffers once (same size every frame)
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

    // BARRIER 1: Frame captured, both threads can start grayscale
    pthread_barrier_wait(&barr_capture);

    // ===== PHASE 2: GRAYSCALE (parallel - each thread does half) =====
    if (isThread0) pc_start(&perf_counters);

    grayScale(src, img_gray, gray_start, gray_end);

    // BARRIER 2: Grayscale complete on both halves
    // Must sync here because Sobel's 3x3 kernel reads across the boundary
    pthread_barrier_wait(&barr_gray);

    if (isThread0) {
      pc_stop(&perf_counters);
      gray_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count;
    }

    // ===== PHASE 3: SOBEL (parallel - each thread does half) =====
    if (isThread0) pc_start(&perf_counters);

    sobelCalc(img_gray, img_sobel, sobel_start, sobel_end);

    // BARRIER 3: Sobel complete on both halves
    pthread_barrier_wait(&barr_sobel);

    if (isThread0) {
      pc_stop(&perf_counters);
      sobel_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count;

      // ===== PHASE 4: DISPLAY (thread 0 only) =====
      pc_start(&perf_counters);
      namedWindow(top, CV_WINDOW_AUTOSIZE);
      imshow(top, img_sobel);
      pc_stop(&perf_counters);

      disp_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count;

      // Accumulate performance stats
      cap_total += cap_time;
      gray_total += gray_time;
      sobel_total += sobel_time;
      sobel_l1cm_total += sobel_l1cm;
      sobel_ic_total += sobel_ic;
      disp_total += disp_time;
      total_fps += PROC_FREQ / float(cap_time + disp_time + gray_time + sobel_time);
      total_ipc += float(sobel_ic / float(cap_time + disp_time + gray_time + sobel_time));
      i++;

      // Check exit condition
      char c = cvWaitKey(10);
      if (c == 'q' || i >= opts.numFrames) {
        mt_done = 1;
      }
    }

    // BARRIER 4: Iteration complete, both threads check done flag together
    pthread_barrier_wait(&barr_display);

    if (mt_done) break;
  }

  // Thread 0 writes results and cleans up
  if (isThread0) {
    // Use NCORES=2 for energy calculation in multi-threaded mode
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
