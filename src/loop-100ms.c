#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

double militime() {
  double t_result;
  struct timeval t_now;

  gettimeofday(&t_now, NULL);
  t_result = (double)(t_now.tv_sec) +  ((1.e-6) * (double)t_now.tv_usec);

  return t_result;
}

int main(int argc, char* argv[])
{
  double t_start;
  double t_end;
  long int count = 0;

  while(1)
  {
    t_start = militime();
    usleep(100000);
    t_end = militime();
    printf("Slept for %lf s (%ld)\n", t_end - t_start, count);
    count += 1;
  }

  return 0;
}
