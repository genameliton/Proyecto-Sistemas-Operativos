#include "structures.h"

const char *state_to_string(STATES state)
{
    switch (state)
    {
    case READY:
        return "READY";
    case EXEC:
        return "EXEC";
    case EXIT:
        return "EXIT";
    default:
        return "UNKNOWN_STATE";
    }
}

const char *sched_algorithm_to_string(SCHED_ALGORITHM algorithm)
{
    switch (algorithm)
    {
    case FIFO:
        return "FIFO";
    case PRIORIDADES:
        return "PRIORIDADES";
    default:
        return "UNKNOWN_ALGORITHM";
    }
}

SCHED_ALGORITHM parse_algorithm(const char *algorithm)
{
    if (strcmp(algorithm, "FIFO") == 0)
        return FIFO;
    if (strcmp(algorithm, "PRIORIDADES") == 0)
        return PRIORIDADES;
    return -1;
}

void destroy_query_data(t_query *query)
{
    if (query)
    {
        if (query->query_file_path)
            free(query->query_file_path);
        pthread_mutex_destroy(&query->mutex);
        free(query);
    }
}