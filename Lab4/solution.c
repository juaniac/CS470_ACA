#include <emmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <x86intrin.h>

#include <stdlib.h>

uint8_t LUT[256 * 512];

int victim(int input) {
  int index = (input * 163) & 0xFF;
  volatile int internal_value = LUT[index * 512];
  return (internal_value * 233) & 0xFFFF;
}

void attack(int input) {
  // threshold depends on processors
  const int THRESHOLD = 250;

  // measure the access count of each block
  int hit_count[256] = {0};

  // repeat the attack to improve precision
  for (int rep = 0; rep < 100; ++rep) {
    // flush LUT from the cache
    for (int i = 0; i < 256; i++) {
      _mm_clflush(&LUT[i * 512]);
    }

    // wait for clflush to commit
    for (volatile int i = 0; i < 100; i++);

    // memory fence
    _mm_mfence();

    // call the victim function
    victim(input);

    for (int i = 0; i < 256; ++i) {
      volatile unsigned int junk = 0;
      uint64_t s0 = __rdtscp((uint32_t *)&junk);
      junk = LUT[i * 512]; // read it
      uint64_t delta = __rdtscp((uint32_t *)&junk) - s0;
      // check if it is less than the threshold
      if (delta < THRESHOLD) hit_count[i]++;
    }
  }

  // find the index with maximum probability
  int guessed_index = 0;
  uint64_t max_count = 0;

  for (int i = 0; i < 256; i++) {
    if (hit_count[i] > max_count) {
      max_count = hit_count[i];
      guessed_index = i;
    }
    // printing the distribution of the hit count
    //printf("%4d: %4d\t", i, hit_count[i]);
    //if (i % 8 == 7) printf("\n");
  }
  // compute the expected index
  int oracle_index = (input * 163) & 0xFF;

  // print the attack result
  printf("Attack index: %d, Correct index: %d \n", guessed_index, oracle_index);
}

int main() {
  // initialize the LUT
  for (int i = 0; i < 256; i++) {
    LUT[i * 512] = rand();
  }

  // perform the attack
  attack(10);
  attack(35);
  attack(100);

  // you may add more testing cases here

  return 0;
}