#pragma once

#include <omp.h>
#include <stdatomic.h>

extern atomic_int fences[10000];

__attribute__((noinline))
extern "C"
void DECADES_BARRIER() {
#pragma omp barrier
}

__attribute__((noinline))
extern "C"
void DECADES_PARTIAL_BARRIER(int fence_id, int nb_threads) {
  atomic_int *fence = &fences[fence_id];
  int current = atomic_fetch_add(fence, 1);

  while(atomic_load(fence)<(nb_threads))
    if (atomic_load(fence) == 0)
      break;

  atomic_store(fence, 0);
}
