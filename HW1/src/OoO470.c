#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../include/json_utils.h"

void commit_stage(){
  if(active_list.empty) return;

  int nb_committed = 0;
  instruction_state is = active_list.list[active_list.head];
  while(!active_list.empty && nb_committed < 4 && is.done){
    if(is.exception){
      epc = is.pc;
      pc = EXCEPTION_ADDRESS;
      exception = true;
      return;
    }
    push_free_list(is.oldDest);
    commit_active_list();
    is = active_list.list[active_list.head];
    nb_committed += 1;
  }
}

void execute2_stage(){
  pipeline_queue *pq = ALU2_reg;

  if(exception){
    while (pq != NULL){
      pipeline_queue *pq_next = pq->next;
      delete_pq_from_queue(&ALU2_reg, pq);
      pq = pq_next;
    }
    return;
  }

  while (pq != NULL){
    instruction_state *is = pq->is;

    if(!strncmp(is->opCode, "add", 4)){
      register_file[is->destReg] = is->opAValue + is->opBValue;
    }else if(!strncmp(is->opCode, "addi", 5)){
      register_file[is->destReg] = is->opAValue + (int64_t)(is->opBValue);
    }else if(!strncmp(is->opCode, "sub", 4)){
      register_file[is->destReg] = is->opAValue - is->opBValue;
    }else if(!strncmp(is->opCode, "mulu", 5)){
      register_file[is->destReg] = is->opAValue * is->opBValue;
    }else if(!strncmp(is->opCode, "divu", 5)){
      if(is->opBValue == 0)
        is->exception = true;
      else
        register_file[is->destReg] = is->opAValue / is->opBValue;
    }else if(!strncmp(is->opCode, "remu", 5)){
      if(is->opBValue == 0)
        is->exception = true;
      else
        register_file[is->destReg] = is->opAValue % is->opBValue;
    }

    if(!is->exception)
      busy_bit_table[is->destReg] = false;
    is->done = 1;

    pipeline_queue *pq_next = pq->next;
    delete_pq_from_queue(&ALU2_reg, pq);
    pq = pq_next;
  }
}

void execute1_stage(){
  pipeline_queue *pq = ALU1_reg;

  if(exception){
    while (pq != NULL){
      pipeline_queue *pq_next = pq->next;
      delete_pq_from_queue(&ALU1_reg, pq);
      pq = pq_next;
    }
    return;
  }

  while (pq != NULL){
    pipeline_queue *pq_next = pq->next;
    insert_is_in_queue(&ALU2_reg, pq->is);
    delete_pq_from_queue(&ALU1_reg, pq);
    pq = pq_next;
  }
}

void issue_stage(){
  pipeline_queue *pq = IQ_reg;

  if(exception){
    while (pq != NULL){
      pipeline_queue *pq_next = pq->next;
      delete_pq_from_queue(&IQ_reg, pq);
      pq = pq_next;
    }
    return;
  }

  int nb_issues = 0;
  while (pq != NULL){
    instruction_state *is = pq->is;

    if(!is->opAIsReady && !busy_bit_table[is->opARegTag]){
      is->opAIsReady = true;
      is->opAValue = register_file[is->opARegTag];
    }
    if(!is->opBIsReady && !busy_bit_table[is->opBRegTag]){
      is->opBIsReady = true;
      is->opBValue = register_file[is->opBRegTag];
    }

    pipeline_queue *pq_next = pq->next;
    if(nb_issues < 4 && is->opAIsReady && is->opBIsReady){
      insert_is_in_queue(&ALU1_reg, pq->is);
      delete_pq_from_queue(&IQ_reg, pq);
      nb_issues += 1;
    }

    pq = pq_next;
  }
}

