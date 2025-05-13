#include "kernel1.h"

void kernel1( int array[ARRAY_SIZE] )
{
	#pragma HLS array_partition variable=array type=complete
	int i;
	for(i=0; i<ARRAY_SIZE; i++){
		#pragma HLS unroll
		array[i] = array[i] * 5;
    }
}
