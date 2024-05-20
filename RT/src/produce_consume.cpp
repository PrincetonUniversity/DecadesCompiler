#include "../include/desc_DECADES.h" // hack so it doesn't think we have 2 linking files
#include "math.h"
#include "../include/dec_decoupling.h"
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <cstdlib>
#include <netinet/in.h> 
#include <string.h>
#include "assert.h"
#include "../include/desc.h"
#include <map>
#include <iostream>

#define MAX_THREADS 256 // Because the numba pipeline has issues with dynamic allocation

#define QUEUE_SIZE 256
//#define DECOUPLING_MODE = 'q'

int SUPPLY_PRODUCE_I8 = 0;
int SUPPLY_PRODUCE_I32 = 0;
int SUPPLY_PRODUCE_I64 = 0;
int SUPPLY_PRODUCE_PTR = 0;
int SUPPLY_PRODUCE_FLOAT = 0;
int SUPPLY_PRODUCE_DOUBLE = 0;

int SUPPLY_LOAD_PRODUCE_I8 = 0;
int SUPPLY_LOAD_PRODUCE_I32 = 0;
int SUPPLY_LOAD_PRODUCE_I64 = 0;
int SUPPLY_LOAD_PRODUCE_PTR = 0;
int SUPPLY_LOAD_PRODUCE_FLOAT = 0;
int SUPPLY_LOAD_PRODUCE_DOUBLE = 0;

int SUPPLY_CONSUME_I8 = 0;
int SUPPLY_CONSUME_I32 = 0;
int SUPPLY_CONSUME_I64 = 0;
int SUPPLY_CONSUME_PTR = 0;
int SUPPLY_CONSUME_FLOAT = 0;
int SUPPLY_CONSUME_DOUBLE = 0;

int SUPPLY_PRED_SWAPS = 0;

int SUPPLY_PRODUCE__Znwm = 0;
int SUPPLY_PRODUCE__Znam = 0;

int LOADS_REMOVED = 0;
int EXCLUSIVE_STORES = 0;
int EXCLUSIVE_RMWS = 0;

std::map<int, int> loads;
std::map<int, int> terminal_loads;
std::map<int, int> loads_removed;
std::map<int, int> terminal_rmws;

int COMPUTE_EXCLUSIVE_FETCHADD = 0;
int SUPPLY_ALU_RMW_FETCHADD_FLOAT = 0;
int SUPPLY_ALU_RMW_FETCHMIN = 0;
int SUPPLY_ALU_RMW_CAS = 0;

// this is the index_value to lookup for actual size in this table:
// size_table ={8, 12, 32, 48, 64, 80, 96, 128};
// It the can be a parameter to be set at compilation, for now it is hardcoded
int queueSize = 7;

extern int DEC_NUM_THREADS;
extern int DEC_NUM_CONSUMERS;
char DEC_DECOUPLING_MODE = 'g';


// init and cleanup
extern "C"
void desc_init(int num_threads, int num_consumers, char decoupling_mode) {
  //Numba backend doesn't like dynamic memory
  //assert(num_pairs*2 <= MAX_THREADS);
  // num_pairs is not used AND
  // the value of threads is reassigned here (NEED TO BE TAKEN CARE OF!)

  //int DEC_NUM_THREADS = num_threads;
  //int DEC_NUM_CONSUMERS = num_consumers;
  //char DEC_DECOUPLING_MODE = decoupling_mode;
  DEC_DECOUPLING_MODE = 'g';
  string a1 = "DEC_NUM_THREADS="+std::to_string(num_threads);
  char *s1 = new char[a1.size()+1];         // +1 for the null-termination
  strcpy( s1, a1.c_str() );
  //char const *p1 = (char *)("DEC_NUM_THREADS="+std::to_string(num_threads));
  putenv(s1);
  string a2 = "DEC_NUM_CONSUMERS="+std::to_string(num_consumers);
  char *s2 = new char[a2.size()+1];         // +1 for the null-termination  
  //char const *p2 = (char *)("DEC_NUM_CONSUMERS="+std::to_string(num_consumers));
  strcpy( s2, a2.c_str() );
  putenv(s2);
  string a3 = "DEC_DECOUPLING_MODE="+std::to_string(decoupling_mode);
  char *s3 = new char[a3.size()+1];         // +1 for the null-termination
  strcpy( s3, a3.c_str() );
  //char const *p3 = (char *)("DEC_DECOUPLING_MODE="+decoupling_mode);
  putenv(s3);
    
  int threads = DEC_NUM_THREADS;//DEC_NUM_CONSUMERS + DEC_NUM_PRODUCERS;
  
  switch(DEC_DECOUPLING_MODE){
  //conjoint, asymmetric (variable ratio):
  case 'c':
    {
      dec_fifo_init(DEC_NUM_CONSUMERS,queueSize);
      break;
    }
  // desc (1:1): 
  case 'd':
    {
      dec_fifo_init(2*threads,queueSize);
      break;
    }
  // graphattack (1:1):
  case 'g':
    {
      dec_fifo_init(2*threads,queueSize);
      break;
    }
  }
}



