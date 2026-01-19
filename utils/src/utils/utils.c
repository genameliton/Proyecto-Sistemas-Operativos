#include "utils.h"

t_config *config_init(char *path)
{
    t_config *config_file;

    config_file = config_create(path);
    if (config_file == NULL)
    {
        printf("No se pudo abrir el archivo de configuración: %s\n", path);
        exit(1);
    }

    return config_file;
}

t_log *logger_init(char *path, char *process_name, char *log_level)
{
    t_log *new_logger;
    t_log_level level = log_level_from_string(log_level);
    if (level == -1)
    {
        printf("Nivel de log invalido!\n");
        exit(1);
    }

    new_logger = log_create(path, process_name, 1, level);
    if (new_logger == NULL)
    {
        printf("No se pudo crear el logger!\n");
        exit(1);
    }

    return new_logger;
}

void free_component(int socket_size, int socket[], t_log *logger, t_config *config_file, void *config_component)
{
    for (int i = 0; i < socket_size; i++)
        free_connection(socket[i]);
    if (logger != NULL)
        log_destroy(logger);
    if (config_file != NULL)
        config_destroy(config_file);
    if (config_component != NULL)
        free(config_component);
}