#include "storage.h"

int fd_server_storage;
pthread_t thread_conexiones;

void start_listener_server()
{
    fd_server_storage = init_server(PUERTO_ESCUCHA);
    log_info(logger, "Servidor Storage escuchando en puerto %s", PUERTO_ESCUCHA);

    t_server_args *args_storage = malloc(sizeof(t_server_args));
    args_storage->fd_server = fd_server_storage;
    args_storage->client_handler = process_comms;
    pthread_create(&thread_conexiones, NULL, (void *)handle_multiple_connections_server, (void *)args_storage);
}

void init_storage(char *config_path)
{
    init_config(config_path);

    if (!init_superblock())
    {
        log_error(logger, "Fallo crítico: No se pudo inicializar el superblock. Abortando.");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&mutex_connected_workers, NULL);
    init_file_locks();  // Debe estar antes de init_filesystem
    
    init_filesystem();
}

void destroy_config()
{
    if (config_file)
        config_destroy(config_file);
    if (config_storage)
        free(config_storage);
    if (logger)
        log_destroy(logger);
}

void destroy_storage()
{
    log_warning(logger, "Destruyendo Storage");
    if (block_bitmap)
    {
        sync_bitmap();
        destroy_block_bitmap();
    }
    if (superblock)
        config_destroy(superblock);
    if (hash_index)
        config_destroy(hash_index);
    if (fs_cfg)
        free(fs_cfg);

    pthread_mutex_destroy(&mutex_connected_workers);
    pthread_mutex_destroy(&MUTEX_HASH_INDEX);
    destroy_file_locks();
    destroy_config();
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso: %s [archivo_config]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *config_path = argv[1];

    init_storage(config_path);
    start_listener_server();

    // // Esperar input del usuario para mantener el servidor activo
    // char* line = NULL;
    // while ((line = readline("")) != NULL)
    // {
    //     if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
    //     {
    //         free(line);
    //         break;
    //     }
    //     free(line);
    // }

    pthread_join(thread_conexiones, NULL);

    destroy_storage();

    return 0;
}