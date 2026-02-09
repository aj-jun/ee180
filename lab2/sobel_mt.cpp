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

// Replaces img.step[0] and img.step[1] calls in sobel calc

using namespace cv;

static ofstream results_file;

// Define image mats to pass between function calls
static Mat img_gray, img_sobel;
static float total_fps, total_ipc, total_epf;
static float gray_total, sobel_total, cap_total, disp_total;
static float sobel_ic_total, sobel_l1cm_total;

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
  // Set up variables for computing Sobel
  string top = "Sobel Top";
  Mat src;
  uint64_t cap_time, gray_time, sobel_time, disp_time, sobel_l1cm, sobel_ic;
  pthread_t myID = pthread_self();
  counters_t perf_counters;

  // Allow the threads to contest for thread0 (controller thread) status
  pthread_mutex_lock(&thread0);

  // Check to see if this thread is first to this part of the code
  if (thread0_id == 0) {
    thread0_id = myID;
  }
  pthread_mutex_unlock(&thread0);

  // distinction between two threads
  bool isThread0 = (myID == thread0_id);

  // split image into two
  int gray_start, gray_end;
  int sobel_start, sobel_end;

  // set gray start and stop and sobel start and stop 
  if (isThread0) {
    gray_start = 0;
    gray_end = IMG_HEIGHT / 2;
    sobel_start = 1;
    sobel_end = IMG_HEIGHT / 2;
  } else {
    gray_start = IMG_HEIGHT / 2;
    gray_end = IMG_HEIGHT;
    sobel_start = IMG_HEIGHT / 2;
    sobel_end = IMG_HEIGHT;
  }
  
  // move things to be included in the 1st thread
  if (isThread0) {
    pc_init(&perf_counters, 0);

    // Start algorithm
    CvCapture* video_cap;

    if (opts.webcam) {
      video_cap = cvCreateCameraCapture(-1);
    } else {
      video_cap = cvCreateFileCapture(opts.videoFile);
    }

    video_cap = cvCreateFileCapture(opts.videoFile);
    cvSetCaptureProperty(video_cap, CV_CAP_PROP_FRAME_WIDTH, IMG_WIDTH);
    cvSetCaptureProperty(video_cap, CV_CAP_PROP_FRAME_HEIGHT, IMG_HEIGHT);

  }



  // if (myID != thread0_id) {
  //   pthread_barrier_wait(&endSobel);
  //   return NULL;
  // }

  
  // Keep track of the frames
  int i = 0;

  while (1) {
    // Allocate memory to hold grayscale and sobel images
    img_gray = Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1);
    img_sobel = Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1);
    if (isThread0) {
      pc_start(&perf_counters);
      src = cvQueryFrame(video_cap);
      pc_stop(&perf_counters);

      cap_time = perf_counters.cycles.count;
      sobel_l1cm = perf_counters.l1_misses.count;
      sobel_ic = perf_counters.ic.count;
    }
    
    // wait until the frame is captured for before starting grayscale for both 
    pthread_barrier_wait(&barr_capture);
    // run grayscale
    if (isThread0) {
      pc_start(&perf_counters);
    }
    grayScale(src, img_gray, gray_start, gray_end);

    // wait until grayscale is done for both before sobel
    pthread_barrier_wait(&barr_capture);
    
    if (isThread0) {
      pc_stop(&perf_counters);
      gray_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count;    
    }
    
    // run sobel for each thread in parallel
    if (isThread0) {
      pc_start(&perf_counters);
    }
    sobelCalc(img_gray, img_sobel);

    // wait until both sobel is completed
    pthread_barrier_wait(&barr_capture);
    if (isThread0) {
      pc_stop(&perf_counters);
      sobel_time = perf_counters.cycles.count;
      sobel_l1cm += perf_counters.l1_misses.count;
      sobel_ic += perf_counters.ic.count; 
      // display the report using the first thread & exit based on when it is completed
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
      total_fps += PROC_FREQ/float(cap_time + disp_time + gray_time + sobel_time);
      total_ipc += float(sobel_ic/float(cap_time + disp_time + gray_time + sobel_time));
      i++;

      // Press q to exit
      char c = cvWaitKey(10);
      if (c == 'q' || i >= opts.numFrames) {
        is_mt_done = 1;
      }
    }
    
    // wait until both threads are done to mark completion
    pthread_barrier_wait(&barr_display);
    if (mt_done) break;
    // write report and clean up allocated   
  }


  if (isThread0) {
    total_epf = PROC_EPC*NCORES/(total_fps/i);
    float total_time = float(gray_total + sobel_total + cap_total + disp_total);

    results_file.open("mt_perf.csv", ios::out);
    results_file << "Percent of time per function" << endl;
    results_file << "Capture, " << (cap_total/total_time)*100 << "%" << endl;
    results_file << "Grayscale, " << (gray_total/total_time)*100 << "%" << endl;
    results_file << "Sobel, " << (sobel_total/total_time)*100 << "%" << endl;
    results_file << "Display, " << (disp_total/total_time)*100 << "%" << endl;
    results_file << "\nSummary" << endl;
    results_file << "Frames per second, " << total_fps/i << endl;
    results_file << "Cycles per frame, " << total_time/i << endl;
    results_file << "Energy per frames (mJ), " << total_epf*1000 << endl;
    results_file << "Total frames, " << i << endl;
    results_file << "\nHardware Stats (Cap + Gray + Sobel + Display)" << endl;
    results_file << "Instructions per cycle, " << total_ipc/i << endl;
    results_file << "L1 misses per frame, " << sobel_l1cm_total/i << endl;
    results_file << "L1 misses per instruction, " << sobel_l1cm_total/sobel_ic_total << endl;
    results_file << "Instruction count per frame, " << sobel_ic_total/i << endl;

    cvReleaseCapture(&video_cap);
    results_file.close();
  }

  
  pthread_barrier_wait(&endSobel);
  return NULL;
}
