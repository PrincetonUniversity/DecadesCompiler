#include "desc_DECADES.h" // hack so it doesn't think we have 2 linking files
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h>
#include "assert.h"
#include "desc.h"
#include <map>

#define MAX_THREADS 256 // Because the numba pipeline has issues with dynamic allocation

#define QUEUE_SIZE 256

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

// Circular buffer for queues
template <typename T>
struct T_queue {
  T arr[QUEUE_SIZE];
  volatile int head;
  volatile int tail;
  
  bool is_full() {    
    return tail == ((head + 1) % QUEUE_SIZE);
  }
  
  bool is_empty() {
    return (head == tail);
  }
  
  void enqueue(T v) {
    //printf("accessing %d\n", head);
    int index = head;
    arr[index] = v;
    head = ((index + 1) % QUEUE_SIZE);
  }
  
  T dequeue() {
    //printf("accessing %d\n", tail);
    int index = tail;
    T ret = arr[index];
    tail = ((index + 1) % QUEUE_SIZE);
    return ret;
  }
  
  void do_enqueue(T v) {
    //printf("pushing value begin %d\n", v);
    int index = head;
    while (tail == ((head + 1) % QUEUE_SIZE)) { __asm__ ( "pause;" );};
    enqueue(v);
    //printf("pushing value end %d\n", v);
  }

  T do_dequeue() {
    //printf("returning value begin\n");
    int index = tail;
    while (head == index) {__asm__ ( "pause;" );};
    T ret = dequeue();
    //printf("returning value end %d\n", ret);
    return ret;
  }  
};

T_queue<int> i32_queue_compute_to_supply[MAX_THREADS];
T_queue<int> i32_queue_supply_to_compute[MAX_THREADS];

T_queue<uint64_t> i64_queue_compute_to_supply[MAX_THREADS];
T_queue<uint64_t> i64_queue_supply_to_compute[MAX_THREADS];

T_queue<void*> addr_queue_compute_to_supply[MAX_THREADS];
T_queue<void*> addr_queue_supply_to_compute[MAX_THREADS];

T_queue<float> float_queue_compute_to_supply[MAX_THREADS];
T_queue<float> float_queue_supply_to_compute[MAX_THREADS];

T_queue<double> double_queue_compute_to_supply[MAX_THREADS];
T_queue<double> double_queue_supply_to_compute[MAX_THREADS];

T_queue<char> i8_queue_compute_to_supply[MAX_THREADS];
T_queue<char> i8_queue_supply_to_compute[MAX_THREADS];

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

// Compute

int _desc_compute_consume_i32() {
  int pair_tid = omp_get_thread_num();
  return i32_queue_supply_to_compute[pair_tid].do_dequeue();    
}

char _desc_compute_consume_i8() {
  int pair_tid = omp_get_thread_num();
  return i8_queue_supply_to_compute[pair_tid].do_dequeue();  
}

extern "C"
char desc_compute_consume_i8(int label) {
  my_print_label("compute consuming i8\n", label);
  char tmp = _desc_compute_consume_i8();
  return tmp;  
}

extern "C"
int desc_compute_consume_i32(int label) {
  my_print_label("compute consuming i32\n", label);
  int tmp = _desc_compute_consume_i32();
  return tmp;
}

extern "C"
char desc_compute_consume_cmp(int label) {
  my_print_label("compute consuming cmp\n", label);
  int tmp = _desc_compute_consume_i32();
  my_print_label("compute finished consuming cmp\n", label);
  return tmp;
}

uint64_t _desc_compute_consume_i64() {
  int pair_tid = omp_get_thread_num();  
  return i64_queue_supply_to_compute[pair_tid].do_dequeue();    
}

extern "C"
uint64_t desc_compute_consume_i64(int label) {
  my_print_label("compute consuming i64\n", label);
  uint64_t tmp =  _desc_compute_consume_i64();
  return tmp;  
}

