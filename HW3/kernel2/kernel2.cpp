#include "kernel2.h"

void kernel2( int array[ARRAY_SIZE] )
{
    int prev1 = array[0];
    int prev2 = array[1];
    int prev3 = array[2];
    for(int i=3; i<ARRAY_SIZE; i++){
        int tmp = prev3 + prev2 * prev1;
        prev1 = prev2;
        prev2 = prev3;
        prev3 = tmp;
        array[i] = tmp;
    }
}
