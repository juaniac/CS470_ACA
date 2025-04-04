#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "../include/json_utils.h"
#include "../include/cJSON.h"

int allocated_instructions = 0;

long extract_operand(char *operand) {
  if (operand[0] == 'x') {
    return strtol(operand + 1, NULL, 10); 
  }
  return strtol(operand, NULL, 10);
}

void parse_instruction(const char *line, program *p) {
  if (p->instruction_count >= allocated_instructions) {
    allocated_instructions = (allocated_instructions == 0) ? 10 : allocated_instructions * 2;
    p->instructions = realloc(p->instructions, allocated_instructions * sizeof(program_instruction));
    if (!p->instructions) {
      perror("Memory allocation failed");
      exit(EXIT_FAILURE);
    }
  }

  char temp[50];
  strncpy(temp, line, sizeof(temp) - 1);
  temp[sizeof(temp) - 1] = '\0';

  char *token = strtok(temp, " ,");
  if (!token) return;
  strcpy(p->instructions[p->instruction_count].opcode, token);
    
  token = strtok(NULL, " ,");
  p->instructions[p->instruction_count].dest_reg = token ? extract_operand(token) : 0;
    
  token = strtok(NULL, " ,");
  p->instructions[p->instruction_count].op_A_reg = token ? extract_operand(token) : 0;
    
  token = strtok(NULL, " ,");
  p->instructions[p->instruction_count].op_B_reg = token ? extract_operand(token) : 0;
    
  p->instruction_count += 1;
}

char *read_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    return NULL;
  }
    
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  rewind(file);
    
  char *buffer = malloc(length + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }
    
  fread(buffer, 1, length, file);
  buffer[length] = '\0';
  fclose(file);
  return buffer;
}

void parse_json(char *json_data, program *p) {
  char *ptr = json_data;
  while ((ptr = strchr(ptr, '"')) != NULL) { // Find the next "
    ptr++; // Move past the "
    char *end = strchr(ptr, '"');
    if (!end) break; // If no closing quote, stop
    *end = '\0'; // Null-terminate the instruction
    parse_instruction(ptr, p);
    ptr = end + 1; // Move past the closing "
  }
}

void add_pc_values(program *p){
  for (int i = 0; i < p->instruction_count; i++) {
    p->instructions[i].pc = i;
  }
}

void convert_json_into_program(const char *json_file, program *p){
  char *json_data = read_file(json_file);
  if (!json_data) return;
    
  parse_json(json_data, p);
  free(json_data);

  add_pc_values(p);
}

cJSON *log_array = NULL;

void init_logs() {
  log_array = cJSON_CreateArray();
}

void append_state_to_logs() {
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
      cJSON_AddNumberToObject(entry, "LogicalDestination", is->log_dest);
      cJSON_AddNumberToObject(entry, "OldDestination", is->old_dest);
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
      cJSON_AddNumberToObject(entry, "DestRegister", is->dest_reg);
      cJSON_AddBoolToObject(entry, "OpAIsReady", is->op_a_is_ready);
      cJSON_AddNumberToObject(entry, "OpARegTag", is->op_a_reg_tag);
      //This fixes the rounding issues by directly copying the number in this buffer
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)is->op_a_value);
      cJSON_AddItemToObject(entry, "OpAValue", cJSON_CreateRaw(buffer));
      cJSON_AddBoolToObject(entry, "OpBIsReady", is->op_b_is_ready);
      cJSON_AddNumberToObject(entry, "OpBRegTag", is->op_b_reg_tag);
      snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)is->op_b_value);
      cJSON_AddItemToObject(entry, "OpBValue", cJSON_CreateRaw(buffer));
      cJSON_AddStringToObject(entry, "OpCode", strncmp(is->opcode, "addi", 5) ? is->opcode : "add"); // BECAUSE ADD NOT ADDI
      cJSON_AddNumberToObject(entry, "PC", is->pc);
      cJSON_AddItemToArray(iq_array, entry);
    }
    cur = cur->next;
  }
  cJSON_AddItemToObject(root, "IntegerQueue", iq_array);

  cJSON_AddItemToArray(log_array, root);
}

void convert_logs_to_json(const char *json_file) {
  if (!log_array) return;

  char *json_str = cJSON_Print(log_array);
  FILE *f = fopen(json_file, "w");
  if (f) {
    fputs(json_str, f);
    fclose(f);
  }
  free(json_str);
  cJSON_Delete(log_array);
  log_array = NULL;
}