#pragma once

#include <atomic>
#include <mutex>

int local_sense[MAX_THREADS]; // private per processor

typedef struct
{
  std::atomic<int> counter; // initialize to 0
  std::atomic<int> flag; // initialize to 0
  std::mutex lock;
} barrier_type;


// barrier for p processors
void simple_barrier(barrier_type* b, int tid, int p) {
  local_sense[tid] = 1 - local_sense[tid];
  int arrived = ++b->counter;
  if (arrived == p) {      
    b->counter = 0;
    b->flag = local_sense[tid];
  }
  else {
    
    while (b->flag != local_sense[tid]) {
       __asm__ ( "pause;" );
    }  // wait for flag
    }
}
