#ifndef COMMS_H_
#define COMMS_H_

#include "shared.h"

void worker_interrupt_request(int fd_connection, int query_id, QUERY_OP_CODE interrupt_type);
void worker_dispatch_request(t_query *query, t_worker *worker);
void query_control_read_notify(int fd_query_control, char *file_name, char *content);
void query_control_end_notify(int fd_query_control, const char *reason);
void *worker_monitor(void *args_ptr);
void *query_control_monitor(void *args_ptr);

#endif