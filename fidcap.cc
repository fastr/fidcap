#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h> // used for printf()
#include <fstream> // used for close()
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <linux/videodev2.h>
//#include <arpa/inet.h>

/* Device parameters */
#define V4L2_PIX_FMT_FSR172X  v4l2_fourcc('F', 'S', 'R', '0') /* 12bit raw fsr172x */
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

#include "configure_registers.c"

//! Describes a capture frame buffer
struct sCaptureBuffer {
  void         *start;        //!< Starting memory location of buffer
  unsigned long offset;        //!< Offset into buffer to first valid data
  size_t        length;        //!< Length of data
};

//! The FSR class
class FSR
{
public:
  FSR();
  ~FSR();

  int Init();
  int CaptureBlock();
  void ReleaseBlock();
  void Stop();

  //! Returns the most recently capture memory pointer
  void * GetDataPtr(){ return m_CapBufs[m_V4l2Buf.index].start; }

private:
  int                         m_FD;    //!< The FSR file descriptor
  sCaptureBuffer              *m_CapBufs; //!< Holds the capture buffer memory maps to the CCDC memory regions
    struct v4l2_buffer      m_V4l2Buf;  //!< Contains returned data from a capture frame indicating which memory is aquired

  double            m_FSRSensorStartTime; //!< Contains the last valid fsr_sensor start time (-1 if no valid start time)
};

/**
*  
*  @brief Standard constructor
*
*/
FSR::FSR()
{
  m_FD = 0;
  m_CapBufs = NULL;
  m_FSRSensorStartTime = -1;
}


/**
*  
*  @brief Standard deconstructor
*
*/
FSR::~FSR()
{
  if(m_FD)
  {
    Stop();
  }
}


/**
*  
*  @brief Initializes and configures the CCD controller for capture
*
*  @return Returns 0 on success
*/


