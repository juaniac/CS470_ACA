typedef struct {
  char opcode[5];
  long dest_reg;
  long op_A_reg;
  long op_B_reg;
  long pc;
} program_instruction;

typedef struct {
program_instruction *instructions;
int instruction_count;
} program;

extern uint64_t register_file[64];
extern bool busy_bit_table[64];
extern int  register_map_table[32];

extern long pc;
extern long epc;
extern bool exception;
#define EXCEPTION_ADDRESS 0x10000

typedef struct {
  int list[32];
  int head; 
  int tail; 
  bool empty;
  bool full;
}int_fifo_list;

extern int_fifo_list free_list;

int pop_free_list();
void push_free_list(int free_reg);

typedef struct{
  bool done;
  bool exception;
  int log_dest;
  int old_dest;
  int pc;
  int dest_reg;
  bool op_a_is_ready;
  int op_a_reg_tag;
  uint64_t op_a_value;
  bool op_b_is_ready;
  int op_b_reg_tag;
  uint64_t op_b_value;
  char opcode[5];
}instruction_state;

typedef struct{
  instruction_state list[32];
  int head; 
  int tail; 
  bool empty;
  bool full;
}instruction_fifo_list;

extern instruction_fifo_list active_list;

void commit_active_list();
void dispatch_active_list(instruction_state **is);
int empty_entries_active_list();
void exception_pop_active_list(instruction_state **is);

typedef struct pipeline_queue{
  struct pipeline_queue *prev;
  instruction_state *is;
  struct pipeline_queue *next;
}pipeline_queue;

extern program_instruction *DIR_reg[4];
extern pipeline_queue *IQ_reg;
extern pipeline_queue *ALU1_reg;
extern pipeline_queue *ALU2_reg;

void insert_is_in_queue(pipeline_queue **queue, instruction_state *is);
void delete_pq_from_queue(pipeline_queue **queue, pipeline_queue *pq);