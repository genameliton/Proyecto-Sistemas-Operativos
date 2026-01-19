#include "master.h"
#include "handler.h"
#include "queries.h"
#include "worker_resource.h"

int fd_server_master;
t_config *config_file;
pthread_t thread_conexiones;
t_master_config *config_master;
t_log *logger;

void init_master(char *config_path)
{
    config_file = config_init(config_path);
    config_master = malloc(sizeof(t_master_config));

    config_master->puerto_escucha = config_get_string_value(config_file, "PUERTO_ESCUCHA");
    config_master->algoritmo_planificacion = config_get_string_value(config_file, "ALGORITMO_PLANIFICACION");
    config_master->tiempo_aging = config_get_int_value(config_file, "TIEMPO_AGING");
    config_master->log_level = config_get_string_value(config_file, "LOG_LEVEL");

    logger = logger_init(LOG_PATH, "MASTER", config_master->log_level);

    initialize_query_lists();
    init_worker_resource();
    init_scheduler();
    start_scheduler();
}

void start_listener_server()
{
    fd_server_master = init_server(config_master->puerto_escucha);
    log_debug(logger, "Servidor Master escuchando en puerto %s", config_master->puerto_escucha);

    t_server_args *args_master = malloc(sizeof(t_server_args));
    args_master->fd_server = fd_server_master;
    args_master->client_handler = handle_client_master;
    pthread_create(&thread_conexiones, NULL, (void *)handle_multiple_connections_server, (void *)args_master);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Uso: %s [archivo_configuracion]\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *config_path = argv[1];

    init_master(config_path);
    start_listener_server();

    pthread_join(thread_conexiones, NULL);
    return 0;
}