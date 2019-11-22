#include "omp.h"

extern "C"
void DECADES_BARRIER() {
  #pragma omp barrier
}