extern "C"
void * desc_compute_consume_ptr(int label) {
  my_print_label("compute consuming ptr\n", label);
  int pair_tid = omp_get_thread_num();
  void * ret = addr_queue_supply_to_compute[pair_tid].do_dequeue();
  my_print_label("compute finished consuming ptr\n", label);
  return ret;
}

void * desc_compute_consume__Znwm(int label) {
  assert(0);
}

void * desc_compute_consume__Znam(int label) {
  assert(0);
}

float _desc_compute_consume_float() {
  int pair_tid = omp_get_thread_num();
  return float_queue_supply_to_compute[pair_tid].do_dequeue();
}

extern "C"
float desc_compute_consume_float(int label) {
  my_print_label("compute consuming float\n",label);
  float ret = _desc_compute_consume_float();
  return ret;
}

extern "C"
double desc_compute_consume_double(int label) {
  my_print_label("compute consuming double\n", label);
  int pair_tid = omp_get_thread_num();
  return double_queue_supply_to_compute[pair_tid].do_dequeue();
}


void _desc_compute_produce_i32(int x) {
  int pair_tid = omp_get_thread_num();
  i32_queue_compute_to_supply[pair_tid].do_enqueue(x);
}

void _desc_compute_produce_i8(char x) {
  int pair_tid = omp_get_thread_num();
  i8_queue_compute_to_supply[pair_tid].do_enqueue(x);
}

extern "C"
void desc_compute_produce_i8(char x, int label) {
  my_print_label("compute produce i8\n", label);
  _desc_compute_produce_i8(x);
}

extern "C"
void desc_compute_produce_i32(int x, int label) {
  my_print_label("compute produce i32\n", label);
  _desc_compute_produce_i32(x);
}

void _desc_compute_produce_i64(uint64_t x) {
  int pair_tid = omp_get_thread_num();  
  i64_queue_compute_to_supply[pair_tid].do_enqueue(x);
}

extern "C"
void desc_compute_produce_i64(uint64_t x, int label) {
  my_print_label("compute produce i64\n", label);
  _desc_compute_produce_i64(x);
  my_print_label("compute finished produce i64\n", label);
}

void _desc_compute_produce_ptr(void *x) {
  int pair_tid = omp_get_thread_num();
  addr_queue_compute_to_supply[pair_tid].do_enqueue(x);
}

extern "C"
void desc_compute_produce_ptr(void *x, int label) {
  my_print_label("compute produce ptr\n", label);
  _desc_compute_produce_ptr(x);
}

void  _desc_compute_produce_float(float x) {
  int pair_tid = omp_get_thread_num();
  float_queue_compute_to_supply[pair_tid].do_enqueue(x);
}

extern "C"
void desc_compute_produce_float(float x, int label) {
  my_print_label("compute produce float\n", label);
  _desc_compute_produce_float(x);
}

extern "C"
void desc_compute_produce_double(double x, int label) {
  my_print_label("compute produce double\n", label);
  int pair_tid = omp_get_thread_num();
  double_queue_compute_to_supply[pair_tid].do_enqueue(x);
}

// Supply

void _desc_supply_produce_i32(int val) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  i32_queue_supply_to_compute[pair_tid].do_enqueue(val);
}

void _desc_supply_produce_i8(char val) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  i8_queue_supply_to_compute[pair_tid].do_enqueue(val);
}

extern "C"
void desc_supply_produce_i8(char val, int label) {
  SUPPLY_PRODUCE_I8+=1;
  my_print_label("supply produce i8\n", label);
  _desc_supply_produce_i8(val);
}

extern "C"
void desc_supply_load_produce_i8(char *addr, int label) {
  SUPPLY_LOAD_PRODUCE_I8+=1;
  my_print_label("supply load produce i8\n", label);
  _desc_supply_produce_i8(*addr);
}

extern "C"
void desc_supply_produce_i32(int val, int label) {
  SUPPLY_PRODUCE_I32+=1;
  my_print_label("supply produce i32\n", label);
  count_load(label);
  _desc_supply_produce_i32(val);
}

