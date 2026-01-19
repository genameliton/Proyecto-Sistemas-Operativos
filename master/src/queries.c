#include "queries.h"
#include "comms.h"
#include "scheduler.h"
#include "worker_resource.h"

t_list *ready_list;
t_list *exec_list;
t_list *exit_list;

pthread_mutex_t mtx_ready_list;
pthread_mutex_t mtx_exec_list;
pthread_mutex_t mtx_exit_list;

int query_count = 0;
int arrival_counter = 0;

void initialize_query_lists()
{
    ready_list = list_create();
    exec_list = list_create();
    exit_list = list_create();

    pthread_mutex_init(&mtx_ready_list, NULL);
    pthread_mutex_init(&mtx_exec_list, NULL);
    pthread_mutex_init(&mtx_exit_list, NULL);
}

bool _highest_priority(void *a, void *b)
{
    t_query *q1 = (t_query *)a;
    t_query *q2 = (t_query *)b;
    if (q1->priority != q2->priority)
    {
        return q1->priority < q2->priority;
    }
    return q1->arrival_seq < q2->arrival_seq;
}

static void _get_list_and_mutex(STATES state, t_list **target_list, pthread_mutex_t **target_mutex)
{
    switch (state)
    {
    case READY:
        *target_list = ready_list;
        *target_mutex = &mtx_ready_list;
        break;
    case EXEC:
        *target_list = exec_list;
        *target_mutex = &mtx_exec_list;
        break;
    case EXIT:
        *target_list = exit_list;
        *target_mutex = &mtx_exit_list;
        break;
    default:
        *target_list = NULL;
        *target_mutex = NULL;
        break;
    }
}

void create_query(int fd_query_control, char *query_file_path, int priority)
{
    t_query *query = malloc(sizeof(t_query));

    query->fd_query_control = fd_query_control;
    query->query_file_path = strdup(query_file_path);
    query->priority = priority;
    query->state = READY;
    query->pc = 0;
    query->id = query_count++;

    pthread_mutex_init(&query->mutex, NULL);

    pthread_mutex_lock(&mtx_ready_list);
    query->arrival_seq = arrival_counter++;
    SCHED_ALGORITHM alg = parse_algorithm(config_master->algoritmo_planificacion);
    if (alg == PRIORIDADES)
        list_add_sorted(ready_list, query, _highest_priority);
    else
        list_add(ready_list, query);

    pthread_mutex_unlock(&mtx_ready_list);

    pthread_t query_monitor_th;
    pthread_create(&query_monitor_th, NULL, (void *)query_control_monitor, (void *)query);
    pthread_detach(query_monitor_th);

    log_debug(logger, "Nueva Query ID: %d en READY", query->id);
    log_info(logger, "## Se conecta un Query Control para ejecutar la Query %s con prioridad %d - Id asignado: %d. Nivel multiprocesamiento %d", query->query_file_path, query->priority, query->id, list_size(workers));

    sem_post(&sem_query_on_queue);
    sem_post(&sem_replanify);
}

void query_change_state(t_query *query, STATES new_state)
{
    STATES old_state = query->state;
    if (old_state == new_state)
        return;

    t_list *old_list = NULL;
    pthread_mutex_t *old_mutex = NULL;
    t_list *new_list = NULL;
    pthread_mutex_t *new_mutex = NULL;

    _get_list_and_mutex(old_state, &old_list, &old_mutex);
    _get_list_and_mutex(new_state, &new_list, &new_mutex);

    bool removed = false;
    if (old_list && old_mutex)
    {
        pthread_mutex_lock(old_mutex);
        removed = list_remove_element(old_list, query);
        pthread_mutex_unlock(old_mutex);
    }

    if (!removed && old_list != NULL)
    {
        log_warning(logger, "Intento de mover Query %d de %s a %s, pero no estaba en la lista origen.", query->id, state_to_string(old_state), state_to_string(new_state));
        return;
    }

    if (new_state == READY)
    {
        pthread_mutex_lock(&mtx_ready_list);
        query->arrival_seq = arrival_counter++;
        pthread_mutex_unlock(&mtx_ready_list);
    }

    query->state = new_state;

    if (new_list && new_mutex)
    {
        pthread_mutex_lock(new_mutex);
        if (new_state == READY && parse_algorithm(config_master->algoritmo_planificacion) == PRIORIDADES)
            list_add_sorted(new_list, query, _highest_priority);
        else
            list_add(new_list, query);
        pthread_mutex_unlock(new_mutex);
    }

    log_debug(logger, "## Query %d cambia de estado: %s -> %s", query->id, state_to_string(old_state), state_to_string(new_state));
}

void *query_control_monitor(void *args_ptr)
{
    t_query *query = (t_query *)args_ptr;
    char buffer[1];
    int result = recv(query->fd_query_control, buffer, 1, MSG_PEEK | MSG_WAITALL);

    if (result <= 0)
    {
        log_info(logger, "## Se desconecta un Query Control. Se finaliza la Query %d con prioridad %d. Nivel multiprocesamiento %d",
                 query->id,
                 query->priority,
                 list_size(workers));
        bool handled = false;
        while (!handled)
        {
            pthread_mutex_lock(&query->mutex);
            STATES curr_state = query->state;
            pthread_mutex_unlock(&query->mutex);

            switch (curr_state)
            {
            case READY:
                query_change_state(query, EXIT);
                if (query->state == EXIT)
                    handled = true;
                break;
            case EXEC:
            {
                t_worker *w = find_worker_by_query_id(query->id);
                if (w)
                {
                    pthread_mutex_lock(&w->mutex);
                    if (!w->free && w->working_query && w->working_query->id == query->id)
                    {
                        w->interrupted = true;
                        worker_interrupt_request(w->fd_worker_connection, query->id, QUERY_INTERRUPT_DISCONNECT);
                    }
                    else
                        query_change_state(query, EXIT);

                    pthread_mutex_unlock(&w->mutex);
                    handled = true;
                }
                else
                {
                    query_change_state(query, EXIT);
                    handled = true;
                }
                break;
            }
            case EXIT:
                log_debug(logger, "## El Query Control de la Query %d se desconectó correctamente tras finalizar.", query->id);
                handled = true;
                break;
            }
            if (!handled)
                usleep(1000);
        }

        if (query->fd_query_control != -1)
        {
            close(query->fd_query_control);
            query->fd_query_control = -1;
        }
    }
    return NULL;
}