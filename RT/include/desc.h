#include "assert.h"
#include "omp.h"


#define DESC_DEBUG 1

int desc_counter = 0;

void my_print(const char *s) {
  if (DESC_DEBUG) {
    int ret;
#pragma omp atomic capture
    {
      ret = desc_counter;
      desc_counter += 1;
    }   

    printf("%d: desc debug: %s\n", ret, s);
  }
}

void my_print_label(const char *s, int label) {
  if (DESC_DEBUG) {
    int ret;
#pragma omp atomic capture
    {
      ret = desc_counter;
      desc_counter += 1;
    }   

    printf("%d %d: desc debug: %s\n", label, ret, s);
  }
}
