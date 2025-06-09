
# HW4: Spectre Attack Report

This homework was performed on this processor: 12th Gen Intel(R) Core(TM) i7-12700H   2.30 GHz

## (1) How did you train the global and local branch predictors?

To trigger speculative execution on an out-of-bounds index, we used a training loop inside the `attack()` function that repeatedly called `victim_function()` with in-bounds values. This trains both the local predictor, which checks that the branch condition `if (x < array1_size)` as always true, and the global branch predictor, which uses recent history of taken branches to guess future behavior.

We interleaved training and attack in a way that performs one attack every `VALID_RUNS` iterations. This ensures the branch predictor is trained on many in-bounds accesses before seeing a malicious access, therefor increasing the likelihood of misprediction.

---

## (2) How did you extend the side channel?

The cache timing side channel was extended by mapping the secret-dependent value into a large array `array2[]` with 256 possible slots. Each secret byte value indexes a cache line.
The side channel is then measured using timing differences. If a value was accessed during speculation, it remains cached and thus when accessed is accessed faster.
To allow speculative execution to proceed past the bounds check `(if (x < array1_size))`, we flushed `array1_size` from the cache using `_mm_clflush()`. This slows down the bounds check, creating a window for speculative execution to load data based on a secret index.

---

## (3) What techniques did you use to improve attack accuracy?

To increase the reliability and clarity of the leaked values, we used multiple enhancements:

- Bitwise predication: We selected either `malicious_x` or `valid_x` without branching, avoiding interference with the branch predictor.

- Delay and memory fences: We introduced small delay loops and `_mm_mfence()` to give speculative execution time to execute before resolution.

- Cache flushing: Used `_mm_clflush()` to ensure memory was not cached, helping identify which lines were pulled in by speculation.

- Threshold calibration: The access time threshold was tuned using a helper function `find_threshold()` to distinguish cache hits from misses.

- Randomized probing order: Accesses to `array2[]` were shuffled with a `forward[]` array to avoid consistent memory access patterns that might trigger hardware optimizations or prefetchers.

- Repeated trials: We used `ATTACK_RUNS` to accumulate multiple measurements and build confidence in the most likely secret byte value.

---

## (4) How did you solve additional problems you encountered?

### Problem 1: Inconsistent misprediction  
**Fix**: We interleaved training and attack calls instead of separating them. This ensured the branch predictor was primed correctly just before the malicious access.

### Problem 2: Compiler optimizations removing memory access  
**Fix**: We used `volatile`, added XOR with a dummy value, and ensured the read access was observable:

```c
junk ^= *addr;
```

### Problem 3: Prefetching effects during timing  
**Fix**: We used shuffled access order via `forward[]` to eliminate false positives caused by linear memory scanning.

---
