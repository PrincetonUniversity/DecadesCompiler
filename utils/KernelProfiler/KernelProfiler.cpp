#include "stdio.h"
#include "time.h"

int count = 0;
clock_t begin;

extern "C"
void decades_kp_start() {
  printf("\n--\n");
  printf("DECADES kernel instrumentation:\nstarting kernel execution\n");
  printf("kernel called %d times\n", count);
  count++;
  begin = clock();
}

extern "C"
void decades_kp_end() {
  printf("\nKernel execution finished\n");
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  printf("executing time (s): %2.2f\n", time_spent);
  printf("\n--\n");
}
