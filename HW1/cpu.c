#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include "parser.c"
#include "cJSON.h"

uint64_t register_file[64];
bool busy_bit_table[64];
int  register_map_table[32];

long pc;
long epc;
bool exception;
#define EXCEPTION_ADDRESS 0x10000

typedef struct {
  int list[32];
  int head; 
  int tail; 
  bool empty;
  bool full;
}int_fifo_list;

int_fifo_list free_list;

int pop_free_list(){
  if(free_list.empty){
    printf("EMPTY FREE LIST");
    return -1;
  }
  
  int free_reg = free_list.list[free_list.head];
  free_list.head = (free_list.head + 1) % 32;

  if(free_list.head == free_list.tail)
    free_list.empty = true;
  free_list.full = false;
  return free_reg;
}

int push_free_list(int free_reg){
  if(free_list.full){
    printf("FULL FREE LIST");
    return -1;
  }
    
  free_list.list[free_list.tail] = free_reg;
  free_list.tail = (free_list.tail + 1) % 32;

  if(free_list.head == free_list.tail)
    free_list.full = true; 
  free_list.empty = false;
  return 0;
}

typedef struct{
  bool done;
  bool exception;
  int logDest;
  int oldDest;
  int pc;
  int destReg;
  bool opAIsReady;
  int opARegTag;
  uint64_t opAValue;
  bool opBIsReady;
  int opBRegTag;
  uint64_t opBValue;
  char opCode[5];
}instruction_state;

typedef struct{
  instruction_state list[32];
  int head; 
  int tail; 
  bool empty;
  bool full;
}instruction_fifo_list;

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

typedef struct pipeline_queue{
  struct pipeline_queue *prev;
  instruction_state *is;
  struct pipeline_queue *next;
}pipeline_queue;


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

void reset(){
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

cJSON *log_array = NULL;

void init_logger() {
  log_array = cJSON_CreateArray();
}

void append_state_to_log() {
  cJSON *root = cJSON_CreateObject();

  cJSON_AddNumberToObject(root, "PC", pc);

  cJSON *prf_array = cJSON_CreateArray();
  for (int i = 0; i < 64; i++) {
    //This fixes the rounding issues by directly copying the number in this buffer
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)register_file[i]);
    cJSON_AddItemToArray(prf_array, cJSON_CreateRaw(buffer)); 
  }
  cJSON_AddItemToObject(root, "PhysicalRegisterFile", prf_array);

  cJSON *prf_array = cJSON_CreateArray();
  for (int i = 0; i < 64; i++) {
    cJSON_AddItemToArray(prf_array, cJSON_CreateNumber(register_file[i])); 
  }
  cJSON_AddItemToObject(root, "PhysicalRegisterFile", prf_array);
  
  cJSON *dir_array = cJSON_CreateArray();
  int i = 0;
  while(DIR_reg[i] != NULL && i < 4){
    cJSON_AddItemToArray(dir_array, cJSON_CreateNumber(DIR_reg[i]->pc));
    i+=1;
  }
  cJSON_AddItemToObject(root, "DecodedPCs", dir_array);

  cJSON_AddNumberToObject(root, "ExceptionPC", epc);
  cJSON_AddBoolToObject(root, "Exception", exception);

  cJSON *rmt_array = cJSON_CreateArray();
  for (int i = 0; i < 32; i++) {
    cJSON_AddItemToArray(rmt_array, cJSON_CreateNumber(register_map_table[i]));
  }
  cJSON_AddItemToObject(root, "RegisterMapTable", rmt_array);

  cJSON *fl_array = cJSON_CreateArray();
  if (!free_list.empty) {
    int cur = free_list.head;
    while (true) {
      cJSON_AddItemToArray(fl_array, cJSON_CreateNumber(free_list.list[cur]));
      cur = (cur + 1) % 32;
      if (cur == free_list.tail) break;
    }
  }
  cJSON_AddItemToObject(root, "FreeList", fl_array);

  cJSON *bbt_array = cJSON_CreateArray();
  for (int i = 0; i < 64; i++) {
    cJSON_AddItemToArray(bbt_array, cJSON_CreateBool(busy_bit_table[i]));
  }
  cJSON_AddItemToObject(root, "BusyBitTable", bbt_array);

  cJSON *al_array = cJSON_CreateArray();
  if (!active_list.empty) {
    int idx = active_list.head;
    while (true) {
      instruction_state *is = &active_list.list[idx];
      cJSON *entry = cJSON_CreateObject();
      cJSON_AddBoolToObject(entry, "Done", is->done);
      cJSON_AddBoolToObject(entry, "Exception", is->exception);
      cJSON_AddNumberToObject(entry, "LogicalDestination", is->logDest);
      cJSON_AddNumberToObject(entry, "OldDestination", is->oldDest);
      cJSON_AddNumberToObject(entry, "PC", is->pc);
      cJSON_AddItemToArray(al_array, entry);
          
      idx = (idx + 1) % 32;
      if (idx == active_list.tail) break;
    }
  }
  cJSON_AddItemToObject(root, "ActiveList", al_array);

  cJSON *iq_array = cJSON_CreateArray();
  pipeline_queue *cur = IQ_reg;
  while (cur != NULL) {
    instruction_state *is = cur->is;
    if (is) {
      cJSON *entry = cJSON_CreateObject();
      cJSON_AddNumberToObject(entry, "DestRegister", is->destReg);
      cJSON_AddBoolToObject(entry, "OpAIsReady", is->opAIsReady);
      cJSON_AddNumberToObject(entry, "OpARegTag", is->opARegTag);
      //This fixes the rounding issues by directly copying the number in this buffer
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)is->opAValue);
      cJSON_AddItemToObject(entry, "OpAValue", cJSON_CreateRaw(buffer));
      cJSON_AddBoolToObject(entry, "OpBIsReady", is->opBIsReady);
      cJSON_AddNumberToObject(entry, "OpBRegTag", is->opBRegTag);
      snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)is->opBValue);
      cJSON_AddItemToObject(entry, "OpBValue", cJSON_CreateRaw(buffer));
      cJSON_AddStringToObject(entry, "OpCode", strncmp(is->opCode, "addi", 5) ? is->opCode : "add"); // BECAUSE ADD NOT ADDI
      cJSON_AddNumberToObject(entry, "PC", is->pc);
      cJSON_AddItemToArray(iq_array, entry);
    }
    cur = cur->next;
  }
  cJSON_AddItemToObject(root, "IntegerQueue", iq_array);

  cJSON_AddItemToArray(log_array, root);
}

void finalize_logger(const char *filename) {
  if (!log_array) return;

  char *json_str = cJSON_Print(log_array);
  FILE *f = fopen(filename, "w");
  if (f) {
      fputs(json_str, f);
      fclose(f);
  }
  free(json_str);
  cJSON_Delete(log_array);
  log_array = NULL;
}

void main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
    return;
  }

  const char *input_json = argv[1];
  const char *output_json = argv[2];

  program *p = calloc(1, sizeof(program));
  convert_json_into_program(input_json, p);

  reset();
  init_logger();
  append_state_to_log();
  
  int cycle = 0;
  while(!((pc == p->instruction_count || pc == EXCEPTION_ADDRESS) &&  DIR_reg[0] == NULL && active_list.empty)){
    propagate(p);
    append_state_to_log();
  }
  if(exception){
    exception = false;
    append_state_to_log();
  }
  finalize_logger(output_json);
  for(int i = 0; i < 64; i++){
    printf("r%d=%lu\n", i, register_file[i]);
  }
}