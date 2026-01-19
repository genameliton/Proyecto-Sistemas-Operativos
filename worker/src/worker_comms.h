#ifndef WORKER_COMMS_H
#define WORKER_COMMS_H

#include <utils/codes.h>
#include <stdbool.h>

int storage_get_block_size();
STATUS_QUERY_OP_CODE storage_write(char* file_tag, int block,void* block_content, int query_id);
void* storage_read(char* file_tag, int block, STATUS_QUERY_OP_CODE* status_code, int query_id);
int storage_get_file_tag_size(char* file_tag, STATUS_QUERY_OP_CODE* status_code, int query_id);
void master_send_context(int fd_connection, int query_id, int pc, QUERY_OP_CODE interrupt_op_code);
void master_send_end(int fd_connection, int query_id, STATUS_QUERY_OP_CODE status_code);

#endif
