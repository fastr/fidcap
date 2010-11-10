#include <stdlib.h> // NULL
#include <stdio.h> // printf
#include <sys/time.h> // gettimeofday

#include "fidcap.h"

double microtime()
{
  static double InitTime = 0;
  struct timeval tp;

  gettimeofday(&tp, NULL);
  if(InitTime == 0)
  {
    InitTime = ((double)tp.tv_sec+(1.e-6)*tp.tv_usec);
  }

  return ((double)tp.tv_sec+(1.e-6)*tp.tv_usec) - InitTime;
}

int main(int argc, char *argv[])
{
  double t_latest = microtime();
  int i, j;
  
  if(fsr_init())
  {
    printf("Error initing FSR\n");
    return 1;
  }
  printf("FSR: Init complete.\n");
  
  // Capture `i` blocks of data
  //system("node ccdc-reg.js > before_capture.txt");
  for(i = 0; i < 500; i++)
  {
    //char* fsr_raw[FSR_DATA_SIZE];
    char* fsr_raw;
    //char* fsr_raw_p = 
    fsr_buffer_get(&fsr_raw);

    printf("Got a block of data at %p Elapsed Time = %lf\n",fsr_raw, microtime() - t_latest);
    t_latest = microtime();
    
    // A new FSR block starts every 2048 samples
    // Now search through the first 10,000 values looking for just one.
    unsigned short *fsr_values = (unsigned short *)fsr_raw;
    for(j=0; j<10000; j++)
    {
      if(fsr_values[j] & 0x8000) break;
    }
    
    for(j=0; j<2048000; j++)
    {
      if(fsr_values[j] != 0) break;
    }
      
    if(j != 2048000)
    {
      printf("Found a non-zero value at (%d) value = 0x%04X\n", j, fsr_values[j]);    
    }
    else
    {
      printf("Warning: All data is zeros\n");
    }
    fsr_buffer_release();
  }
  //system("node ccdc-reg.js > after_capture.txt");
  
  printf("All done...exiting.\n");

  return 0;
}