int rename_and_dispatch_stage(){
  if(exception){
    for(int i = 0; i < 4; i+=1){
      DIR_reg[i] = NULL;
    }
    return 1;
  }

  int nb_to_rename = 0;
  for(int i = 0; i < 4; i+=1){
    if(DIR_reg[i] != NULL)
      nb_to_rename += 1;
  }
  if(empty_entries_active_list() < nb_to_rename)
    return 1;
  
  for(int i = 0; i < 4; i+=1){
    if(DIR_reg[i] != NULL){
      program_instruction *pi = DIR_reg[i];

      instruction_state *is = NULL;
      dispatch_active_list(&is);

      int op_a = register_map_table[pi->opB];
      if(!busy_bit_table[op_a]){
        is->opAIsReady = true;
        is->opAValue = register_file[op_a];
      }else{
        is->opARegTag = op_a;
      }
      if(!strncmp(pi->opCode, "addi", 5)){
        is->opBIsReady = true;
        is->opBValue = pi->opC;
      }else {
        int op_b = register_map_table[pi->opC];
        if(!busy_bit_table[op_b]){
          is->opBIsReady = true;
          is->opBValue = register_file[op_b];
        }else{
          is->opBRegTag = op_b;
        }
      }

      int log_dest = pi->opA;
      int dest_reg = pop_free_list();
      busy_bit_table[dest_reg] = true;
      int old_dest  = register_map_table[log_dest];
      register_map_table[log_dest] = dest_reg;
      
      is->destReg = dest_reg;
      is->logDest = log_dest;
      is->oldDest = old_dest;
      
      is->pc = pi->pc;
      strncpy(is->opCode, pi->opCode, 5);

      insert_is_in_queue(&IQ_reg, is);
    }
  }
  return 0;
}

void fetch_and_decode_stage(program *p){
  for(int i = 0; i < 4; i+=1){
    if(pc != p->instruction_count){
      DIR_reg[i] = &(p->instructions[pc]);
      pc += 1;
    }else{
      DIR_reg[i] = NULL;
    }
  } 
}

void exception_recovery_mode(){
  int nb_recovered = 0;
  while(!active_list.empty && nb_recovered < 4){
    instruction_state *is = NULL;
    exception_pop_active_list(&is);

    push_free_list(is->destReg);
    busy_bit_table[is->destReg] = false;
    register_map_table[is->logDest] = is->oldDest;

    nb_recovered += 1;
  }
}

void propagate(program *p){
  if(exception){
    exception_recovery_mode();
    return;
  }
  commit_stage();
  execute2_stage();
  execute1_stage();
  issue_stage();
  int backpressure = rename_and_dispatch_stage();
  if(!backpressure)
    fetch_and_decode_stage(p);
}

void reset_pipeline(){
  for(int i = 0; i < 64; i += 1){
    register_file[i] = 0;
    busy_bit_table[i] = false;
  }
  
  free_list.empty = true;
  free_list.full = false;
  free_list.head = 0;
  free_list.tail = 0;
  
  active_list.empty = true;
  active_list.full = false;
  active_list.head = 0;
  active_list.tail = 0;

  for(int i = 0; i < 32; i += 1){
    register_map_table[i] = i;
    push_free_list(i + 32);
  }

  pc = 0;
  epc = 0;
  exception = false;

  for(int i = 0; i < 4; i += 1){
    DIR_reg[i] = NULL;
  }
  IQ_reg = NULL;
  ALU1_reg = NULL;
  ALU2_reg = NULL;
}

void main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <input_json> <output_json>\n", argv[0]);
    return;
  }

  const char *input_json = argv[1];
  const char *output_json = argv[2];

  program *p = calloc(1, sizeof(program));
  convert_json_into_program(input_json, p);

  reset_pipeline();

  init_logs();
  append_state_to_logs();
  
  int cycle = 0;
  while(!((pc == p->instruction_count || pc == EXCEPTION_ADDRESS) &&  DIR_reg[0] == NULL && active_list.empty)){
    propagate(p);
    append_state_to_logs();
  }
  if(exception){
    exception = false;
    append_state_to_logs();
  }
  convert_logs_to_json(output_json);
}