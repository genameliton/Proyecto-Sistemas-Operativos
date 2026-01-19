#include "scheduler.h"
#include "worker_resource.h"
#include "queries.h"
#include "comms.h"

sem_t sem_replanify;
sem_t sem_query_on_queue;
SCHED_ALGORITHM current_algorithm = -1;

void init_scheduler()
{
    sem_init(&sem_replanify, 0, 0);
    sem_init(&sem_query_on_queue, 0, 0);

    char *algorithm = config_master->algoritmo_planificacion;
    current_algorithm = parse_algorithm(algorithm);

    if (current_algorithm == -1)
    {
        log_error(logger, "Algoritmo desconocido.");
        exit(EXIT_FAILURE);
    }
}

void start_scheduler()
{
    pthread_t scheduler_thread;

    if (current_algorithm == FIFO)
        pthread_create(&scheduler_thread, NULL, (void *)fifo, NULL);
    else
        pthread_create(&scheduler_thread, NULL, (void *)priorities, NULL);

    pthread_detach(scheduler_thread);

    if (current_algorithm == PRIORIDADES && config_master->tiempo_aging > 0)
    {
        pthread_t aging_thread;
        pthread_create(&aging_thread, NULL, aging, NULL);
        pthread_detach(aging_thread);
    }
}

void fifo()
{
    log_debug(logger, "Iniciando Planificador FIFO");
    while (1)
    {
        sem_wait(&sem_query_on_queue);
        sem_wait(&sem_free_workers);

        t_worker *worker = get_free_worker();
        t_query *query = NULL;

        pthread_mutex_lock(&mtx_ready_list);
        if (list_size(ready_list) > 0)
            query = list_get(ready_list, 0);
        pthread_mutex_unlock(&mtx_ready_list);

        if (worker && query)
        {
            query_change_state(query, EXEC);
            if (query->state == EXEC)
            {
                pthread_mutex_lock(&worker->mutex);
                worker->working_query = query;
                pthread_mutex_unlock(&worker->mutex);
                worker_dispatch_request(query, worker);
            }
            else
                free_worker(worker);
        }
        else if (worker)
        {
            free_worker(worker);
        }
    }
}

void priorities()
{
    log_debug(logger, "Iniciando Planificador PRIORIDADES (con Desalojo)");
    while (1)
    {
        sem_wait(&sem_replanify);

        pthread_mutex_lock(&mtx_ready_list);
        pthread_mutex_lock(&mtx_workers);

        if (list_size(ready_list) > 0)
        {
            int workers_count = 0;
            sem_getvalue(&sem_free_workers, &workers_count);

            if (workers_count == 0)
            {
                t_query *best_query_ready = list_get(ready_list, 0);
                t_worker *victim_worker = NULL;
                int worst_priority_exec = -1;

                for (int i = 0; i < list_size(workers); i++)
                {
                    t_worker *w = list_get(workers, i);
                    pthread_mutex_lock(&w->mutex);
                    if (w->interrupted)
                    {
                        pthread_mutex_unlock(&w->mutex);
                        continue;
                    }

                    if (!w->free && w->working_query)
                    {
                        int working_query_priority = w->working_query->priority;

                        log_debug(logger, "Comparando Query Running %d (Pri %d) vs Ready %d (Pri %d)",
                                  w->working_query->id, working_query_priority,
                                  best_query_ready->id, best_query_ready->priority);

                        if (working_query_priority > best_query_ready->priority)
                        {
                            if (working_query_priority > worst_priority_exec)
                            {
                                worst_priority_exec = working_query_priority;
                                victim_worker = w;
                            }
                        }
                    }
                    pthread_mutex_unlock(&w->mutex);
                }

                if (victim_worker)
                {
                    pthread_mutex_lock(&victim_worker->mutex);

                    if (!victim_worker->free && victim_worker->working_query && !victim_worker->interrupted)
                    {
                        victim_worker->interrupted = true;

                        worker_interrupt_request(victim_worker->fd_worker_connection,
                                                 victim_worker->working_query->id,
                                                 QUERY_INTERRUPT_PRIORITY);

                        log_debug(logger, "## Enviando interrupción por prioridad al Worker %s (Query %d desalojada por Query %d)",
                                  victim_worker->worker_id, victim_worker->working_query->id, best_query_ready->id);
                    }
                    pthread_mutex_unlock(&victim_worker->mutex);
                }
            }
        }

        pthread_mutex_unlock(&mtx_workers);
        pthread_mutex_unlock(&mtx_ready_list);

        if (sem_trywait(&sem_free_workers) == 0)
        {
            if (sem_trywait(&sem_query_on_queue) == 0)
            {
                t_worker *worker = get_free_worker();
                t_query *query = NULL;

                pthread_mutex_lock(&mtx_ready_list);
                if (list_size(ready_list) > 0)
                    query = list_get(ready_list, 0);
                pthread_mutex_unlock(&mtx_ready_list);

                if (worker && query)
                {
                    query_change_state(query, EXEC);

                    if (query->state == EXEC)
                    {
                        worker->working_query = query;
                        worker_dispatch_request(query, worker);
                    }
                    else
                    {
                        worker->free = true;
                        sem_post(&sem_free_workers);
                    }
                }
                else
                {
                    if (worker)
                    {
                        worker->free = true;
                        sem_post(&sem_free_workers);
                    }
                    if (!query)
                        sem_post(&sem_query_on_queue);
                }
            }
            else
                sem_post(&sem_free_workers);
        }
    }
}

void *aging(void *args)
{
    log_debug(logger, "Iniciando proceso de Aging");
    int tiempo_aging = config_master->tiempo_aging;
    if (tiempo_aging <= 0)
        return NULL;

    while (1)
    {
        usleep(tiempo_aging * 1000);

        bool priority_changed = false;
        pthread_mutex_lock(&mtx_ready_list);
        for (int i = 0; i < list_size(ready_list); i++)
        {
            t_query *q = list_get(ready_list, i);
            int prev_priority = q->priority;
            if (prev_priority > 0)
            {
                q->priority--;
                log_info(logger, "## %d Cambio de prioridad: %d - %d", q->id, prev_priority, q->priority);
                priority_changed = true;
            }
        }

        if (priority_changed)
            list_sort(ready_list, _highest_priority);

        pthread_mutex_unlock(&mtx_ready_list);

        if (priority_changed)
            sem_post(&sem_replanify);
    }
    return NULL;
}