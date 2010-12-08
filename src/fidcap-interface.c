#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // close
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

/* Device parameters */
#define V4L2_DEVICE     "/dev/video2"

/* Macro for clearing structures */
#define CLEAR(x) memset (&(x), 0, sizeof (x))

// Configures the height and width of the frame.
// NOTE: Changing values here alone will not help, you also 
// need to change values in the driver since its generated VD and HD signals
#define V4L_BUFFERS_DEFAULT 8

#include "fsr172x.h"
#include "fidcap.h"
int fsr_dqbuf();

//! Describes a capture frame buffer
struct fsr_v4l2_buffer {
  void* start;          //!< Starting memory location of buffer
  unsigned long offset; //!< Offset into buffer to first valid data
  size_t length;        //!< Length of data
};

int fsr_fd = 0;
struct fsr_v4l2_buffer* fsr_v4l2_buf; //!< Holds the capture buffer memory maps to the CCDC memory regions
struct v4l2_buffer v4l2_buf;     //!< Contains returned data from a capture frame indicating which memory is aquired
unsigned char raw_data_buffer[FSR_DATA_SIZE];
enum v4l2_memory memtype = V4L2_MEMORY_MMAP;
enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;


/**
*  
*  @brief Initializes and configures the CCD controller for capture
*
*  @return Returns 0 on success
*/
int fsr_init()
{
  struct v4l2_requestbuffers  rb;
  struct v4l2_capability      cap;
  struct v4l2_format          fmt;
  struct v4l2_buffer          buf;

  unsigned int                fsr_num_bufs;

  CLEAR(cap);

  fsr_v4l2_buf = NULL;
  
  printf("FSR: Initializing CCD controller.\n");

  /* Open video capture device */
  fsr_fd = open(V4L2_DEVICE, O_RDWR | O_NONBLOCK);

  if (fsr_fd < -1) {
    printf("FSR: Cannot open.\n");
    return 1;
  }


  printf("FSR: Capture device opened.\n");

  // Query for capture device capabilities 
  if (ioctl(fsr_fd, VIDIOC_QUERYCAP, &cap) == -1) {
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
  fmt.type                = type;
  fmt.fmt.pix.width       = 2048;
  fmt.fmt.pix.height      = 128;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SGRBG12;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;

  // Set the video capture format 
  if (ioctl(fsr_fd, VIDIOC_S_FMT, &fmt) == -1) {
    printf("FSR: VIDIOC_S_FMT failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }

  printf("FSR: Video capture format is set. \n");


  //  VIDIOC_S_CROP is not required since we are not using the smoothing function 

  CLEAR(rb);
  rb.count  = V4L_BUFFERS_DEFAULT;
  rb.type   = type;
  rb.memory = memtype;

  // Allocate buffers in the capture device driver 
  if (ioctl(fsr_fd, VIDIOC_REQBUFS, &rb) == -1) {
    printf("FSR: VIDIOC_REQBUFS failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }

  printf("FSR: %d capture buffers were successfully allocated.\n", rb.count);

  if (rb.count < V4L_BUFFERS_DEFAULT) {
    printf("FSR: Insufficient buffer memory on %s\n", V4L2_DEVICE);
    return 1;
  }

  fsr_v4l2_buf = (struct fsr_v4l2_buffer *)calloc(rb.count, sizeof(*fsr_v4l2_buf));

  if (!fsr_v4l2_buf) {
    printf("FSR: Failed to allocate memory for capture buffer structs.\n");
    return 1;
  }


  // Map the allocated buffers to user space 
  for (fsr_num_bufs = 0; fsr_num_bufs < rb.count; fsr_num_bufs++) 
  {
    CLEAR(buf);
    buf.type = type;
    buf.memory = memtype;
    buf.index = fsr_num_bufs;


    if (ioctl(fsr_fd, VIDIOC_QUERYBUF, &buf) == -1) {
      printf("FSR: Failed VIDIOC_QUERYBUF on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }

    fsr_v4l2_buf[fsr_num_bufs].length = buf.length;
    fsr_v4l2_buf[fsr_num_bufs].offset = buf.m.offset;
    fsr_v4l2_buf[fsr_num_bufs].start  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fsr_fd, buf.m.offset);

    if (fsr_v4l2_buf[fsr_num_bufs].start == MAP_FAILED) {
      printf("FSR: Failed to mmap buffer on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }

    printf("FSR: Capture driver buffer %d at physical address %#lx mapped to "
                  "virtual address %#lx. with length %d\n", 
                  fsr_num_bufs, 
                  (unsigned long) fsr_v4l2_buf[fsr_num_bufs].start, fsr_v4l2_buf[fsr_num_bufs].offset, fsr_v4l2_buf[fsr_num_bufs].length);

    if (ioctl(fsr_fd, VIDIOC_QBUF, &buf) == -1) 
    {
      printf("FSR: VIODIOC_QBUF failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }
  }



  // Start the video streaming 
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (ioctl(fsr_fd, VIDIOC_STREAMON, &type) == -1) 
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
  memcpy((void*) raw_data_buffer, fsr_v4l2_buf[v4l2_buf.index].start, FSR_DATA_SIZE);
  *buffer = raw_data_buffer;
  //*buffer = fsr_v4l2_buf[v4l2_buf.index].start;
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
  result = ioctl(fsr_fd, VIDIOC_QBUF, &v4l2_buf);

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
  CLEAR(v4l2_buf);
  v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  v4l2_buf.memory = memtype;

  while(1)
  {
    capture_status = ioctl(fsr_fd, VIDIOC_DQBUF, &v4l2_buf);
    
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
  unsigned int       i;

  if(fsr_fd == 0) return;

  /* Shut off the video capture */
  if (ioctl(fsr_fd, VIDIOC_STREAMOFF, &type) == -1) {
    printf("FSR: VIDIOC_STREAMOFF failed (%s)\n", strerror(errno));
  }

  if (-1 == close(fsr_fd)) {
    printf("FSR: Failed to close capture device (%s)\n", strerror(errno));
  }

  if(fsr_v4l2_buf == NULL) return;

  for (i = 0; i < V4L_BUFFERS_DEFAULT; i++) {
    if (munmap(fsr_v4l2_buf[i].start, fsr_v4l2_buf[i].length) == -1) {
      printf("FSR: Failed to unmap capture buffer %d\n", i);
    }
  }

  free(fsr_v4l2_buf);

  printf("FSR: Capture device closed and %d capture buffers freed. \n", V4L_BUFFERS_DEFAULT);
}