extern "C"
void desc_supply_special_produce_cmp(char cmp, int label) {
  SUPPLY_PRED_SWAPS += 1;
  my_print_label("supply produce cmp\n", label);
  _desc_supply_produce_i32(cmp);
  my_print_label("supply finished produce cmp\n", label);
}

extern "C"
void desc_supply_load_produce_i32(int* addr, int label) {
  SUPPLY_LOAD_PRODUCE_I32+=1;
  count_terminal_load(label);
  my_print_label("supply load producing i32\n", label);
  _desc_supply_produce_i32(*addr);
}


void _desc_supply_produce_i64(uint64_t val) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);  
  i64_queue_supply_to_compute[pair_tid].do_enqueue(val);

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
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  addr_queue_supply_to_compute[pair_tid].do_enqueue(addr);
}

void desc_special_supply_produce__Znwm(char *cmp, int label) {
  assert(0);
}

void desc_special_supply_produce__Znam(char *cmp, int label) {
  assert(0);
}

extern "C"
void desc_supply_produce_ptr(void *addr, int label) {
  SUPPLY_PRODUCE_PTR+=1;
  my_print_label("supply producing ptr\n", label);
  count_load(label);
  _desc_supply_produce_ptr(addr);
  my_print_label("supply finished producing ptr\n", label);
}

extern "C"
void desc_supply_load_produce_ptr(void **addr, int label) {
  SUPPLY_LOAD_PRODUCE_PTR+=1;
  count_terminal_load(label);
  my_print_label("supply load producing ptr\n", label);
  _desc_supply_produce_ptr(*addr);
}

void _desc_supply_produce_float(float val) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  float_queue_supply_to_compute[pair_tid].do_enqueue(val);
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
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  double_queue_supply_to_compute[pair_tid].do_enqueue(val);
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

// supply consume

void _desc_supply_consume_i8(char *addr) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  *addr = i8_queue_compute_to_supply[pair_tid].do_dequeue();
}

extern "C"
void desc_supply_consume_i8(char *addr, int label) {
  SUPPLY_CONSUME_I8+=1;
  my_print_label("supply consume i8", label);
  _desc_supply_consume_i8(addr);
}

void _desc_supply_consume_i32(int *addr) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  *addr = i32_queue_compute_to_supply[pair_tid].do_dequeue();
}

extern "C"
void desc_supply_consume_i32(int *addr, int label) {
  SUPPLY_CONSUME_I32 += 1;
  my_print_label("supply consume i32\n", label);
  _desc_supply_consume_i32(addr);
}

void _desc_supply_consume_i64(uint64_t *addr) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);  
  *addr = i64_queue_compute_to_supply[pair_tid].do_dequeue();
}

extern "C"
void desc_supply_consume_i64(uint64_t *addr, int label) {
  SUPPLY_CONSUME_I64 += 1;
  my_print_label("supply consume i64\n", label);
  _desc_supply_consume_i64(addr);
  my_print_label("supply finished consume i64\n", label);

}

void _desc_supply_consume_ptr(void **addr) {
  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  *addr = addr_queue_compute_to_supply[pair_tid].do_dequeue();
}

extern "C"
void desc_supply_consume_ptr(void **addr, int label) {
  SUPPLY_CONSUME_PTR += 1;
  my_print_label("supply consume ptr\n", label);

  _desc_supply_consume_ptr(addr);
}

extern "C"
void desc_supply_consume_float(float *addr, int label) {
  SUPPLY_CONSUME_FLOAT += 1;
  my_print_label("supply consume float\n", label);

  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  *addr = float_queue_compute_to_supply[pair_tid].do_dequeue();;
}

