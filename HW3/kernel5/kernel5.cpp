#include "kernel5.h"

float kernel5(float bound, float a[ARRAY_SIZE], float b[ARRAY_SIZE]){
  float sums[ARRAY_SIZE];
  int in_bounds[ARRAY_SIZE];
  for(int i = 0; i < ARRAY_SIZE; i++){
    #pragma HLS pipeline II=1
    float sum = a[i] + b[i];
    sums[i] = sum;
    in_bounds[i] = sum < bound;
  }
  for(int i = 0; i < ARRAY_SIZE; i++){
    if(!in_bounds[i])
      return sums[i];
  }
  return sums[ARRAY_SIZE - 1];
}
