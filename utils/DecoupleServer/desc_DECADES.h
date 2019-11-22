#pragma once

#include "omp.h"
#include "stdatomic.h"

__attribute__((noinline))
extern "C"
int desc_DECADES_COMPARE_EXCHANGE_STRONG(atomic_int* addr, int *expected, int desired) {
  return atomic_compare_exchange_strong_explicit(addr, expected, desired, memory_order_relaxed, memory_order_relaxed);
}

__attribute__((noinline))
extern "C"
int desc_DECADES_COMPARE_EXCHANGE_WEAK(atomic_int* addr, int *expected, int desired) {
  return atomic_compare_exchange_weak_explicit(addr, expected, desired, memory_order_relaxed, memory_order_relaxed);
}

__attribute__((noinline))
extern "C"
int desc_DECADES_COMPARE_AND_SWAP(volatile int* addr, int to_compare, int new_val) {
  int ret = *addr;
  desc_DECADES_COMPARE_EXCHANGE_STRONG((atomic_int *) addr, &to_compare, new_val);
  return ret;
}

__attribute__((noinline))
extern "C"
int desc_DECADES_FETCH_ADD(volatile int* addr, int to_add) {
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
int desc_DECADES_FETCH_MIN(volatile int* addr, int to_min) {
  int ret;
  int value = *addr;
  while (to_min < value) {
    if (desc_DECADES_COMPARE_EXCHANGE_WEAK((atomic_int *) addr, &value, to_min)) {
	return 1;
      }
  }
  return 0;
}

__attribute__((noinline))
extern "C"
float desc_DECADES_FETCH_ADD_FLOAT(volatile float* addr, float to_add) {
  union {
    int int_val;
    float float_val;
  } value, ret;

  do {
    value.float_val = *addr;
    ret.float_val = value.float_val + to_add;
  } while (!desc_DECADES_COMPARE_EXCHANGE_WEAK((atomic_int *) addr, &value.int_val, ret.int_val));

  return value.float_val;
}
