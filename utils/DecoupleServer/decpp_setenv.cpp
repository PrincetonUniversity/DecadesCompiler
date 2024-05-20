#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>

extern "C"
void desc_setenv(int num_threads, int num_consumers, char decoupling_mode) {

  int DEC_NUM_THREADS = num_threads;
  int DEC_NUM_CONSUMERS = num_consumers;
  char DEC_DECOUPLING_MODE = decoupling_mode;
  printf("DEC_NUM_THREADS:%d\n", DEC_NUM_THREADS);
  printf("DEC_NUM_CONSUMERS:%d\n", DEC_NUM_CONSUMERS);
  printf("DEC_DECOUPLING_MODE:%c\n", DEC_DECOUPLING_MODE);


  std::string a1 = std::to_string(num_threads);
  char *s1 = new char[a1.size()+1];
  strcpy( s1, a1.c_str() );
  setenv("DEC_NUM_THREADS", s1, 1);

  std::string a2 = std::to_string(num_consumers);
  char *s2 = new char[a2.size()+1];
  strcpy( s2, a2.c_str() );
  setenv("DEC_NUM_CONSUMERS", s2, 1);

  std::string a3 = std::to_string(decoupling_mode);
  char *s3 = new char[a3.size()+1];
  strcpy( s3, a3.c_str() );
  setenv("DEC_DECOUPLING_MODE", s3, 1);
}
