#pragma once

#include "omp.h"
#include "stdatomic.h"

#define DECADES_TILE_ID 0
#define DECADES_NUM_TILES 1

__attribute__((noinline))
extern "C"
void DECADES_BARRIER() {
#pragma omp barrier
}

__attribute__((noinline))
extern "C"
int DECADES_COMPARE_EXCHANGE_STRONG(atomic_int* addr, int *expected, int desired) {
  return atomic_compare_exchange_strong_explicit(addr, expected, desired, memory_order_relaxed, memory_order_relaxed);
}

__attribute__((noinline))
extern "C"
int DECADES_COMPARE_EXCHANGE_WEAK(atomic_int* addr, int *expected, int desired) {
  return atomic_compare_exchange_weak_explicit(addr, expected, desired, memory_order_relaxed, memory_order_relaxed);
}

__attribute__((noinline))
extern "C"
int DECADES_COMPARE_AND_SWAP(volatile int* addr, int to_compare, int new_val) {
  DECADES_COMPARE_EXCHANGE_STRONG((atomic_int *) addr, &to_compare, new_val);
  return to_compare;
}

__attribute__((noinline))
extern "C"
int DECADES_FETCH_ADD(volatile int* addr, int to_add) {
  int ret;
  #pragma omp atomic capture
  {
    ret = addr[0];
    addr[0] += to_add;
  }   
  return ret;
}

__attribute__((noinline))
extern "C"
int DECADES_FETCH_ADD_BOUNDED(volatile unsigned char * addr, int bound, int to_add) {
  unsigned char ret;
  #pragma omp critical
  {
    ret = addr[0];
    if (ret < bound)
      addr[0] += to_add;
  }   
  return ret;
}

__attribute__((noinline))
extern "C"
int DECADES_FETCH_MIN(volatile int* addr, int to_min) {
  int ret;
  int value = *addr;
  while (to_min < value) {
    if (DECADES_COMPARE_EXCHANGE_WEAK((atomic_int *) addr, &value, to_min)) {
	return 1;
      }
  }
  return 0;
}
extern "C"
int wrapper_FETCH_ADD_MAX(volatile int* addr, int bound) {
  int ret;
  int value = *addr;
  int desired = value + 1;
  int new_value;
  while (value < bound) {
    int new_value = DECADES_COMPARE_AND_SWAP(addr, value, desired);
    if (value == new_value)
	    return 1;
    else value = new_value;
    desired = value + 1;
  }
  return 0;
}

__attribute__((noinline))
extern "C"
float DECADES_FETCH_ADD_FLOAT(volatile float* addr, float to_add) {
  union {
    int int_val;
    float float_val;
  } value, ret;

  do {
    value.float_val = *addr;
    ret.float_val = value.float_val + to_add;
  } while (!DECADES_COMPARE_EXCHANGE_WEAK((atomic_int *) addr, &value.int_val, ret.int_val));

  return value.float_val;
}
