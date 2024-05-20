#include "../include/dec_decoupling.h"
#include "../include/DECADES.h"
#include "math.h"
#include <stdlib.h>
#include "assert.h"
#include "omp.h"
#include <vector>
#include "stdatomic.h"
#include <iostream>
//#include "../include/dec_atomics.h"

using namespace std;

#define QUEUE_SIZE 128
#define MAX_QUEUES 256

#define DEC_DEBUG 1 // Do some runtime checks, but potentially slows
		    // things down a bit.

//extern int DEC_NUM_THREADS;
extern int DEC_NUM_CONSUMERS;
char DEC_DECOUPLING_MODE = 'c';

int DEC_NUM_CONSUMERS =1;


uint64_t get_producer_qid(int data = NAN){
  uint64_t qid;
  //printf("DEC_DECOUPLING_MODE:%c\n", DEC_DECOUPLING_MODE);
  switch(DEC_DECOUPLING_MODE) {
  //conjoint, asymmetric (variable ratio):
  case 'c':
    {
      qid =  (uint64_t) (data / DEC_NUM_CONSUMERS);
      break;
    }
  // desc (1:1): 
  case 'd' :
    {
      
      qid = (uint64_t) omp_get_thread_num() - DEC_NUM_CONSUMERS;
      break;
    }
  // graphattack (1:1):
  case 'g':
    {
      qid = (uint64_t) (omp_get_thread_num() - DEC_NUM_CONSUMERS);
      //printf("THREAD_NUM: %d\n",omp_get_thread_num());
      //printf("qid:%d\n", qid);
      //printf("DEC_NUM_CONSUMERS:%d\n", DEC_NUM_CONSUMERS);
      
      break;
    }
  // default (1:1):
  default :
    {
      qid = (uint64_t) (omp_get_thread_num() - DEC_NUM_CONSUMERS);
      break;
    }
  }
  //printf("in get_producer_qid, qid=%d\n", (int) qid);
  return qid;
}

// Circular buffer for queues
template <typename T>
struct T_queue {
  // max size just in case. A better implementation would dynamically
  // allocate this. But then you have to clean up
  T arr[QUEUE_SIZE]; 
  uint32_t virtual_size;
  volatile int head;
  volatile int tail;

  void set_size(uint32_t size) {
    assert(size <= QUEUE_SIZE);
    virtual_size = size;
  }

  void init(uint32_t size) {
    set_size(size);
    head = tail = 0;
  }

  bool is_full() {    
    return tail == ((head + 1) % virtual_size);
  }
  
  bool is_empty() {
    return (head == tail);
  }
  
  void enqueue(T v) {
    int index = head;
    //printf("Enqueueing at index %d, %d\n", index, v);
    //printf("Enqueueing at address: %p\n", (void*)&(arr[index]));
    arr[index] = v;
    head = ((index + 1) % virtual_size);
  }
  
  T dequeue() {
    int index = tail;
    //printf("Dequeueing is at address: %p\n", (void*)&(arr[index]));
    T ret = arr[index];
    //printf("Dequeueing at index %d, %d\n", index, ret);
    tail = ((index + 1) % virtual_size);
    return ret;
  }

  T peek() {
    int index = tail;
    T ret = arr[index];
    return ret;
  }

  T peek(uint32_t n) {
    int index =tail;
    T ret = arr[((index+n) % virtual_size)];
    return ret;
  }  
  void do_enqueue(T v) {
    int index = head;
    while (tail == ((head + 1) % virtual_size)) { };
    enqueue(v);
  }

  T do_dequeue() {
    int index = tail;
    while (head == index) {};
    T ret = dequeue();
    return ret;
  }  
};

// The actual queues
vector<T_queue<uint64_t> > DEC_QUEUES;
bool INITIALIZED = false;
atomic_int producer_ownership_map[MAX_QUEUES];
atomic_int consumer_ownership_map[MAX_QUEUES];

#define DEC_GET_TID omp_get_thread_num()

#define DEBUG_CHECK assert(INITIALIZED); \
  assert(qid < DEC_QUEUES.size());	 \

#define DEBUG_PRODUCER assert(DEC_GET_TID == producer_ownership_map[qid]);
#define DEBUG_CONSUMER assert(DEC_GET_TID == consumer_ownership_map[qid]);

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

