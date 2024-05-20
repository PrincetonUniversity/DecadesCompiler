#if defined(BARE_METAL)
void assert(int condition){
    if (condition){
        printf("ASSERT!!\n");
    }
}
#else
#include "../include/dec_decoupling.h"
#include "stdio.h"
#include "assert.h"
#endif
#define BYTE         8
#define TILE         28
#define FIFO         9
#define BASE   0xe100900000

#define QUEUE_SIZE 128
#define MAX_QUEUES 256
#define MAX_TILES 16
#define SP_SIZE 256
#define QUEUE_PER_IS 8
#define INVALID_FIFO 65536

//#define DEC_DEBUG 1 // Do some runtime checks, but potentially slows
#define DEBUG_INIT assert(initialized);
#define DEBUG_NOT_INIT assert(!initialized);

#ifndef BARE_METAL  
  #include "dcp_alloc.h"
#endif

//static variables
static uint32_t num_tiles;
static volatile uint32_t initialized = 0;
static uint32_t queues_per_tile;
static uint64_t fpid[MAX_QUEUES];
static uint64_t fcid[MAX_QUEUES];
static uint64_t base[MAX_TILES];

static uint64_t cons_addr = (uint64_t)(7*BYTE);
static uint64_t prod_addr = (uint64_t)(8*BYTE);
//ATOMICs
static uint64_t add_addr  = (uint64_t)(32*BYTE);
static uint64_t and_addr  = (uint64_t)(33*BYTE);
static uint64_t or_addr   = (uint64_t)(34*BYTE);
static uint64_t xor_addr  = (uint64_t)(35*BYTE);
static uint64_t max_addr  = (uint64_t)(36*BYTE);
static uint64_t maxu_addr = (uint64_t)(37*BYTE);
static uint64_t min_addr  = (uint64_t)(38*BYTE);
static uint64_t minu_addr = (uint64_t)(39*BYTE);
static uint64_t swap_addr = (uint64_t)(40*BYTE);
static uint64_t cas1_addr = (uint64_t)(41*BYTE);
static uint64_t cas2_addr = (uint64_t)(42*BYTE);
//TLOAD
static uint64_t tload_addr32 = (uint64_t)(10*BYTE);
static uint64_t tload_addr64 = (uint64_t)(11*BYTE);
static uint64_t push_loop = (uint64_t)(14*BYTE);
//CONFIG
static uint64_t destroy_tile_addr  = (uint64_t)(1*BYTE);
//UNUSED
static uint64_t producer_fifoc_addr  = (uint64_t)(6*BYTE);
static uint64_t stats_addr           = (uint64_t)(12*BYTE);

//CONF TLB
static uint64_t tlb_flush            = (uint64_t)(13*BYTE);
static uint64_t conf_tlb             = (uint64_t)(15*BYTE | 1 << FIFO);

//CONNECT
static uint64_t producer_conf_addr  = (uint64_t)(2*BYTE);
static uint64_t consumer_conf_addr  = (uint64_t)(3*BYTE);
static uint64_t producer_dconf_addr = (uint64_t)(4*BYTE);
static uint64_t consumer_dconf_addr = (uint64_t)(5*BYTE);

// Init and End decoupling
extern "C"
uint32_t dec_open_producer(uint64_t qid) {
while (!initialized)
  ;
#if defined(DEC_DEBUG)
  assert(fpid[qid] == INVALID_FIFO);
#endif

  uint32_t tile = qid/queues_per_tile;
  uint64_t fifo = base[tile] | ( (qid%queues_per_tile) << FIFO);
  uint32_t res_producer_conf; 
  // connect can return 0 if queue does not exist or if it is already taken 
  do {res_producer_conf = *(volatile uint64_t*)(producer_conf_addr | fifo);
    //printf("OPROD:%d\n",res_producer_conf);
    } while (res_producer_conf == 0);
  fpid[qid] = fifo;
  return 1;
}

extern "C"
uint32_t dec_open_consumer(uint64_t qid) {
while (!initialized)
  ;
#if defined(DEC_DEBUG)
  assert(fcid[qid] == INVALID_FIFO);
#endif

  uint32_t tile = qid/queues_per_tile;
  uint64_t fifo = base[tile] | ( (qid%queues_per_tile) << FIFO);
  uint32_t res_consumer_conf; 
  // connect can return 0 if queue does not exist or if it is already taken 
  do {res_consumer_conf = *(volatile uint64_t*)(consumer_conf_addr | fifo); 
    //printf("OCONS:%d\n",res_consumer_conf);
    } while (res_consumer_conf == 0);
  fcid[qid] = fifo;
  return 1;
}

