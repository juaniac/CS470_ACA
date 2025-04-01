
#include <stdio.h>
#include <stdlib.h>
#include "parser.c"

long register_file[64];
int  busy_bit_table[64];
int  register_map_table[32];

long pc = 0;
long epc = 0;
int exception = 0;

typedef struct {
  int list[32];
  int head; //points to element to pop.
  int tail; //points to last element in the list. CANNOT OVERFLOW
}fifo_list;

fifo_list free_list;
int pop_free_list(){
  if(free_list.head == free_list.tail)
    return -1;
  int free_reg = free_list.list[free_list.head];
  free_list.head = (free_list.head + 1) % 32;
  return free_reg;
}

void push_free_list(int free_reg){
  free_list.tail += 1;
  free_list.list[free_list.tail] = free_reg;
}

void main(){
  program *p = calloc(1, sizeof(program));
  convert_json_into_program("input.json", p);
  printf("DONE \n");
}