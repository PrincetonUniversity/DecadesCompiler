#pragma once
#include <stdint.h>
#include "dec_atomics.h"

__attribute__((noinline))
extern "C"
int compute_exclusive_load(int *addr) {
  int value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
int compute_exclusive_load_uint8(char *addr) {
  int value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
float compute_exclusive_load_float(float *addr) {
  float value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
double compute_exclusive_load_double(double *addr) {
  double value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
long compute_exclusive_load_long(long *addr) {
  long value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
uint64_t compute_exclusive_load_uint64t(uint64_t *addr) {
  uint64_t value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
uint32_t compute_exclusive_load_uint32t(uint32_t *addr) {
  uint32_t value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
void compute_exclusive_store(int *addr, int value) {
  *addr = value;
}


__attribute__((noinline))
extern "C"
void compute_exclusive_store_ptr(int **addr, int *value) {
  *addr = value;
}

__attribute__((noinline))
extern "C"
void compute_exclusive_store_uint8(char *addr,char value) {
  *addr = value;
}

__attribute__((noinline))
extern "C"
void compute_exclusive_store_float(float *addr, float value) {
  *addr = value;
}

__attribute__((noinline))
extern "C"
void compute_exclusive_store_double(double *addr, double value) {
  *addr = value;
}

__attribute__((noinline))
extern "C"
void compute_exclusive_store_long(long *addr, long value) {
  *addr = value;
}

__attribute__((noinline))
extern "C"
void compute_exclusive_store_uint64t(uint64_t *addr, uint64_t value) {
  *addr = value;
}


__attribute__((noinline))
extern "C"
void compute_exclusive_store_uint32t(uint32_t *addr, uint32_t value) {
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
int compute_exclusive_fetch_min(int *addr, int value) {
  return dec_atomic_fetch_min((uint32_t *) addr, (uint32_t) value);
}


__attribute__((noinline))
extern "C"
int compute_exclusive_compare_exchange(int *addr, int val1, int val2) {
  return dec_atomic_compare_exchange((uint32_t *) addr, (uint32_t) val1, (uint32_t) val2);
}


__attribute__((noinline))
extern "C"
void compute_side_store(int *addr, int value) {
  *addr = value;
}

//__attribute__((noinline))
//extern "C"
//int compute_side_fetch_min(int *addr, int value) {
//  return DECADES_FETCH_MIN(addr, value);/
//}


__attribute__((noinline))
extern "C"
float supply_exclusive_load_float(float *addr) {
  float value = *addr;
  return value;
}

__attribute__((noinline))
extern "C"
void supply_exclusive_store_float(float *addr, float value) {
  *addr = value;
}