extern "C"
uint32_t dec_close_producer(uint64_t qid) {
  uint64_t fifo = fpid[qid];
#if defined(DEC_DEBUG)
  assert(fifo !=INVALID_FIFO);
  //fpid[qid] = INVALID_FIFO;
#endif
  // close can return 0 if the queue does not exist or it is not configured by the core
  volatile uint32_t res_producer_conf = *(volatile uint64_t*)(producer_dconf_addr | fifo);
  //printf("CPROD:%d\n",res_producer_conf);
  return res_producer_conf;
}

extern "C"
uint32_t dec_close_consumer(uint64_t qid) {
  uint64_t fifo = fcid[qid];
#if defined(DEC_DEBUG)
  assert(fifo !=INVALID_FIFO);
  //fcid[qid] = INVALID_FIFO;
#endif
  volatile uint32_t res_consumer_conf = *(volatile uint64_t*)(consumer_dconf_addr | fifo);
  //printf("CCONS:%d\n",res_consumer_conf);
  return res_consumer_conf;
}

#define DCP_SIZE_8  0
#define DCP_SIZE_16 1
#define DCP_SIZE_32 2
#define DCP_SIZE_48 3
#define DCP_SIZE_64 4
#define DCP_SIZE_80 5
#define DCP_SIZE_96 6
#define DCP_SIZE_128 7  

#define LOOP_PRODUCE 50
#define LOOP_TLOAD64 49
#define LOOP_TLOAD32 48

//CLEANUP
extern "C"
uint32_t dec_fifo_cleanup() {
#if defined(DEC_DEBUG)
  //DEBUG_INIT;
#endif

  for (uint32_t i=0; i<1;i++){
    volatile uint32_t res_reset = *(volatile uint32_t*)(destroy_tile_addr | base[i]);
    //printf("RESET:%d\n",res_reset);
  }
  return 1; //can this fail? security issues?
}



uint32_t dec_fifo_init_conf(uint32_t count, uint32_t size, void * A, void * B, uint32_t op) {
#if defined(DEC_DEBUG)
  // hardware should also ignore init messages once to a tile which is already initialized
  DEBUG_NOT_INIT;
  for(uint32_t i=0; i<MAX_QUEUES;i++){
    fcid[i] = INVALID_FIFO;
    fpid[i] = INVALID_FIFO;
  }
  assert (count); //check that count is bigger than 0
#endif
  uint32_t len = DCP_SIZE_64; //size
  queues_per_tile = 2;
  uint32_t partial_tile = (count%queues_per_tile)>0;
  uint32_t entire_tiles = count/queues_per_tile;
  uint32_t num_tiles = entire_tiles + partial_tile;

  uint32_t allocated_tiles;
  uint64_t conf_tlb_addr;
#ifdef BARE_METAL
    for (int i = 0; i < num_tiles; i++){
        base[i] = BASE | ((i*4+1) << TILE); 
    }
    allocated_tiles = num_tiles;
#else
  allocated_tiles = (uint64_t)alloc_tile(num_tiles,base);
  if (!allocated_tiles) return 0;
  dec_fifo_cleanup();
  // TILE ALLOCATED AND RESET
  conf_tlb_addr = syscall(258);
  //printf("Syscall ptbase %p\n", (uint64_t*)conf_tlb_addr);
#endif

  //alloc "count" queues
  uint32_t i = 0;
  uint32_t res_producer_conf;

  do {
    //Hardware would do the best allocation based on the number of tiles!
    // init_tile responds with the number of allocated queues
    uint64_t addr_maple = (base[i] | (len << FIFO));
    //printf("Target MAPLE %d: address %p\n", i,(uint32_t*)addr_maple);
    // INIT TILE
    res_producer_conf = *(volatile uint32_t*)addr_maple;
    //printf("Target MAPLE %d: res %d, now config TLB\n", i, res_producer_conf);
#ifndef BARE_METAL
    *(volatile uint64_t*)(conf_tlb | base[i]) = conf_tlb_addr; 
    //printf("Config MAPLE %d: ptbase %p\n", i, (uint64_t*)conf_tlb_addr);
#endif
    i++;
  } while (i<allocated_tiles && res_producer_conf > 0);
  // count configured tiles
  uint32_t config_tiles = i;
  if (!res_producer_conf) config_tiles--;

  if (B!=0){
    uint64_t cr_conf_addr = op*BYTE;
    for (int j=0; j<config_tiles; j++){
      *(volatile uint64_t*)(cr_conf_addr | base[j]) = (uint64_t)A;
      *(volatile uint64_t*)(cr_conf_addr | base[j]) = (uint64_t)B;
    }
  }
  initialized = 1;
  uint32_t res = config_tiles*queues_per_tile;
  //printf("INIT: res 0x%08x\n", ((uint32_t)res) & 0xFFFFFFFF);
  return res;
}

extern "C"
uint32_t is_malloc(uint32_t tiles){
  //function would do iomap and return the number of pages (tiles) that were allocated
  return tiles;
}

