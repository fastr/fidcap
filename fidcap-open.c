#include <stdlib.h>
#include <fcntl.h>

#define V4L2_DEVICE     "/dev/video0"

static int fd;

int main(int argc, char** argv) {
  open(V4L2_DEVICE, O_RDWR | O_NONBLOCK, 0);

  while(1) {
    sleep(10);
  }

  exit(0);
}
