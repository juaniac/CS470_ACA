#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "../include/data_structures.h"

uint64_t register_file[64];
bool busy_bit_table[64];
int  register_map_table[32];

long pc;
long epc;
bool exception;

int_fifo_list free_list;

int pop_free_list(){
  if(free_list.empty){
    return -1;
  }
  
  int free_reg = free_list.list[free_list.head];
  free_list.head = (free_list.head + 1) % 32;

  if(free_list.head == free_list.tail)
    free_list.empty = true;
  free_list.full = false;
  return free_reg;
}

void push_free_list(int free_reg){
  if(free_list.full){
    return;
  }
    
  free_list.list[free_list.tail] = free_reg;
  free_list.tail = (free_list.tail + 1) % 32;

  if(free_list.head == free_list.tail)
    free_list.full = true; 
  free_list.empty = false;
}

instruction_fifo_list active_list;

void commit_active_list(){
  if(active_list.empty){
    printf("EMPTY ACTIVE LIST");
    return;
  }
    
  active_list.head = (active_list.head + 1) % 32;

  if(active_list.head == active_list.tail)
    active_list.empty = true;
  active_list.full = false;
}

void dispatch_active_list(instruction_state **is){
  if(active_list.full){
    printf("FULL ACTIVE LIST");
    return;
  }
    
  active_list.list[active_list.tail].destReg = 0;
  active_list.list[active_list.tail].done = false;
  active_list.list[active_list.tail].exception = 0;
  active_list.list[active_list.tail].logDest = 0;
  active_list.list[active_list.tail].oldDest = 0;
  active_list.list[active_list.tail].opAIsReady = false;
  active_list.list[active_list.tail].opARegTag = 0;
  active_list.list[active_list.tail].opAValue = 0;
  active_list.list[active_list.tail].opBIsReady = false;
  active_list.list[active_list.tail].opBRegTag = 0;
  active_list.list[active_list.tail].opBValue = 0;
  active_list.list[active_list.tail].pc = 0;

  (*is) = &(active_list.list[active_list.tail]); 
  
  active_list.tail = (active_list.tail + 1) % 32;
  
  if(active_list.head == active_list.tail)
    active_list.full = true; 
  active_list.empty = false;
}

int empty_entries_active_list(){
  if(active_list.full)
    return 0;
  if(active_list.empty)
    return 32;

  int cur = active_list.tail;
  int nb_empty_entries = 0;
  while(cur != active_list.head){
    cur = (cur + 1) % 32;
    nb_empty_entries += 1;
  }
  return nb_empty_entries;
}

void exception_pop_active_list(instruction_state **is){
  if(active_list.empty){
    printf("EMPTY ACTIVE LIST");
    return;
  }
    
  active_list.tail = (active_list.tail - 1) % 32;
  (*is) = &(active_list.list[active_list.tail]);

  if(active_list.tail == active_list.head)
    active_list.empty = true;
  active_list.full = false;
}

program_instruction *DIR_reg[4];
pipeline_queue *IQ_reg;
pipeline_queue *ALU1_reg;
pipeline_queue *ALU2_reg;

void insert_is_in_queue(
  pipeline_queue **queue, 
  instruction_state *is
){
  pipeline_queue* pq = calloc(1, sizeof(pipeline_queue));
  pq->is = is;

  pipeline_queue *next_pq = *queue;
  pipeline_queue *prev_pq = NULL;

  while (next_pq != NULL && next_pq->is->pc < is->pc) {
    prev_pq = next_pq;
    next_pq = next_pq->next;
  }

  pq->next = next_pq;
  pq->prev = prev_pq;

  if (next_pq != NULL) next_pq->prev = pq;
  if (prev_pq != NULL) prev_pq->next = pq;
  else *queue = pq;
}

void delete_pq_from_queue(
  pipeline_queue **queue, 
  pipeline_queue *pq
){
  if (pq->next != NULL) pq->next->prev = pq->prev;
  if (pq->prev != NULL) pq->prev->next = pq->next;
  else *queue = pq->next;
  free(pq);
}