extern "C"
uint32_t dec_fifo_init(uint32_t count, uint32_t size) {
    return dec_fifo_init_conf(count, size, 0, 0, 0);
}


//STATS
extern "C"
uint32_t dec_fifo_stats(uint64_t qid, uint32_t stat, uint32_t* result) {
  //TODO adapt to fetch only stat (key)
  volatile uint64_t res_stat = *(volatile uint64_t*)(stats_addr | fpid[qid]);
  return res_stat;
}

// Produce/Consume
extern "C"
void dec_produce32(uint64_t qid, uint32_t data) {
#if defined(DEC_DEBUG)
  assert(fpid[qid] !=INVALID_FIFO);
#endif
  *(volatile uint32_t *)(prod_addr | fpid[qid] ) = data;
}

extern "C"
void dec_produce64(uint64_t qid, uint64_t data) {
#if defined(DEC_DEBUG)
  assert(fpid[qid] !=INVALID_FIFO);
#endif
  *(volatile uint64_t *)(prod_addr | fpid[qid])  = data;
}

extern "C"
uint32_t dec_consume32(uint64_t qid) {
#if defined(DEC_DEBUG)
  assert(fcid[qid] !=INVALID_FIFO);
#endif
  uint32_t res = *(volatile uint32_t *)(cons_addr | fcid[qid] );
  return res;
}

extern "C"
uint64_t dec_consume64(uint64_t qid) {
#if defined(DEC_DEBUG)
  assert(fcid[qid] !=INVALID_FIFO);
#endif
  uint64_t res = *(volatile uint64_t *)(cons_addr | fcid[qid] );
  return res;
}

// Async Loads
extern "C"
void dec_load32_async(uint64_t qid, uint32_t *addr) {
#if defined(DEC_DEBUG)
  assert(fpid[qid] !=INVALID_FIFO);
#endif
  *(volatile uint64_t *)(tload_addr32 | fpid[qid] ) = (uint64_t)addr;
}

extern "C"
void dec_load64_async(uint64_t qid, uint64_t *addr) {
#if defined(DEC_DEBUG)
  assert(fpid[qid] !=INVALID_FIFO);
#endif
  *(volatile uint64_t *)(tload_addr64 | fpid[qid] ) = (uint64_t)addr;
}

// Async RMWs
/*
extern "C"
void dec_atomic_fetch_add_async(uint64_t qid, uint32_t *addr, uint32_t val) {
#if defined(DEC_DEBUG)
  assert(fpid[qid] !=INVALID_FIFO);
#endif
  *(volatile uint64_t *)(add_addr | fpid[qid] ) = ((uint64_t)addr) << 32 | (uint64_t)val;
}
*/

// Async RMWs
/*
extern "C"
void dec_atomic_fetch_add_async(uint64_t qid, uint32_t *addr, uint32_t val) {
#if defined(DEC_DEBUG)
  assert(fpid[qid] !=INVALID_FIFO);
#endif
  *(volatile uint64_t *)(add_addr | fpid[qid] ) = ((uint64_t)addr) << 32 | (uint64_t)val;
}
*/
//copy the rest when check that add works
extern "C"
void dec_atomic_fetch_add_async(uint32_t *addr, uint32_t val) {
}

extern "C"
void dec_atomic_fetch_and_async(uint64_t qid, uint32_t *addr, uint32_t val) {
}

extern "C"
void dec_atomic_fetch_or_async(uint64_t qid, uint32_t *addr, uint32_t val) {
}

extern "C"
void dec_atomic_fetch_xor_async(uint64_t qid, uint32_t *addr, uint32_t val) {
}

extern "C"
void dec_atomic_fetch_max_async(uint64_t qid, uint32_t *addr, uint32_t val) {
}

/*extern "C"
void dec_atomic_fetch_min_async(uint64_t qid, uint32_t *addr, uint32_t val) {
}*/

extern "C"
void dec_atomic_fetch_min_async(uint32_t *addr, uint32_t val) {
}

extern "C"
void dec_atomic_exchange_async(uint64_t qid, uint32_t *addr, uint32_t val) {
}

extern "C"
void dec_atomic_compare_exchange_async(uint64_t qid, uint32_t *addr, uint32_t data1, uint32_t data2) {
  uint64_t fifo = fpid[qid];
#if defined(DEC_DEBUG)
  assert(fifo !=INVALID_FIFO);
#endif
  if (data1<-32768 || data1>32767 || data2 <-32768 || data2>32767){ //Two messages
      *(volatile uint64_t *)(cas1_addr | fifo ) = ((uint64_t) data2) << 32 | (uint32_t)data1;
  } 
  *(volatile uint64_t *)(cas2_addr | fifo ) = ((uint64_t)addr) << 32 | data2 << 16 | (uint16_t)data1;  
}
