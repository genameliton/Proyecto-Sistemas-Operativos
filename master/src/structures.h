#ifndef STRUCTURES_H_
#define STRUCTURES_H_

#include <utils/utils.h>
#include <semaphore.h>
#include <pthread.h>

typedef enum
{
    READY,
    EXEC,
    EXIT
} STATES;

typedef enum
{
    FIFO,
    PRIORIDADES,
} SCHED_ALGORITHM;

typedef struct t_worker t_worker;
typedef struct t_query t_query;

struct t_query
{
    int id;
    int fd_query_control;
    char *query_file_path;
    int priority;
    int pc;
    STATES state;
    int arrival_seq;
    pthread_mutex_t mutex;
};

struct t_worker
{
    int fd_worker_connection;
    char *worker_id;
    bool free;
    bool interrupted;
    t_query *working_query;
    pthread_mutex_t mutex;
};

const char *state_to_string(STATES state);
const char *sched_algorithm_to_string(SCHED_ALGORITHM algorithm);
SCHED_ALGORITHM parse_algorithm(const char *algorithm);
void destroy_query_data(t_query *query);

#endif