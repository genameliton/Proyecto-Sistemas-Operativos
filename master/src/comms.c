#include "comms.h"
#include "queries.h"
#include "worker_resource.h"

void worker_interrupt_request(int fd_connection, int query_id, QUERY_OP_CODE interrupt_type)
{
    if (fd_connection == -1)
        return;

    t_packet *packet = create_packet();
    QUERY_OP_CODE op_code = interrupt_type;

    add_to_packet(packet, &op_code, sizeof(QUERY_OP_CODE));
    add_to_packet(packet, &query_id, sizeof(int));

    send_packet(packet, fd_connection);
    destroy_packet(packet);
}

void worker_dispatch_request(t_query *query, t_worker *worker)
{
    pthread_mutex_lock(&worker->mutex);
    if (worker->fd_worker_connection == -1)
    {
        pthread_mutex_unlock(&worker->mutex);
        return;
    }

    worker->working_query = query;

    t_packet *packet = create_packet();
    add_to_packet(packet, &query->id, sizeof(int));
    add_to_packet(packet, &query->pc, sizeof(int));
    add_to_packet(packet, query->query_file_path, strlen(query->query_file_path) + 1);
    send_packet(packet, worker->fd_worker_connection);
    pthread_mutex_unlock(&worker->mutex);
    log_info(logger, "## Se envía la Query %d (%d) al Worker %s", query->id, query->priority, worker->worker_id);
    destroy_packet(packet);
}

void query_control_read_notify(int fd_query_control, char *file_name, char *content)
{
    if (fd_query_control == -1)
        return;

    t_packet *packet = create_packet();
    QUERY_OP_CODE op_code = QUERY_READ;

    add_to_packet(packet, &op_code, sizeof(QUERY_OP_CODE));
    add_to_packet(packet, file_name, strlen(file_name) + 1);
    add_to_packet(packet, content, strlen(content) + 1);

    send_packet(packet, fd_query_control);
    destroy_packet(packet);
}

void query_control_end_notify(int fd_query_control, const char *reason)
{
    if (fd_query_control == -1)
        return;

    t_packet *packet = create_packet();
    QUERY_OP_CODE op_code = QUERY_END;

    add_to_packet(packet, &op_code, sizeof(QUERY_OP_CODE));
    add_to_packet(packet, (void *)reason, strlen(reason) + 1);

    send_packet(packet, fd_query_control);
    destroy_packet(packet);
}