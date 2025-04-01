#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char operation[5];
    long opA;
    long opB;
    long opC;
} program_instruction;

typedef struct {
  program_instruction *instructions;
  int instruction_count;
} program;

int allocated_instructions = 0;

long extract_operand(char *operand) {
    if (operand[0] == 'x') {
        return strtol(operand + 1, NULL, 10); 
    }
    return strtol(operand, NULL, 10);
}

// Function to parse a single instruction
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
    strcpy(p->instructions[p->instruction_count].operation, token);
    
    token = strtok(NULL, " ,");
    p->instructions[p->instruction_count].opA = token ? extract_operand(token) : 0;
    
    token = strtok(NULL, " ,");
    p->instructions[p->instruction_count].opB = token ? extract_operand(token) : 0;
    
    token = strtok(NULL, " ,");
    p->instructions[p->instruction_count].opC = token ? extract_operand(token) : 0;
    
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

void print_program(program *p) {
    for (int i = 0; i < p->instruction_count; i++) {
        printf("%s %ld, %ld, %ld\n", p->instructions[i].operation, p->instructions[i].opA, p->instructions[i].opB, p->instructions[i].opC);
    }
}

int convert_json_into_program(const char *json_file, program *p){
    char *json_data = read_file(json_file);
    if (!json_data) return 1;
    
    parse_json(json_data, p);
    free(json_data);
    
    print_program(p);
    return 0;
}