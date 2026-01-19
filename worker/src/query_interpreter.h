#ifndef QUERY_INTERPRETER_H
#define QUERY_INTERPRETER_H

#include <sharedWorker.h>
#include <memory.h>

t_context *receive_context(); 
char* build_path_queries(char* file_name, int file_name_size);
t_list* create_list_of_instructions(char* path_queries);
inst_type get_instruction(char* instr);
int receive_op_status(t_context context); 
int execute_instruction(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_CREATE(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_TRUNCATE(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_TAG(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_COMMIT(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_DELETE(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_END(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_WRITE(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_READ(char* instruction, t_context context);
STATUS_QUERY_OP_CODE execute_FLUSH(char* instruction, t_context context);
void query_interpreter(); 


#endif