int FSR::Init()
{
  struct v4l2_requestbuffers  req;
  struct v4l2_capability      cap;
  //struct v4l2_cropcap         cropCap;
  struct v4l2_format          fmt;
  struct v4l2_buffer          buf;
  enum v4l2_buf_type          type;
  v4l2_std_id                 std;  
  unsigned int                numCapBufs;
  
  unsigned char         value[4];

  printf("FSR: Initializing CCD controller.\n");

  /* Open video capture device */
  m_FD = open(V4L2_DEVICE, O_RDWR | O_NONBLOCK, 0);

  if (m_FD == -1) {
    printf("FSR: Cannot open.\n");
    return 1;
  }


  printf("FSR: Capture device opened. CaptureWidth = %d, CaptureHeight = %d\n", D1_WIDTH, D1_HEIGHT);

  // Query for capture device capabilities 
  if (ioctl(m_FD, VIDIOC_QUERYCAP, &cap) == -1) {
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
  if (ioctl(m_FD, VIDIOC_S_FMT, &fmt) == -1) {
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
  if (ioctl(m_FD, VIDIOC_REQBUFS, &req) == -1) {
    printf("FSR: VIDIOC_REQBUFS failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }

  printf("FSR: %d capture buffers were successfully allocated.\n", req.count);

  if (req.count < NUM_CAPTURE_BUFS) {
    printf("FSR: Insufficient buffer memory on %s\n", V4L2_DEVICE);
    return 1;
  }

  m_CapBufs = (sCaptureBuffer *)calloc(req.count, sizeof(*m_CapBufs));

  if (!m_CapBufs) {
    printf("FSR: Failed to allocate memory for capture buffer structs.\n");
    return 1;
  }


  // Call a single function to configure all the register settings for the ISP and CCDC needed
  ConfigureAllRegisters();
  


  // Map the allocated buffers to user space 
  for (numCapBufs = 0; numCapBufs < req.count; numCapBufs++) 
  {
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = numCapBufs;


    if (ioctl(m_FD, VIDIOC_QUERYBUF, &buf) == -1) {
      printf("FSR: Failed VIDIOC_QUERYBUF on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }

    m_CapBufs[numCapBufs].length = buf.length;
    m_CapBufs[numCapBufs].offset = buf.m.offset;
    m_CapBufs[numCapBufs].start  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_FD, buf.m.offset);

    if (m_CapBufs[numCapBufs].start == MAP_FAILED) {
      printf("FSR: Failed to mmap buffer on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }

    printf("FSR: Capture driver buffer %d at physical address %#lx mapped to "
                  "virtual address %#lx. with length %d\n", 
                  numCapBufs, 
                  (unsigned long) m_CapBufs[numCapBufs].start, m_CapBufs[numCapBufs].offset, m_CapBufs[numCapBufs].length);

    if (ioctl(m_FD, VIDIOC_QBUF, &buf) == -1) 
    {
      printf("FSR: VIODIOC_QBUF failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
      return 1;
    }
  }



  // Start the video streaming 
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (ioctl(m_FD, VIDIOC_STREAMON, &type) == -1) 
  {
    printf("FSR: VIDIOC_STREAMON failed on %s (%s)\n", V4L2_DEVICE, strerror(errno));
    return 1;
  }

  printf("FSR: Capture device initialized, begin video streaming. \n");

  return 0;
}


/**
*  
*  @brief Releases all memory and files associated with the Init routine
*
*/
void FSR::Stop()
{
  enum v4l2_buf_type type;
  unsigned int       i;

  if(m_FD == 0) return;

  /* Shut off the video capture */
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(m_FD, VIDIOC_STREAMOFF, &type) == -1) {
    printf("FSR: VIDIOC_STREAMOFF failed (%s)\n", strerror(errno));
  }

  if (close(m_FD) == -1) {
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



/**
*  
*  @brief Captures a single block of memory defined by D1_WIDTH and D1_HEIGHT
*       NOTE: This call will block until data is received!
*
*  @return Returns 0 on success
*/
int FSR::CaptureBlock()
{
  // Reset last valid start time
  m_FSRSensorStartTime = -1;

  // Get a frame buffer with captured data
  CLEAR(m_V4l2Buf);
  m_V4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  m_V4l2Buf.memory = V4L2_MEMORY_MMAP;

  int result;
  do
  {
    result = ioctl(m_FD, VIDIOC_DQBUF, &m_V4l2Buf);
  }
  while(result == -1 && (errno == EAGAIN));

  // We still had an error
  if(result == -1) {printf("FSR: Failed capturing buffer (%s)\n", strerror(errno)); return 1; }

  return 0;
}


/**
*  
*  @brief Releases and puts back into the queue the current buffer
*
*/
void FSR::ReleaseBlock()
{
  // Issue captured frame buffer back to device driver
  if (ioctl(m_FD, VIDIOC_QBUF, &m_V4l2Buf) == -1) 
  {
    printf("FSR: VIDIOC_QBUF failed (%s)\n", strerror(errno));
  }
}


// Simple test file for FSR
double microtime()
{
        static double InitTime = 0;
        struct timeval tp;

        gettimeofday(&tp, NULL);
        if(InitTime == 0)
                InitTime = ((double)tp.tv_sec+(1.e-6)*tp.tv_usec);

        return ((double)tp.tv_sec+(1.e-6)*tp.tv_usec) - InitTime;
}

int main(int argc, char *argv[])
{
  FSR fsr;
  double t_latest = microtime();
  int j;
  
  if(fsr.Init())
  {
    printf("Error initing FSR\n");
    return 1;
  }
  printf("FSR: Init complete.\n");
  
  // Capture `i` blocks of data
  //system("node ccdc-reg.js > before_capture.txt");
  for(int i = 0; i < 500; i++)
  {
    printf("FSR: Stream begin.\n");
    if(0 != fsr.CaptureBlock())
    { 
      printf("Error capturing.\n");
      break;
    }
    printf("FSR: Captured.\n");
    
    //system("node ccdc-reg.js > during_capture.txt");
    printf("Got a block of data at %p Elapsed Time = %lf\n",fsr.GetDataPtr(),microtime() - t_latest);
    t_latest = microtime();
    
    // Now search through the first 10,000 values looking for a start of an fsr block. 
    // We should find it every 2048 samples since fsr runs at 1X rates
    unsigned short *DataValues = (unsigned short *)fsr.GetDataPtr();
    for(j=0; j<10000; j++)
    {
      if(DataValues[j] & 0x8000) break;
    }
    
//    if(j == 10000) printf("Did not find a start of chirp\n");
//    else printf("Found start of chirp at %d\n",j);
    
    // print out the first 10 sample values
    for(j=0; j<2048000; j++)
    {
      if(DataValues[j] != 0) break;
    }
      
    if(j != 2048000)
      printf("Found a non-zero value at (%d) value = 0x%04X\n", j, DataValues[j]);    
    else
      printf("Warning: All data is zeros\n");

    fsr.ReleaseBlock();
  }
  //system("node ccdc-reg.js > after_capture.txt");
  
  

  printf("All done...exiting.\n");

  //while(1) {
  //  sleep(10);
  //}
  
  return 0;

}
