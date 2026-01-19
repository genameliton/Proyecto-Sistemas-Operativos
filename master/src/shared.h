#ifndef SHARED_H_
#define SHARED_H_

#include <structures.h>
#include <commons/collections/list.h>

typedef struct
{
    char *puerto_escucha;
    char *algoritmo_planificacion;
    int tiempo_aging;
    char *log_level;
} t_master_config;

extern t_master_config *config_master;
extern t_log *logger;

extern t_list *ready_list;
extern t_list *exec_list;
extern t_list *exit_list;
extern pthread_mutex_t mtx_ready_list;
extern pthread_mutex_t mtx_exec_list;
extern pthread_mutex_t mtx_exit_list;

extern sem_t sem_query_on_queue;
extern sem_t sem_free_workers;
extern sem_t sem_replanify;

#endif