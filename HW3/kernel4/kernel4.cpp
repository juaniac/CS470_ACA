#include "kernel4.h"

void kernel4(int array[ARRAY_SIZE], int index[ARRAY_SIZE], int offset)
{
	int tmp[ARRAY_SIZE];
    for (int i=offset+1; i<ARRAY_SIZE-1; ++i){
		#pragma HLS pipeline II=1
    	tmp[i] = - index[i] * array[i] + index[i] * array[i + 1];
    }
    int acc = array[offset];
    for (int i=offset+1; i<ARRAY_SIZE-1; ++i){
        acc += tmp[i];
    }
    array[offset] = acc;
}
