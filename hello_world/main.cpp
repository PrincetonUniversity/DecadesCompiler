// Just run DEC++ main.cpp

#include "DECADES/DECADES.h"
#include <stdio.h>

#define SIZE 256

void __attribute__((noinline)) _kernel_(float *a, float *b, float *c, int tid, int num_threads) {
  for (int i = tid; i < SIZE; i+=num_threads) {
    c[i] += a[i] + b[i];
  }
}

int main() {
  float a[SIZE], b[SIZE], c[SIZE];

  for (int i = 0; i < SIZE; i++) {
    a[i] = b[i] = i;
    c[i] = 0.0;
  }

  _kernel_(a, b, c, 0, 1);

  printf("finished!\n");
  for (int i = 1; i < SIZE; i++) {
    c[0] += c[i];
  }

  printf("reference result: %f\n", c[0]);
  return 0;
}
