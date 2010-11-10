#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // close
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
//#include <arpa/inet.h>

/* Device parameters */
#define V4L2_DEVICE     "/dev/video0"

/* Macro for clearing structures */
#define CLEAR(x) memset (&(x), 0, sizeof (x))

// Configures the height and width of the frame.
// NOTE: Changing values here alone will not help, you also 
// need to change values in the driver since its generated VD and HD signals
#define D1_WIDTH        128 
#define D1_HEIGHT       16000 
#define D1_FRAME_SIZE   D1_WIDTH * D1_HEIGHT
#define NUM_CAPTURE_BUFS 3

#include "fsr172x.h"
int fsr_dqbuf();

//! Describes a capture frame buffer
struct sCaptureBuffer {
  void         *start;        //!< Starting memory location of buffer
  unsigned long offset;        //!< Offset into buffer to first valid data
  size_t        length;        //!< Length of data
};

int captureFd = 0;
struct sCaptureBuffer* m_CapBufs; //!< Holds the capture buffer memory maps to the CCDC memory regions
struct v4l2_buffer m_V4l2Buf;  //!< Contains returned data from a capture frame indicating which memory is aquired


/**
*  
*  @brief Initializes and configures the CCD controller for capture
*
*  @return Returns 0 on success
*/
int fsr_init()
{
  struct v4l2_requestbuffers  req;
  struct v4l2_capability      cap;
  struct v4l2_format          fmt;
  struct v4l2_buffer          buf;

  enum v4l2_buf_type          type;

  unsigned int                numCapBufs;

  captureFd = 0;
  m_CapBufs = NULL;
  
  printf("FSR: Initializing CCD controller.\n");

  /* Open video capture device */
  captureFd = open(V4L2_DEVICE, O_RDWR | O_NONBLOCK, 0);

  if (captureFd == -1) {
    printf("FSR: Cannot open.\n");
    return 1;
  }


  printf("FSR: Capture device opened. CaptureWidth = %d, CaptureHeight = %d\n", D1_WIDTH, D1_HEIGHT);

  // Query for capture device capabilities 
  if (ioctl(captureFd, VIDIOC_QUERYCAP, &cap) == -1) {
    if (errno == EINVAL) {
      printf("FSR: %s is not a V4L2 device\n", V4L2_DEVICE);
      return 1;
    }
    printf("FSR: Failed VIDIOC_QUERYCAP on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }


  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    printf("FSR: %s is not a video capture device\n", V4L2_DEVICE);
    return 1;
  }

  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    printf("FSR: %s does not support streaming i/o\n", V4L2_DEVICE);
    return 1;
  }

  
  // Set the video capture image format
  CLEAR(fmt);
  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  //fmt.fmt.pix.width       = D1_WIDTH;
  //fmt.fmt.pix.height      = D1_HEIGHT;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_FSR172X; // ISP driver associates YUYV with SYNC mode
  fmt.fmt.pix.field       = V4L2_FIELD_NONE;    

  // Set the video capture format 
  if (ioctl(captureFd, VIDIOC_S_FMT, &fmt) == -1) {
    printf("FSR: VIDIOC_S_FMT failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }

  printf("FSR: Video capture format is set. \n");


  //  VIDIOC_S_CROP is not required since we are not using the smoothing function 

  CLEAR(req);
  req.count  = NUM_CAPTURE_BUFS;
  req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  // Allocate buffers in the capture device driver 
  if (ioctl(captureFd, VIDIOC_REQBUFS, &req) == -1) {
    printf("FSR: VIDIOC_REQBUFS failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }

  printf("FSR: %d capture buffers were successfully allocated.\n", req.count);

  if (req.count < NUM_CAPTURE_BUFS) {
    printf("FSR: Insufficient buffer memory on %s\n", V4L2_DEVICE);
    return 1;
  }

  m_CapBufs = (struct sCaptureBuffer *)calloc(req.count, sizeof(*m_CapBufs));

  if (!m_CapBufs) {
    printf("FSR: Failed to allocate memory for capture buffer structs.\n");
    return 1;
  }


  // Call a single function to configure all the register settings for the ISP and CCDC needed
  fsr_set_registers();


  // Map the allocated buffers to user space 
  for (numCapBufs = 0; numCapBufs < req.count; numCapBufs++) 
  {
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = numCapBufs;


    if (ioctl(captureFd, VIDIOC_QUERYBUF, &buf) == -1) {
      printf("FSR: Failed VIDIOC_QUERYBUF on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }

    m_CapBufs[numCapBufs].length = buf.length;
    m_CapBufs[numCapBufs].offset = buf.m.offset;
    m_CapBufs[numCapBufs].start  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, captureFd, buf.m.offset);

    if (m_CapBufs[numCapBufs].start == MAP_FAILED) {
      printf("FSR: Failed to mmap buffer on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }

    printf("FSR: Capture driver buffer %d at physical address %#lx mapped to "
                  "virtual address %#lx. with length %d\n", 
                  numCapBufs, 
                  (unsigned long) m_CapBufs[numCapBufs].start, m_CapBufs[numCapBufs].offset, m_CapBufs[numCapBufs].length);

    if (ioctl(captureFd, VIDIOC_QBUF, &buf) == -1) 
    {
      printf("FSR: VIODIOC_QBUF failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }
  }



  // Start the video streaming 
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (ioctl(captureFd, VIDIOC_STREAMON, &type) == -1) 
  {
    printf("FSR: VIDIOC_STREAMON failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }

  printf("FSR: Capture device initialized, begin video streaming. \n");

  return 0;
}


int fsr_buffer_get(void** buffer)
{
  if (-1 == fsr_dqbuf())
  {
    return -1;
  }
  *buffer = m_CapBufs[m_V4l2Buf.index].start;
  return 0;
}


/**
*  
*  @brief Releases and puts back into the queue the current buffer
*
*/
void fsr_buffer_release()
{
  int result;

  // Issue captured frame buffer back to device driver
  result = ioctl(captureFd, VIDIOC_QBUF, &m_V4l2Buf);

  if (-1 == result)
  {
    printf("FSR: VIDIOC_QBUF failed (%s)\n", strerror(errno));
  }
}


/**
*  
*  @brief Captures a single block of memory defined by D1_WIDTH and D1_HEIGHT
*       NOTE: This call will block until data is received!
*
*  @return Returns 0 on success
*/
int fsr_dqbuf()
{
  int capture_status = -1;

  // Get a frame buffer with captured data
  CLEAR(m_V4l2Buf);
  m_V4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  m_V4l2Buf.memory = V4L2_MEMORY_MMAP;

  while(1)
  {
    capture_status = ioctl(captureFd, VIDIOC_DQBUF, &m_V4l2Buf);
    
    if (-1 != capture_status) { break; }
    if (EAGAIN == errno) { continue; }

    // perror("Unexpected FSR Error");
    printf("FSR: Failed capturing buffer (%s)\n", strerror(errno));
    return 1;
    // exit(EXIT_FAILURE);
  }

  return 0;
}


/**
*  
*  @brief Releases all memory and files associated with the Init routine
*
*/
void fsr_cleanup()
{
  enum v4l2_buf_type type;
  unsigned int       i;

  if(captureFd == 0) return;

  /* Shut off the video capture */
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(captureFd, VIDIOC_STREAMOFF, &type) == -1) {
    printf("FSR: VIDIOC_STREAMOFF failed (%s)\n", strerror(errno));
  }

  if (-1 == close(captureFd)) {
    printf("FSR: Failed to close capture device (%s)\n", strerror(errno));
  }

  if(m_CapBufs == NULL) return;

  for (i = 0; i < NUM_CAPTURE_BUFS; i++) {
    if (munmap(m_CapBufs[i].start, m_CapBufs[i].length) == -1) {
      printf("FSR: Failed to unmap capture buffer %d\n", i);
    }
  }

  free(m_CapBufs);

  printf("FSR: Capture device closed and %d capture buffers freed. \n", NUM_CAPTURE_BUFS);
}
