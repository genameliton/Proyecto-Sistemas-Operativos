#include "worker_resource.h"
#include "comms.h"
#include "scheduler.h"
#include "queries.h"

t_list *workers;
pthread_mutex_t mtx_workers;
sem_t sem_free_workers;

void _handle_interruption(t_worker *worker, t_query *query, int pc, bool is_priority_preemption)
{
    query->pc = pc;

    if (is_priority_preemption)
    {
        log_info(logger, "## Se desaloja la Query %d (%d) del Worker %s - Motivo: PRIORIDAD",
                 query->id, query->priority, worker->worker_id);

        query_change_state(query, READY);
        sem_post(&sem_query_on_queue);
    }
    else
    {
        log_info(logger, "## Se desaloja la Query %d (%d) del Worker %s - Motivo: DESCONEXION",
                 query->id, query->priority, worker->worker_id);
        query_change_state(query, EXIT);
    }
    worker->interrupted = false;
    free_worker(worker);
}

void init_worker_resource()
{
    workers = list_create();
    pthread_mutex_init(&mtx_workers, NULL);
    sem_init(&sem_free_workers, 0, 0);
}

void add_worker(int fd_worker_connection, char *worker_id)
{
    t_worker *worker = malloc(sizeof(t_worker));
    worker->fd_worker_connection = fd_worker_connection;
    worker->worker_id = strdup(worker_id);
    worker->free = true;
    worker->interrupted = false;
    worker->working_query = NULL;

    pthread_mutex_init(&worker->mutex, NULL);

    pthread_mutex_lock(&mtx_workers);
    list_add(workers, worker);
    int total_workers = list_size(workers);
    pthread_mutex_unlock(&mtx_workers);

    log_info(logger, "## Se conecta el Worker %s - Cantidad total de Workers: %d", worker->worker_id, total_workers);

    sem_post(&sem_free_workers);
    sem_post(&sem_replanify);

    pthread_t listener_thread;
    pthread_create(&listener_thread, NULL, (void *)worker_monitor, (void *)worker);
    pthread_detach(listener_thread);
}

t_worker *get_free_worker()
{
    t_worker *found = NULL;
    pthread_mutex_lock(&mtx_workers);
    for (int i = 0; i < list_size(workers); i++)
    {
        t_worker *w = list_get(workers, i);
        if (w->free)
        {
            w->free = false;
            found = w;
            break;
        }
    }
    pthread_mutex_unlock(&mtx_workers);
    return found;
}

t_worker *find_worker_by_query_id(int query_id)
{
    t_worker *found = NULL;
    pthread_mutex_lock(&mtx_workers);
    for (int i = 0; i < list_size(workers); i++)
    {
        t_worker *w = list_get(workers, i);

        pthread_mutex_lock(&w->mutex);

        if (!w->free && w->working_query && w->working_query->id == query_id)
        {
            found = w;
            pthread_mutex_unlock(&w->mutex);
            break;
        }
        pthread_mutex_unlock(&w->mutex);
    }
    pthread_mutex_unlock(&mtx_workers);
    return found;
}

void free_worker(t_worker *worker)
{
    pthread_mutex_lock(&worker->mutex);
    if (!worker->free)
    {
        worker->free = true;
        worker->working_query = NULL;
    }
    pthread_mutex_unlock(&worker->mutex);
    sem_post(&sem_free_workers);
    sem_post(&sem_replanify);
}

void worker_destroyer(void *args)
{
    t_worker *w = (t_worker *)args;
    if (w->worker_id)
        free(w->worker_id);
    pthread_mutex_destroy(&w->mutex);
    free(w);
}

void disconnect_worker(t_worker *worker_to_remove)
{
    bool _is_the_worker(void *elem)
    {
        t_worker *worker = (t_worker *)elem;
        return worker->fd_worker_connection == worker_to_remove->fd_worker_connection;
    }

    pthread_mutex_lock(&worker_to_remove->mutex);
    bool was_free = worker_to_remove->free;
    pthread_mutex_unlock(&worker_to_remove->mutex);

    close(worker_to_remove->fd_worker_connection);
    worker_to_remove->fd_worker_connection = -1;

    pthread_mutex_lock(&mtx_workers);
    list_remove_and_destroy_by_condition(workers, _is_the_worker, worker_destroyer);
    pthread_mutex_unlock(&mtx_workers);

    if (was_free)
        sem_trywait(&sem_free_workers);

    sem_post(&sem_replanify);
}

void *worker_monitor(void *args_ptr)
{
    t_worker *worker = (t_worker *)args_ptr;

    while (1)
    {
        if (worker->fd_worker_connection == -1)
            break;

        t_list *data = receive_packet(worker->fd_worker_connection);

        if (!data)
        {
            t_query *lost_query = worker->working_query;

            pthread_mutex_lock(&mtx_workers);
            int total_restantes = list_size(workers) - 1;
            pthread_mutex_unlock(&mtx_workers);
            if (lost_query != NULL)
            {
                log_info(logger, "## Se desconecta el Worker %s - Se finaliza la Query %d - Cantidad total de Workers: %d",
                         worker->worker_id, lost_query->id, total_restantes);

                query_change_state(lost_query, EXIT);
                query_control_end_notify(lost_query->fd_query_control, "DESCONEXION WORKER");
            }
            else
            {
                log_info(logger, "## Se desconecta el Worker %s - Cantidad total de Workers: %d",
                         worker->worker_id, total_restantes);
            }

            disconnect_worker(worker);
            return NULL;
        }

        QUERY_OP_CODE op_code;
        extract_data(data, 0, &op_code, sizeof(QUERY_OP_CODE));

        t_query *my_query = worker->working_query;

        switch (op_code)
        {
        case QUERY_END:
        {
            STATUS_QUERY_OP_CODE reason;
            extract_data(data, 2, &reason, sizeof(STATUS_QUERY_OP_CODE));

            log_info(logger, "## Se terminó la Query %d en el Worker %s", my_query->id, worker->worker_id);

            query_change_state(my_query, EXIT);

            const char *reason_str = status_query_to_string(reason);
            query_control_end_notify(my_query->fd_query_control, reason_str);

            free_worker(worker);
            break;
        }
        case QUERY_READ:
        {
            char *file_name;
            int buffer_size;
            extract_string(data, 2, &file_name);
            extract_data(data, 3, &buffer_size, sizeof(int));

            char *content = malloc(buffer_size + 1);
            extract_data(data, 4, content, buffer_size);
            content[buffer_size] = '\0';

            log_info(logger, "## Se envía un mensaje de lectura de la Query %d en el Worker %s al Query Control",
                     my_query->id, worker->worker_id);

            query_control_read_notify(my_query->fd_query_control, file_name, content);

            free(file_name);
            free(content);
            break;
        }
        case QUERY_INTERRUPT_PRIORITY:
        case QUERY_INTERRUPT_DISCONNECT:
        {
            int pc;
            extract_data(data, 2, &pc, sizeof(int));
            bool is_priority = (op_code == QUERY_INTERRUPT_PRIORITY);
            _handle_interruption(worker, my_query, pc, is_priority);
            break;
        }

        default:
            log_warning(logger, "Worker %s envió un código de operación desconocido: %d",
                        worker->worker_id, op_code);
            break;
        }
        list_destroy_and_destroy_elements(data, free);
    }
    return NULL;
}