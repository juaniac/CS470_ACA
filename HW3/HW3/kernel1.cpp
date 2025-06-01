#include "kernel1.h"

void kernel1(int array[ARRAY_SIZE]){
  for(int i=0; i<ARRAY_SIZE; i++){
    #pragma HLS pipeline II=1
    array[i] = (array[i] << 2) + array[i];
  }
}
