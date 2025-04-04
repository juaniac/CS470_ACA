#include "data_structures.h"

void convert_json_into_program(const char *json_file, program *p);
void init_logs(); 
void append_state_to_logs();
void convert_logs_to_json(const char *json_file);