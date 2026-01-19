#ifndef QUERIES_H_
#define QUERIES_H_

#include "shared.h"

extern t_list *ready_list;
extern t_list *exec_list;
extern t_list *exit_list;

extern pthread_mutex_t mtx_ready_list;
extern pthread_mutex_t mtx_exec_list;
extern pthread_mutex_t mtx_exit_list;

extern int query_count;

void initialize_query_lists();
bool _highest_priority(void *a, void *b);
void create_query(int fd_query_control, char *query_file_path, int priority);
void query_change_state(t_query *query, STATES new_state);
void *query_control_monitor(void *args_ptr);

#endif