//int DEC_NUM_THREADS=4;
//int DEC_NUM_CONSUMERS=2;  
//int DEC_NUM_THREADS = atoi(getenv("DEC_NUM_THREADS"));
//int DEC_NUM_CONSUMERS = atoi(getenv("DEC_NUM_CONSUMERS"));
int DEC_NUM_PRODUCERS = DEC_NUM_THREADS - DEC_NUM_CONSUMERS;
//getenv("DECOUPLING_LIB_NAME_X86_SHMEM");


//  for bit-casting from and into the queues use:
//  u32 u1;            
//  u1.f = 45.6789;     //write float
//  printf("%d",u1.i);  //read int

typedef union {
  int i;
  float f;
  uint32_t ui;
 } u32;

typedef union {
  long long int i;
  uint64_t ui;
  double d;
  void * v;
 } u64;





int64_t get_producer_qid(int data = NAN){
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

uint64_t get_consumer_qid(){
  // switch(DECOUPLING_MODE) {
  // case 'i' :
  //   int qid = omp_get_thread_num() - DEC_NUM_PRODUCERS;
  // case 'm' :
  //
  // default :
  //  int qid = omp_get_thread_num();
  // }
  uint32_t qid = (uint32_t) omp_get_thread_num();
  //printf("in get_consumer_qid: %d\n", (int) qid);
  return qid;
}




void count_load(int label) {
}

void count_terminal_load(int label) {
}

extern "C"
void count_load_removed(int label) {
  
}

extern "C"
void count_exclusive_rmw() {
  EXCLUSIVE_RMWS++;
}

  /* Currently all the queues are 64bit in shmem and maple, this can be changed/extended
   [0: num_pairs)                 supply_to_compute
   [num_pairs: 2*num_pairs)       compute_to_supply
   [2*num_pairs: 3*num_pairs)     compute_to_supply (addr)
   [3*num_pairs: 4*num_pairs)     supply_to_compute (addr)
  */
  

int _desc_compute_consume_i32() {
  u32 u;
  uint64_t qid = get_consumer_qid();
  dec_open_consumer(qid);
  u.ui = dec_consume32(qid);  //read uint32_t
  dec_close_consumer(qid);
  return u.i;                 // return int
}

extern "C"
int desc_compute_consume_i32(int label) {
  int data = _desc_compute_consume_i32();
  return data;
}


extern "C"
char desc_compute_consume_i8(int label) {
  my_print_label("compute consuming i8\n", label);
  char tmp = (char) _desc_compute_consume_i32();
  return tmp;  
}



extern "C"
char desc_compute_consume_cmp(int label) {
  my_print_label("compute consuming cmp\n", label);
  char tmp = (char) _desc_compute_consume_i32(); 
  my_print_label("compute finished consuming cmp\n", label);
  return tmp;
}


uint64_t _desc_compute_consume_i64() {
  uint64_t qid = get_consumer_qid();
  dec_open_consumer(qid);
  uint64_t data = dec_consume64(qid);
  dec_close_consumer(qid);
  return data;
}


extern "C"
uint64_t desc_compute_consume_i64(int label) {
  my_print_label("compute consuming i64\n", label);
  uint64_t tmp =  _desc_compute_consume_i64();
  return tmp;  
}


void * _desc_compute_consume_ptr(int label) {
  // [3*num_pairs: 4*num_pairs)     supply_to_compute (addr)
  uint64_t qid = get_consumer_qid() + 3*DEC_NUM_CONSUMERS;
  u64 u;
  dec_open_consumer(qid);
  u.ui = dec_consume64(qid);
  dec_close_consumer(qid);
  return u.v;
}

extern "C"
void * desc_compute_consume_ptr(int label) {
  my_print_label("compute consuming ptr\n", label);
  void * ret = _desc_compute_consume_ptr(label);
  my_print_label("compute finished consuming ptr\n", label);
  return ret;
}


void * desc_compute_consume__Znwm(int label) {
  assert(0);
}

void * desc_compute_consume__Znam(int label) {
  assert(0);
}

// THIS NEEDS TO BE FIGURED OUT!! (ASK TYLER)
float _desc_compute_consume_float() {
  uint64_t qid = get_consumer_qid();
  u32 u;
  dec_open_consumer(qid);
  u.ui = dec_consume32(qid);
  dec_close_consumer(qid);
  return u.f;
}



extern "C"
float desc_compute_consume_float(int label) {
  my_print_label("compute consuming float\n",label);
  float ret = _desc_compute_consume_float();
  return ret;
}


// THIS NEEDS TO BE FIGURED OUT!! (ASK TYLER)
double _desc_compute_consume_double() {
  uint64_t qid = get_consumer_qid();
  u64 u;
  dec_open_consumer(qid);
  u.ui = dec_consume64(qid);
  dec_close_consumer(qid);
  return u.d;
}


extern "C"
double desc_compute_consume_double(int label) {
  my_print_label("compute consuming double\n", label);
  double ret  = _desc_compute_consume_double();
  return ret;
}




// Compute Produce Operations:
  /* Currently all the queues are 64bit in shmem and maple, this can be changed/extended
   [0: num_pairs)                 supply_to_compute
   [num_pairs: 2*num_pairs)       compute_to_supply
   [2*num_pairs: 3*num_pairs)     compute_to_supply (addr)
   [3*num_pairs: 4*num_pairs)     supply_to_compute (addr)
  */
  

void _desc_compute_produce_i32(int val) {
  uint64_t qid = get_consumer_qid() + DEC_NUM_CONSUMERS;
  u32 u;
  u.i =val;
  dec_open_producer(qid);
  dec_produce32(qid, u.ui);
  dec_close_producer(qid);
}

extern "C"
void desc_compute_produce_i32(int val, int label) {
  my_print_label("compute produce i32\n", label);
  _desc_compute_produce_i32(val);
}

extern "C"
void desc_compute_produce_i8(char val, int label) {
  my_print_label("compute produce i8\n", label);
  _desc_compute_produce_i32((int) val);
}



void _desc_compute_produce_i64(uint64_t val) {
  uint64_t qid = get_consumer_qid() + DEC_NUM_CONSUMERS;
  dec_open_producer(qid);
  dec_produce64(qid, val);
  dec_close_producer(qid);
}

extern "C"
void desc_compute_produce_i64(uint64_t val, int label) {
  my_print_label("compute produce i64\n", label);
  _desc_compute_produce_i64(val);
  my_print_label("compute finished produce i64\n", label);
}

void _desc_compute_produce_ptr(void *x) {
  // [2*num_pairs: 3*num_pairs)     compute_to_supply (addr)
  uint64_t qid = get_consumer_qid() + 2*DEC_NUM_CONSUMERS;
  u64 u;
  u.v = x;
  dec_open_producer(qid);
  dec_produce64(qid, u.ui);
  dec_close_producer(qid);
}

extern "C"
void desc_compute_produce_ptr(void *x, int label) {
  my_print_label("compute produce ptr\n", label);
  _desc_compute_produce_ptr(x);
}

void  _desc_compute_produce_float(float val) {
  uint64_t qid = get_consumer_qid() + DEC_NUM_CONSUMERS;
  u32 u;
  u.f = val;
  dec_open_producer(qid);
  dec_produce32(qid, u.ui);
  dec_close_producer(qid);
}

extern "C"
void desc_compute_produce_float(float val, int label) {
  my_print_label("compute produce float\n", label);
  _desc_compute_produce_float(val);
}


void  _desc_compute_produce_double(double val) {
  uint64_t qid = get_consumer_qid() + DEC_NUM_CONSUMERS;
  u64 u;
  u.d = val;
  dec_open_producer(qid);
  dec_produce64(qid, u.ui);
  dec_close_producer(qid);
}


extern "C"
void desc_compute_produce_double(double val, int label) {
  my_print_label("compute produce double\n", label);
  _desc_compute_produce_double(val);
}

// Supply Produce:
 
  /* Currently all the queues are 64bit in shmem and maple, this can be changed/extended
   [0: num_pairs)                 supply_to_compute
   [num_pairs: 2*num_pairs)       compute_to_supply
   [2*num_pairs: 3*num_pairs)     compute_to_supply (addr)
   [3*num_pairs: 4*num_pairs)     supply_to_compute (addr)
  */
  

void _desc_supply_produce_i32(int val) {
  uint64_t qid = get_producer_qid();
  //printf("_desc_supply_produce_i32   qid :%d\n", qid); 

  u32 u;
  u.i = val;
  dec_open_producer(qid);
  dec_produce32(qid, u.ui);
  dec_close_producer(qid);
}

extern "C"
void desc_supply_produce_i32(int val, int label) {
  SUPPLY_PRODUCE_I32+=1;
  count_load(label);
  _desc_supply_produce_i32(val);
}

extern "C"
void desc_supply_produce_i8(char val, int label) {
  SUPPLY_PRODUCE_I32+=1;
  count_load(label);
  _desc_supply_produce_i32((int) val);
}


extern "C"
void desc_supply_special_produce_cmp(char cmp, int label) {
  SUPPLY_PRED_SWAPS += 1;
  my_print_label("supply produce cmp\n", label);
  // will store char as uint32_t and convert back:
  _desc_supply_produce_i32((uint32_t) cmp);
  my_print_label("supply finished produce cmp\n", label);
}

extern "C"
void desc_supply_load_produce_i32(int* addr, int label) {
  SUPPLY_LOAD_PRODUCE_I32+=1;
  count_terminal_load(label);
  _desc_supply_produce_i32(*addr);
}


extern "C"
void desc_supply_load_produce_i8(char* addr, int label) {
  SUPPLY_LOAD_PRODUCE_I32+=1;
  count_terminal_load(label);
  _desc_supply_produce_i32((int) *addr);
}


void _desc_supply_produce_i64(uint64_t val) {
  uint64_t qid = get_producer_qid();
  //printf("_desc_supply_produce_i64   qid :%d\n", qid); 

  dec_open_producer(qid);
  dec_produce64(qid, val);
  dec_close_producer(qid);  
}

extern "C"
void desc_supply_produce_i64(uint64_t val, int label) {
  SUPPLY_PRODUCE_I64+=1;
  my_print_label("supply produce i64\n", label);
  count_load(label);
  _desc_supply_produce_i64(val);
}

extern "C"
void desc_supply_load_produce_i64(uint64_t *addr, int label) {
  SUPPLY_LOAD_PRODUCE_I64+=1;
  count_terminal_load(label);
  my_print_label("supply load producing i64\n", label);
  _desc_supply_produce_i64(*addr);
}

void _desc_supply_produce_ptr(void *addr) {
  //   [3*num_pairs: 4*num_pairs)     supply_to_compute (addr)
  uint64_t qid = get_producer_qid() + 3* DEC_NUM_CONSUMERS;
  //printf("_desc_supply_produce_ptr   qid :%d\n", qid); 
  u64 u;
  u.v = addr;
  dec_open_producer(qid);
  dec_produce64(qid, u.ui);
  dec_close_producer(qid);
}

 extern "C"
void desc_supply_produce_ptr(void *addr, int label) {
  SUPPLY_PRODUCE_PTR+=1;
  my_print_label("supply producing ptr\n", label);
  count_load(label);
  _desc_supply_produce_ptr(addr);
  my_print_label("supply finished producing ptr\n", label);
}

void desc_special_supply_produce__Znwm(char *cmp, int label) {
  assert(0);
}


void desc_special_supply_produce__Znam(char *cmp, int label) {
  assert(0);
}


extern "C"
void desc_supply_load_produce_ptr(void **addr, int label) {
  SUPPLY_LOAD_PRODUCE_PTR+=1;
  count_terminal_load(label);
  my_print_label("supply load producing ptr\n", label);
  _desc_supply_produce_ptr(*addr);
}

void _desc_supply_produce_float(float val) {
  uint64_t qid = get_producer_qid();
  //printf("_desc_supply_produce_float qid :%d\n", qid); 
  u32 u;
  u.f = val;
  dec_open_producer(qid);
  dec_produce32(qid, u.ui);
  dec_close_producer(qid);
}

extern "C"
void desc_supply_produce_float(float val, int label) {
  SUPPLY_PRODUCE_FLOAT+=1;
  my_print_label("supply produce float\n", label);
  count_load(label);
  _desc_supply_produce_float(val);
}

extern "C"
void desc_supply_load_produce_float(float *addr, int label) {
  SUPPLY_LOAD_PRODUCE_FLOAT+=1;
  count_terminal_load(label);
  _desc_supply_produce_float(*addr);
}

void _desc_supply_produce_double(double val) {
  uint64_t qid = get_producer_qid();
  //printf("_desc_supply_produce_double qid :%d\n", qid); 
  u64 u;
  u.d = val;
  dec_open_producer(qid);
  dec_produce64(qid, u.ui);
  dec_close_producer(qid);
}


extern "C"
void desc_supply_produce_double(double val, int label) {
  SUPPLY_PRODUCE_DOUBLE+=1;
  my_print_label("supply produce double\n", label);
  count_load(label);
  _desc_supply_produce_double(val);
}

extern "C"
void desc_supply_load_produce_double(double *addr, int label) {
  SUPPLY_LOAD_PRODUCE_DOUBLE+=1;
  count_terminal_load(label);
  my_print_label("supply load produce double\n", label);
  _desc_supply_produce_double(*addr);
}

// Supply Consume Operations:


  /* Currently all the queues are 64bit in shmem and maple, this can be changed/extended
   [0: num_pairs)                 supply_to_compute
   [num_pairs: 2*num_pairs)       compute_to_supply
   [2*num_pairs: 3*num_pairs)     compute_to_supply (addr)
   [3*num_pairs: 4*num_pairs)     supply_to_compute (addr)
  */
  

void _desc_supply_consume_i32(int *addr) {
  uint64_t qid = get_producer_qid() +DEC_NUM_CONSUMERS;
  dec_open_consumer(qid);
  *addr = dec_consume32(qid);
  dec_close_consumer(qid);
}

extern "C"
void desc_supply_consume_i32(int *addr, int label) {
  SUPPLY_CONSUME_I32 += 1;
  my_print_label("supply consume i32\n", label);
  _desc_supply_consume_i32(addr);
}

extern "C"
void desc_supply_consume_i8(char *addr, int label) {
  SUPPLY_CONSUME_I32 += 1;
  my_print_label("supply consume i8\n", label);
  _desc_supply_consume_i32((int*) addr);
}

void _desc_supply_consume_i64(uint64_t *addr) {
  uint64_t qid = get_producer_qid() +DEC_NUM_CONSUMERS;
  dec_open_consumer(qid);
  *addr = dec_consume64(qid);
  dec_close_consumer(qid);
}

extern "C"
void desc_supply_consume_i64(uint64_t *addr, int label) {
  SUPPLY_CONSUME_I64 += 1;
  my_print_label("supply consume i64\n", label);
  _desc_supply_consume_i64(addr);
  my_print_label("supply finished consume i64\n", label);

}

void _desc_supply_consume_ptr(void **addr) {
  uint64_t qid = get_producer_qid() + 2*DEC_NUM_CONSUMERS;
  u64 u;
  dec_open_consumer(qid);
  u.ui = dec_consume64(qid);
  dec_close_consumer(qid);
  *addr = u.v;
}


extern "C"
void desc_supply_consume_ptr(void **addr, int label) {
  SUPPLY_CONSUME_PTR += 1;
  my_print_label("supply consume ptr\n", label);
  _desc_supply_consume_ptr(addr);
}

void _desc_supply_consume_float(float *addr) {
  uint64_t qid = get_producer_qid() + DEC_NUM_CONSUMERS;
  u32 u;
  dec_open_consumer(qid);
  u.ui = dec_consume32(qid);
  dec_close_consumer(qid);
  *addr = u.f;
}

 
extern "C"
void desc_supply_consume_float(float *addr, int label) {
  SUPPLY_CONSUME_FLOAT += 1;
  my_print_label("supply consume float\n", label);
  _desc_supply_consume_float(addr);
}


void _desc_supply_consume_double(double *addr) {
  uint64_t qid = get_producer_qid() + DEC_NUM_CONSUMERS;
  u64 u;
  dec_open_consumer(qid);
  u.ui = dec_consume64(qid);
  dec_close_consumer(qid);
  *addr = u.d;
}
 
extern "C"
void desc_supply_consume_double(double *addr, int label) {
  SUPPLY_CONSUME_DOUBLE += 1;
  my_print_label("supply consume float\n", label);
  _desc_supply_consume_double(addr);
}

// supply ALU RMWs
/*
extern "C"
void desc_supply_alu_rmw_fetchadd_float(float* addr, float to_add, int label) {
  SUPPLY_ALU_RMW_FETCHADD_FLOAT += 1;
  my_print_label("supply alu rmw fetchadd float\n", label);
  uint64_t qid = get_producer_qid();
  printf("desc_supply_alu_rmw_fetchadd_float  qid:%d\n", qid);
  u32 u_to_add;
  u_to_add.f = to_add;
  dec_atomic_fetch_add_async(qid, (uint32_t*) addr, u_to_add.ui);
}
*/
extern "C"
void desc_supply_alu_rmw_fetchadd_float(float* addr, float to_add, int label) {
  SUPPLY_ALU_RMW_FETCHADD_FLOAT += 1;
  my_print_label("supply alu rmw fetchadd float\n", label);
  //uint64_t qid = get_producer_qid();
  //printf("desc_supply_alu_rmw_fetchadd_float  qid:%d\n", qid);
  u32 u_to_add;
  u_to_add.f = to_add;
  dec_atomic_fetch_add_async((uint32_t*) addr, u_to_add.ui);
}



extern "C"
void desc_supply_alu_rmw_fetchmin(int* addr, int to_min, int label) {
  SUPPLY_ALU_RMW_FETCHMIN += 1;
  my_print_label("supply alu rmw fetchmin\n", label);

  //uint64_t qid = get_producer_qid();
  //printf("desc_supply_alu_rmw_fetchmin  qid:%d\n", qid);
  u32 u_to_min;
  u_to_min.i = to_min;
  dec_atomic_fetch_min_async((uint32_t*) addr, u_to_min.ui);
}
/*
extern "C"
void desc_supply_alu_rmw_fetchmin(int* addr, int to_min, int label) {
  SUPPLY_ALU_RMW_FETCHMIN += 1;
  my_print_label("supply alu rmw fetchmin\n", label);

  uint64_t qid = get_producer_qid();
  printf("desc_supply_alu_rmw_fetchmin  qid:%d\n", qid);
  u32 u_to_min;
  u_to_min.i = to_min;
  dec_atomic_fetch_min_async(qid, (uint32_t*) addr, u_to_min.ui);
}*/

extern "C"
void desc_supply_alu_rmw_cas(int* addr, int to_compare, int new_val, int label) {
  SUPPLY_ALU_RMW_CAS += 1;
  my_print_label("supply alu rmw cas\n", label);

  uint64_t qid = get_producer_qid();
  //printf("desc_supply_alu_rmw_cas  qid:%d\n", qid);
  u32 u_to_compare, u_new_val;
  u_to_compare.i = to_compare;
  u_new_val.i = new_val;
  //printf("BEFORE dec_atomic_compare_exchange_async qid:%d\n", qid);
  dec_atomic_compare_exchange_async(qid, (uint32_t *)addr, u_to_compare.ui, u_new_val.ui);
}

  
extern "C"
void desc_cleanup() {

  int total_loads = SUPPLY_PRODUCE_I32   +
    SUPPLY_PRODUCE_I64   +
    SUPPLY_PRODUCE_PTR   +
    SUPPLY_PRODUCE_FLOAT +
    SUPPLY_PRODUCE_DOUBLE;
  
  int total_stores = SUPPLY_CONSUME_I32   +
    SUPPLY_CONSUME_I64   +
    SUPPLY_CONSUME_PTR   +
    SUPPLY_CONSUME_FLOAT +
    SUPPLY_CONSUME_DOUBLE;
  
  int total_opt_loads = SUPPLY_LOAD_PRODUCE_I32   +
    SUPPLY_LOAD_PRODUCE_I64   +
    SUPPLY_LOAD_PRODUCE_PTR   +
    SUPPLY_LOAD_PRODUCE_FLOAT +
    SUPPLY_LOAD_PRODUCE_DOUBLE;
  
  
  printf("\n\n-----\n");
  printf("decoupled runtime information:\n");
  
  printf("-----\n");
  printf("total predicate swaps: %d\n\n", SUPPLY_PRED_SWAPS);
  
  printf("-----\n");
  printf("total stores: %d\n", total_stores);
  printf("  stores i32    : %d\n",   SUPPLY_CONSUME_I32);
  printf("  stores i64    : %d\n",   SUPPLY_CONSUME_I64);
  printf("  stores ptr    : %d\n",   SUPPLY_CONSUME_PTR);
  printf("  stores float  : %d\n",   SUPPLY_CONSUME_FLOAT);
  printf("  stores double : %d\n\n", SUPPLY_CONSUME_DOUBLE);
  
  printf("-----\n");
  printf("total loads: %d\n", total_loads);
  printf("  loads i32    : %d\n", SUPPLY_PRODUCE_I32);
  printf("  loads i64    : %d\n", SUPPLY_PRODUCE_I64);
  printf("  loads ptr    : %d\n", SUPPLY_PRODUCE_PTR);
  printf("  loads float  : %d\n", SUPPLY_PRODUCE_FLOAT);
  printf("  loads double : %d\n\n", SUPPLY_PRODUCE_DOUBLE);
  
#define PERCENT(x,y) ((x+y > 0) ? (((float)x/(y+x)) * 100.0) : 0)
  
  printf("-----\n");
  printf("total opt. loads: %d (%.2f%%)\n", total_opt_loads,
	 PERCENT(total_opt_loads, total_loads));
  printf("  opt. loads i32    : %d (%.2f%%)\n", SUPPLY_LOAD_PRODUCE_I32,
	 PERCENT(SUPPLY_LOAD_PRODUCE_I32, SUPPLY_PRODUCE_I32));
  printf("  opt. loads i64    : %d (%.2f%%)\n", SUPPLY_LOAD_PRODUCE_I64,
	 PERCENT(SUPPLY_LOAD_PRODUCE_I64, SUPPLY_PRODUCE_I64));
  printf("  opt. loads ptr    : %d (%.2f%%)\n", SUPPLY_LOAD_PRODUCE_PTR,
	 PERCENT(SUPPLY_LOAD_PRODUCE_PTR, SUPPLY_PRODUCE_PTR));
  printf("  opt. loads float  : %d (%.2f%%)\n", SUPPLY_LOAD_PRODUCE_FLOAT,
	 PERCENT(SUPPLY_LOAD_PRODUCE_FLOAT, SUPPLY_PRODUCE_FLOAT));
  printf("  opt. loads double : %d (%.2f%%)\n\n", SUPPLY_LOAD_PRODUCE_DOUBLE,
	 PERCENT(SUPPLY_LOAD_PRODUCE_DOUBLE, SUPPLY_PRODUCE_DOUBLE));
  printf("loads breakdown:\n");
  for ( const auto &load : loads) {
    printf("%d: %d\n", load.first, load.second);
  }
  printf("\n");
  printf("opt. loads breakdown:\n");
  for ( const auto &load : terminal_loads) {
    printf("%d: %d\n", load.first, load.second);
  }
  printf("\n");
  printf("loads removed:\n");
  for ( const auto &load : loads_removed) {
    printf("%d: %d\n", load.first, load.second);
  }
  printf("total loads removed: %d\n", LOADS_REMOVED);
  printf("\n");
  printf("terminal rmws:\n");
  for ( const auto &rmw : terminal_rmws) {
    printf("%d: %d\n", rmw.first, rmw.second);
  }
  int TOTAL_TERMINAL_RMWS = SUPPLY_ALU_RMW_FETCHADD_FLOAT + SUPPLY_ALU_RMW_FETCHMIN + SUPPLY_ALU_RMW_CAS;
  printf("total terminal rmws: %d\n", TOTAL_TERMINAL_RMWS);
  printf("\n");
  printf("total compute exclusive rmws: %d\n", EXCLUSIVE_RMWS);
  printf("\n");
  printf("loads summary:\n");
  int TOTAL = total_loads + total_opt_loads + TOTAL_TERMINAL_RMWS + LOADS_REMOVED + EXCLUSIVE_RMWS;
  printf("total loads in program: %d\n", TOTAL);
  printf("%.2f%% loads\n", total_loads*100.0/TOTAL);
  printf("%.2f%% terminal loads\n", (total_opt_loads+ TOTAL_TERMINAL_RMWS)*100.0/TOTAL);
  printf("%.2f%% loads removed\n", LOADS_REMOVED*100.0/TOTAL);
  printf("%.2f%% compute exclusive loads\n", EXCLUSIVE_RMWS*100.0/TOTAL);
}

