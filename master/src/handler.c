#include "handler.h"
#include "worker_resource.h"
#include "scheduler.h"
#include "queries.h"

void *handle_client_master(void *arg)
{
    int fd_connection = *((int *)arg);
    free(arg);
    char *module_id = NULL;
    int result = receive_handshake(fd_connection, MASTER_HANDSHAKE, &module_id);

    if (result == HANDSHAKE_OK)
    {
        log_debug(logger, "Módulo conectado: %s", module_id);
        if (string_starts_with(module_id, "QUERY_CONTROL"))
        {
            t_list *query_data = receive_packet(fd_connection);
            char *query_path;
            int priority;
            extract_string(query_data, 1, &query_path);
            extract_data(query_data, 2, &priority, sizeof(int));
            create_query(fd_connection, query_path, priority);
            list_destroy_and_destroy_elements(query_data, free);
            free(query_path);
        }
        else
        {
            add_worker(fd_connection, module_id);
        }
    }
    else
    {
        log_error(logger, "Error en handshake con cliente (fd %d)", fd_connection);
        free_connection(fd_connection);
    }

    free(module_id);
    return NULL;
}