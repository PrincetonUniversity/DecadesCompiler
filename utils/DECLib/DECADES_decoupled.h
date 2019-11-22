#pragma once

#include "DECADES.h"

__attribute__((noinline))
extern "C"
void compute_exclusive_store(int *addr, int value) {
  *addr = value;
}

__attribute__((noinline))
extern "C"
int compute_exclusive_fetch_add(int *addr, int value) {
  int ret;
  #pragma omp atomic capture
  {
    ret = addr[0];
    addr[0] += value;
  }   
  return ret;
}

__attribute__((noinline))
extern "C"
void compute_side_store(int *addr, int value) {
  *addr = value;
}

__attribute__((noinline))
extern "C"
int compute_side_fetch_min(int *addr, int value) {
  return DECADES_FETCH_MIN(addr, value);
}


