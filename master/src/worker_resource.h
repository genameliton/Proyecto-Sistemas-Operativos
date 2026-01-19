#ifndef WORKER_RESOURCE_H_
#define WORKER_RESOURCE_H_

#include "shared.h"

extern t_list *workers;
extern pthread_mutex_t mtx_workers;
extern sem_t sem_free_workers;

void init_worker_resource();
void add_worker(int fd_worker_connection, char *worker_id);
t_worker *get_free_worker();
t_worker *find_worker_by_query_id(int query_id);
void free_worker(t_worker *worker);
void worker_destroyer(void *args);
void disconnect_worker(t_worker *worker_to_remove);
void *worker_monitor(void *args_ptr);

#endif