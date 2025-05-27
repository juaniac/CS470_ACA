# Homework 3 Report:
## Kernel 1:

- The modified code:
```ruby
void kernel1(int array[ARRAY_SIZE]){
  for(int i=0; i<ARRAY_SIZE; i++){
    #pragma HLS pipeline II=1
    array[i] = (array[i] << 2) + array[i];
  }
}
```

- The scheduled dataflow graph:

- The interval reported by Vitis HLS: 1

- The total number of cycles: 1026

- Explanation of the optimizations: <br>
One of the optimization here was to change the multiplication by a constant value of 5 into a shift and an addition (this is done automatically in Vitis but I made it explicit). Additionally each loop iteration is indepedent from each other so we can apply a pipeline optimization and achieve an II of 1.

## Kernel 2:

- The modified code:
```ruby
void kernel2(int array[ARRAY_SIZE]){
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
```

- The scheduled dataflow graph:

- The interval reported by Vitis HLS: 2

- The total number of cycles: 2045

- Explanation of the optimizations: <br>
To optimize this kernel we introduced `prev1`, `prev2`, and `prev3` variables to store the last three array values needed in the computation. This removes repeated array accesses and acts like a shift register. The multiplication and addition involved in computing `tmp` create a data dependency chain that cannot be completed within a single cycle. Since `tmp` is reused in the next iteration (as `prev3`), we are forced to schedule the loop with an II of 2, which is the loop iteration latency. <br>
There is a small constant-time setup (initialization of `prev1`, `prev2`, `prev3`), but it is negligible compared to the total runtime.

## Kernel 3:

- The modified code:
```ruby
void kernel3(float hist[ARRAY_SIZE], float weight[ARRAY_SIZE], int index[ARRAY_SIZE]){
  for (int i=0; i<ARRAY_SIZE; ++i) {
    #pragma HLS pipeline II=8
    hist[index[i]] = hist[index[i]] + weight[i];
  }
}
```

- The scheduled dataflow graph:

- The interval reported by Vitis HLS: 8

- The total number of cycles: 8194

- Explanation of the optimizations: <br>
The only optimization possible for this kernel is pipelining the loop with an II of 8 instead of the baseline 9. <br>
Here we take advantage of the fact that in the first cycle of the loop iteration we are only loading the values of `index[i]` and `weight[i]`. As the value of the histogram is not needed until the 2nd cycle, we can update the histogram of the previous iteration during the first cycle and not cause a dependency issue. We cannot have a lower II than this because a true dependency may exist between iterations if two different values of `i` yield the same `index[i]`, leading to reading or writing the wrong value in the histogram. <br>
Thus we have to take into account this worse case scenario at all times as we cannot statically know if this dependency exists or not.

## Kernel 4:

- The modified code:
```ruby
void kernel4(int array[ARRAY_SIZE], int index[ARRAY_SIZE], int offset){
  int tmp[ARRAY_SIZE];
  for (int i=offset+1; i<ARRAY_SIZE-1; ++i){
    #pragma HLS pipeline II=1
    tmp[i] = index[i] * (array[i + 1] - array[i]);
  }
  int acc = array[offset];
  for (int i=offset+1; i<ARRAY_SIZE-1; ++i){
    #pragma HLS pipeline II=1
    acc += tmp[i];
  }
  array[offset] = acc;
}
```

- The scheduled dataflow graph:

- The interval reported by Vitis HLS: 1

- The total number of cycles: 2055 - 2 * `offset`

- Explanation of the optimizations: <br>
To optimize this kernel, the original computation was split into two separate loops. This re-structuring isolates the heavy arithmetic operations from the accumulation step, enabling both loops to be independently pipelined with an II of 1. <br>
In the first loop, we compute the expression `index[i] * (array[i + 1] - array[i])` for each iteration and store the result in a temporary buffer. Since this loop has no interloop dependencies, each iteration can work independent and thus can be pipelined easily. <br>
The second loop then accumulates these values. Here we also take advantage of the fact that the latency of the addition operation is less than 1 cycle thus even though there is an interloop dependency, we can still achieve a II of 1. <br>
As a result, we achieve full pipelining across both loops. A drawback of doing this is that we require a temporary memory to store all of intermediate values.

## Kernel 5:

- The modified code:
```ruby
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
```

- The scheduled dataflow graph:

- The interval reported by Vitis HLS: 1, 2

- The total number of cycles: 1036 + 2 * `number_of_in_bounds_floats_in_the_array`

- Explanation of the optimizations:
In this optimization, the original while loop was divided into two distinct stages to allow for better pipelining and latency hiding. <br>
In the first loop, we compute the sum of each corresponding element in arrays `a` and `b`, store the result in a temporary `sums` array, and simultaneously record whether the sum is below the given bound in a separate `in_bounds` array. Because each iteration is independent, this loop can be fully pipelined with an II of 1. <br> 
The second loop then sequentially checks the `in_bounds` flags and returns the first sum that exceeds the bound. Due to the conditional early exit and loop-carried control dependencies, this second loop cannot be pipelined as aggressively and results in an II of 2. <br>
While this transformation adds overhead in terms of memory usage—requiring two additional arrays of the same size as the input, it enables the high-throughput computation of sums in the first stage. A downside of this approach is that it prevents an early exit during the computation stage. The first loop must always process the entire array even if an out-of-bound value appears early.<br>
Despite this, the overall worst-case performance is improved due to pipelining, especially when out-of-bound values are rare.