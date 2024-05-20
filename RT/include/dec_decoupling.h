#pragma once
#include <stdint.h>
#include "omp.h"
//#include "stdatomic.h"
using namespace std;

// Regular RMW operations. These operations should just map to regular RMWs, e.g. as supported by C++
// Keep in mind that decoupling does NOT support synchronization through RMWs. Thus, all memory orders
// are relaxed.

//__attribute__((noinline))
//extern "C"
//int dec_atomic_fetch_add(uint32_t *addr, uint32_t val) {
//  return atomic_fetch_add_explicit((atomic_int*) addr, (int) val, memory_order_relaxed);
//}

__attribute__((noinline))
extern "C"
int dec_atomic_fetch_add(uint32_t *addr, uint32_t val);



//__attribute__((noinline))
//extern "C"
//int dec_atomic_compare_exchange(uint32_t *addr, uint32_t to_compare, uint32_t new_val) {
//  return atomic_compare_exchange_strong_explicit((atomic_int*) addr,(int*) &to_compare, (int) new_val, memory_order_relaxed,// memory_order_relaxed);
//}

__attribute__((noinline))
extern "C"
int dec_atomic_compare_exchange(uint32_t *addr, uint32_t to_compare, uint32_t new_val);

// These atomics need special implementations.
// You can implement any 32-bit operation using Compare and swap.
// You may want to do this for floats, see the fetch_add_float on how to do this.
__attribute__((noinline))
extern "C"
int dec_atomic_fetch_min(uint32_t *addr, uint32_t val);

__attribute__((noinline))
extern "C"
float dec_atomic_fetch_add_float(float* addr, float to_add);

// Init and End decoupling
extern "C"
uint32_t dec_open_producer(uint64_t qid);
extern "C"
uint32_t dec_open_consumer(uint64_t qid);
extern "C"
uint32_t dec_close_producer(uint64_t qid);
extern "C"
uint32_t dec_close_consumer(uint64_t qid);


// FIFO Creation and destruction
// Should probably be an enum

#define DCP_SIZE_8   0
#define DCP_SIZE_16  1
#define DCP_SIZE_32  2
#define DCP_SIZE_48  3
#define DCP_SIZE_64  4
#define DCP_SIZE_80  5
#define DCP_SIZE_96  6
#define DCP_SIZE_128 7



extern "C"
uint32_t dec_fifo_init(uint32_t count, uint32_t size);


extern "C"
uint32_t dec_fifo_cleanup();


// TODO stats needs some enum or defined list
extern "C"
uint32_t dec_fifo_stats(uint64_t qid, uint32_t stat, uint32_t* result);

extern "C"
uint32_t dec_peek(uint64_t qid);

extern "C"
bool dec_isEmptyQ(uint64_t qid);

extern "C"
bool dec_isFullQ(uint64_t qid);

extern "C"
uint32_t dec_queueCapacity(uint64_t qid);

// Produce/Consume
extern "C"
void dec_produce32(uint64_t qid, uint32_t data);
extern "C"
void dec_produce64(uint64_t qid, uint64_t data);

extern "C"
uint32_t dec_consume32(uint64_t qid);
extern "C"
uint64_t dec_consume64(uint64_t qid);


// Async Loads
extern "C"
void dec_load32_async(uint64_t qid, uint32_t *addr);
extern "C"
void dec_load64_async(uint64_t qid, uint64_t *addr);

// Async RMWs
/*extern "C"
  void dec_atomic_fetch_add_async(uint64_t qid, uint32_t *addr, uint32_t val);*/
extern "C"
void dec_atomic_fetch_add_async(uint32_t *addr, uint32_t val);
extern "C"
void dec_atomic_fetch_and_async(uint64_t qid, uint32_t *addr, uint32_t val);
extern "C"
void dec_atomic_fetch_or_async(uint64_t  qid, uint32_t *addr, uint32_t val);
extern "C"
void dec_atomic_fetch_xor_async(uint64_t qid, uint32_t *addr, uint32_t val);
extern "C"
void dec_atomic_fetch_max_async(uint64_t qid, uint32_t *addr, uint32_t val);
/*extern "C"
void dec_atomic_fetch_min_async(uint64_t qid, uint32_t *addr, uint32_t val);*/
extern "C"
void dec_atomic_fetch_min_async(uint32_t *addr, uint32_t val);
extern "C"
void dec_atomic_exchange_async(uint64_t  qid, uint32_t *addr, uint32_t val);
extern "C"
void dec_atomic_compare_exchange_async(uint64_t qid, uint32_t *addr, uint32_t to_compare, uint32_t new_val);
extern "C"
void * dec_malloc(unsigned long size);

