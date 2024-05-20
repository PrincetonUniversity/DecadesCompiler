#pragma once
#include "stdatomic.h"


// You can implement any 32-bit atomic operation using Compare and Swap.
// You may want to do this for floats, see the fetch_add_float on how to do this.
// That is, you need a union to proper bit casting.

// btw... why doesn't c++ support atomic_fetch_min?

__attribute__((noinline))
extern "C"
int dec_atomic_fetch_min(uint32_t *addr, uint32_t val) {
  int tmp;
  do {
    tmp = atomic_load_explicit((atomic_int*) addr, memory_order_relaxed);
    if (tmp < val) {
      return tmp;
    }
  } while (!atomic_compare_exchange_weak_explicit((atomic_int *)addr, &tmp, (int) val, memory_order_relaxed, memory_order_relaxed));
  
  return tmp;
}

__attribute__((noinline))
extern "C"
int dec_atomic_fetch_add(uint32_t *addr, uint32_t val) {
  return atomic_fetch_add_explicit((atomic_int*) addr, (int) val, memory_order_relaxed);
}


__attribute__((noinline))
extern "C"
int dec_atomic_compare_exchange(uint32_t *addr, uint32_t to_compare, uint32_t new_val) {
  return atomic_compare_exchange_strong_explicit((atomic_int*) addr,(int*) &to_compare, (int) new_val, memory_order_relaxed, memory_order_relaxed);
}



// Its nasty but see:
// http://suhorukov.blogspot.com/2011/12/opencl-11-atomic-operations-on-floating.html
__attribute__((noinline))
extern "C"
float dec_atomic_fetch_add_float(float *addr, float to_add) {
  union {
    int int_val;
    float float_val;
  } value, ret;
  
  do {
    value.float_val = *addr;
    ret.float_val = value.float_val + to_add;
  } while (!atomic_compare_exchange_weak_explicit((atomic_int*)addr, &value.int_val, ret.int_val, memory_order_relaxed, memory_order_relaxed));
  
  return value.float_val;
}