// Init and End decoupling
extern "C"
uint32_t dec_open_producer(uint64_t qid) {
  
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
#endif
  int to_compare = -1;
  while(!atomic_compare_exchange_weak_explicit(&producer_ownership_map[qid], &to_compare, DEC_GET_TID, memory_order_relaxed, memory_order_relaxed)){
    to_compare = -1;
  }
  //printf("dec_open_producer, qid:%d\n", (int) qid); 
  return 1;
}

extern "C"
uint32_t dec_open_consumer(uint64_t qid) {
  
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
#endif
  int to_compare = -1;
  //printf("dec_open_consumer trying to open, qid:%d\n", (int) qid);
  while(!atomic_compare_exchange_weak_explicit(&consumer_ownership_map[qid], &to_compare, DEC_GET_TID, memory_order_relaxed, memory_order_relaxed)){
    to_compare = -1;
  }
  //printf("dec_open_consumer opened!!! qid:%d\n", (int) qid); 
  return 1;
}

extern "C"
uint32_t dec_close_producer(uint64_t qid) {
  //printf("dec_close_producer, qid:%d\n", (int) qid); 
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  atomic_store_explicit(&producer_ownership_map[qid], -1, memory_order_relaxed);
  return 1;
}

extern "C"
uint32_t dec_close_consumer(uint64_t qid) {
  //printf("dec_close_consumer, qid:%d\n", (int) qid); 
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_CONSUMER;
#endif
  atomic_store_explicit(&consumer_ownership_map[qid], -1, memory_order_relaxed);
  return 1;
}

extern "C"
uint32_t dec_fifo_init(uint32_t count, uint32_t size) {
  assert(size < 8);
  assert(count > 0);
  int size_table[8] = {8, 12, 32, 48, 64, 80, 96, 128}; 
  int actual_size = size_table[size];

  if (count > MAX_QUEUES) {
    count = MAX_QUEUES;
  }
  
  // Create Queues
  for (uint32_t i = 0; i < count; i++) {
    T_queue<uint64_t> new_q;
    new_q.init(actual_size);
    DEC_QUEUES.push_back(new_q);
    producer_ownership_map[i] = -1;
    consumer_ownership_map[i] = -1;
  }
  
  INITIALIZED = true;
  return count;
}

extern "C"
uint32_t dec_fifo_cleanup() {
  assert(INITIALIZED);
  INITIALIZED = false;
  DEC_QUEUES.clear();
  return 1;
}

// Leave unimplemented for now
extern "C"
uint32_t dec_fifo_stats(uint64_t qid, uint32_t stat, uint32_t* result) {
  // unimplemented
  assert(0);
}

// Check if a queue is empty from threads:
extern "C"
bool dec_isEmptyQ(uint64_t qid){
  return DEC_QUEUES[qid].is_empty();
}

// Check if a queue is empty from threads:
extern "C"
bool dec_isFullQ(uint64_t qid){
  return DEC_QUEUES[qid].is_full();
}

// Check if a queue is empty from threads:
extern "C"
uint32_t dec_queueCapacity(uint64_t qid){
  //cout << "DEC_QUEUES.size() = " << QUEUE_SIZE << "\n";
  //cout << "DEC_QUEUES.tail = " << DEC_QUEUES[qid].tail << "\n";
  //cout << "DEC_QUEUES.head = " << DEC_QUEUES[qid].head << "\n";
  //cout << "Queue Capacity: " << (DEC_QUEUES[qid].tail - DEC_QUEUES[qid].head +
  //		 QUEUE_SIZE-1) % QUEUE_SIZE << "\n";
  if (DEC_QUEUES[qid].head < DEC_QUEUES[qid].tail){
    return (DEC_QUEUES[qid].tail - DEC_QUEUES[qid].head - 1);
  }else
    return (QUEUE_SIZE - 1 -(DEC_QUEUES[qid].head - DEC_QUEUES[qid].tail));
    //(DEC_QUEUES[qid].tail - DEC_QUEUES[qid].head +
    //  (QUEUE_SIZE-1) % QUEUE_SIZE);
}

extern "C" 
uint32_t dec_peek(uint64_t qid){
  return DEC_QUEUES[qid].peek();
}


// Produce/Consume
extern "C"
void dec_produce32(uint64_t qid, uint32_t data) {
  //printf("producing to qid:%d   value:%d\n", (int) qid, (int)data); 
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  DEC_QUEUES[qid].do_enqueue(data);
}

extern "C"
void dec_produce64(uint64_t qid, uint64_t data) {
  //printf("producing to qid:%d   value:%d\n", (int) qid, (int)data); 
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  DEC_QUEUES[qid].do_enqueue(data);
}

