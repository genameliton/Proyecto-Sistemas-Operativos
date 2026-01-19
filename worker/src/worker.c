#include <worker.h>
t_log *logger;
int storage_socket, master_socket;

void init_worker(char *id, char *config_path)
{
    worker_id = strdup(id);

    config_file = config_init(config_path);
    config_worker = malloc(sizeof(t_worker_config));

    config_worker->ip_master = config_get_string_value(config_file, "IP_MASTER");
    config_worker->puerto_master = config_get_string_value(config_file, "PUERTO_MASTER");
    config_worker->ip_storage = config_get_string_value(config_file, "IP_STORAGE");
    config_worker->puerto_storage = config_get_string_value(config_file, "PUERTO_STORAGE");
    config_worker->tam_memoria = config_get_int_value(config_file, "TAM_MEMORIA");
    config_worker->retardo_memoria = config_get_int_value(config_file, "RETARDO_MEMORIA");
    config_worker->algoritmo_de_reemplazo = config_get_string_value(config_file, "ALGORITMO_REEMPLAZO");
    config_worker->path_scripts = config_get_string_value(config_file, "PATH_SCRIPTS");
    config_worker->log_level = config_get_string_value(config_file, "LOG_LEVEL");

    char log_filename[64];
    snprintf(log_filename, sizeof(log_filename), "%s.log", worker_id);

    logger = logger_init(log_filename, "worker", config_worker->log_level);
    log_info(logger, "Iniciado el Worker ID %s", worker_id);
}

void init_worker_sockets()
{
    storage_socket = create_connection(config_worker->ip_storage, config_worker->puerto_storage, logger);
    start_handshake(storage_socket, worker_id, STORAGE_HANDSHAKE, logger);

    master_socket = create_connection(config_worker->ip_master, config_worker->puerto_master, logger);
    start_handshake(master_socket, worker_id, MASTER_HANDSHAKE, logger);
    block_size = storage_get_block_size();
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Uso: %s [archivo_configuracion] [worker_id]\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *config_path = argv[1];
    char *id = argv[2];

    init_worker(id, config_path);
    init_worker_sockets();
    init_memory(block_size);

    query_interpreter();

    // run_memory_tests();

    free_memory();
    return 0;
}