extern "C"
void desc_supply_consume_double(double *addr, int label) {
  SUPPLY_CONSUME_DOUBLE += 1;
  my_print_label("supply consume float\n", label);

  int pair_tid = omp_get_thread_num() - (omp_get_num_threads()/2);
  *addr = double_queue_compute_to_supply[pair_tid].do_dequeue();
}

// supply ALU RMWs

extern "C"
void desc_supply_alu_rmw_fetchadd_float(float* addr, float to_add, int label) {
  SUPPLY_ALU_RMW_FETCHADD_FLOAT += 1;
  my_print_label("supply alu rmw fetchadd float\n", label);
  //if (terminal_rmws.find(label) != terminal_rmws.end()) {
  //  terminal_rmws[label] = terminal_rmws[label] + 1;
  //} else {
  //  terminal_rmws[label] = 1;
  // }

  float old_val = desc_DECADES_FETCH_ADD_FLOAT(addr, to_add);
  _desc_supply_produce_float(old_val); 
}

extern "C"
void desc_supply_alu_rmw_fetchmin(int* addr, int to_min, int label) {
  SUPPLY_ALU_RMW_FETCHMIN += 1;
  my_print_label("supply alu rmw fetchmin\n", label);
  //if (terminal_rmws.find(label) != terminal_rmws.end()) {
  //  terminal_rmws[label] = terminal_rmws[label] + 1;
  //} else {
  //  terminal_rmws[label] = 1;
  // }

  int is_min = desc_DECADES_FETCH_MIN(addr, to_min);
  _desc_supply_produce_i32(is_min);
}

extern "C"
void desc_supply_alu_rmw_cas(int* addr, int to_compare, int new_val, int label) {
  SUPPLY_ALU_RMW_CAS += 1;
  my_print_label("supply alu rmw cas\n", label);
  //if (terminal_rmws.find(label) != terminal_rmws.end()) {
  //  terminal_rmws[label] = terminal_rmws[label] + 1;
  //} else {
  //   terminal_rmws[label] = 1;
  // }

  int old_val = desc_DECADES_COMPARE_AND_SWAP(addr, to_compare, new_val);
  _desc_supply_produce_i32(old_val);
}

// init and cleanup
extern "C"
void desc_init(int threads) {

  int i;
  //Numba backend doesn't like dynamic memory
  assert(threads*2 <= MAX_THREADS);
  //DB = (Desc_Buffer*) malloc(sizeof(Desc_Buffer) * threads);
  //BT = (barrier_type*) malloc(sizeof(barrier_type) * threads);
  //local_sense = (int *) malloc(sizeof(int) * threads * 2);

  for (i = 0; i < threads; i++) {
    
    i32_queue_compute_to_supply[i].head = 0;
    i32_queue_compute_to_supply[i].tail = 0;
    i32_queue_supply_to_compute[i].head = 0;
    i32_queue_supply_to_compute[i].tail = 0;

    i64_queue_compute_to_supply[i].head = 0;
    i64_queue_compute_to_supply[i].tail = 0;
    i64_queue_supply_to_compute[i].head = 0;
    i64_queue_supply_to_compute[i].tail = 0;

    addr_queue_compute_to_supply[i].head = 0;
    addr_queue_compute_to_supply[i].tail = 0;
    addr_queue_supply_to_compute[i].head = 0;
    addr_queue_supply_to_compute[i].tail = 0;

    float_queue_compute_to_supply[i].head = 0;
    float_queue_compute_to_supply[i].tail = 0;
    float_queue_supply_to_compute[i].head = 0;
    float_queue_supply_to_compute[i].tail = 0;

    double_queue_compute_to_supply[i].head = 0;
    double_queue_compute_to_supply[i].tail = 0;
    double_queue_supply_to_compute[i].head = 0;
    double_queue_supply_to_compute[i].tail = 0;

    i8_queue_compute_to_supply[i].head = 0;
    i8_queue_compute_to_supply[i].tail = 0;
    i8_queue_supply_to_compute[i].head = 0;
    i8_queue_supply_to_compute[i].tail = 0;

  }

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