extern "C"
uint32_t dec_consume32(uint64_t qid) {
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_CONSUMER;
#endif
  uint32_t ret = DEC_QUEUES[qid].do_dequeue();
  //printf("consuming 32 from qid:%d   value:%d\n", (int) qid, (int) ret); 
  return ret;  
}

extern "C"
uint64_t dec_consume64(uint64_t qid) {
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_CONSUMER;
#endif
  uint64_t ret = DEC_QUEUES[qid].do_dequeue();
  //printf("consuming 64 from qid:%d   value:%d\n", (int) qid, (int) ret); 
  return ret;  
}



// Async Loads
extern "C"
void dec_load32_async(uint64_t qid, uint32_t *addr) {
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_open_producer(qid);
  dec_produce32(qid, *addr);
  dec_close_producer(qid);
}

extern "C"
void dec_load64_async(uint64_t qid, uint64_t *addr) {
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_open_producer(qid);
  dec_produce64(qid, *addr);
  dec_close_producer(qid);
}


// Async RMWs
/*extern "C"
void dec_atomic_fetch_add_async(uint64_t qid, uint32_t *addr, uint32_t val) {
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_open_producer(qid);
  dec_produce32(qid, dec_atomic_fetch_add(addr, val));
  dec_close_producer(qid);
  }*/

// Async RMWs
extern "C"
void dec_atomic_fetch_add_async(uint32_t *addr, uint32_t val) {
  uint64_t qid = (uint64_t) get_producer_qid();
  dec_open_producer(qid);
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_produce32(qid, dec_atomic_fetch_add(addr, val));
  dec_close_producer(qid);
}

extern "C"
void dec_atomic_fetch_add_float_async(uint64_t qid, float *addr, float val) {
  dec_open_producer(qid);
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_produce32(qid, dec_atomic_fetch_add_float(addr, val));
  dec_close_producer(qid);
}

// leave unimplemented for now
extern "C"
void dec_atomic_fetch_and_async(uint64_t qid, uint32_t *addr, uint32_t val) {
  // unimplemented
  assert(0);
}

// leave unimplemented for now
extern "C"
void dec_atomic_fetch_or_async(uint64_t qid, uint32_t *addr, uint32_t val) {
  // unimplemented
  assert(0);
}

// leave unimplemented for now
extern "C"
void dec_atomic_fetch_xor_async(uint64_t qid, uint32_t *addr, uint32_t val) {
  // unimplemented
  assert(0);
}

// leave unimplemented for now
extern "C"
void dec_atomic_fetch_max_async(uint64_t qid, uint32_t *addr, uint32_t val) {
  // unimplemented
  assert(0);
}
/*
extern "C"
void dec_atomic_fetch_min_async(uint64_t qid, uint32_t *addr, uint32_t val) {
  dec_open_producer(qid);
#if defined(DEC_BUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_produce32(qid, dec_atomic_fetch_min(addr, val));
  dec_close_producer(qid);
  }*/

extern "C"
void dec_atomic_fetch_min_async(uint32_t *addr, uint32_t val) {
  uint64_t qid = (uint64_t) get_producer_qid();
  dec_open_producer(qid);
#if defined(DEC_BUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_produce32(qid, dec_atomic_fetch_min(addr, val));
  dec_close_producer(qid);
}

// leave this one unimplemented
extern "C"
void dec_atomic_exchange_async(uint64_t qid, uint32_t *addr, uint32_t val) {
  // unimplemented
  assert(0);
}

extern "C"
void dec_atomic_compare_exchange_async(uint64_t qid, uint32_t *addr, uint32_t to_compare, uint32_t new_val) {
  //printf("atomic_compare_exchange_async to qid: %d", qid);
  dec_open_producer(qid);
#if defined(DEC_DEBUG)
  DEBUG_CHECK;
  DEBUG_PRODUCER;
#endif
  dec_produce32(qid, dec_atomic_compare_exchange(addr, to_compare, new_val));
  dec_close_producer(qid);
}

extern "C"
void * dec_malloc(unsigned long size) {
  return malloc(size);
    //return malloc(size)
}

// We need to add an dec_atomic_add_float_async

//#pragma once

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
  return dec_atomic_fetch_min((uint32_t*) addr, value);
}


__attribute__((noinline))
extern "C"
int compute_exclusive_compare_exchange(int *addr, int val1, int val2) {
  return dec_atomic_compare_exchange((uint32_t*) addr, val1, val2);
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
