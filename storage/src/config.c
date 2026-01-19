#include <config.h>

t_log *logger;
t_storage_config *config_storage;
t_config *config_file;
char *system_config_path = NULL;

void init_config(char *path)
{
    system_config_path = path;
    config_file = config_create(system_config_path);

    if (config_file == NULL)
    {
        perror("Error al cargar el archivo de configuración");
        exit(EXIT_FAILURE);
    }

    config_storage = malloc(sizeof(t_storage_config));

    config_storage->puerto_escucha = config_get_string_value(config_file, "PUERTO_ESCUCHA");
    config_storage->fresh_start = (strcmp(config_get_string_value(config_file, "FRESH_START"), "TRUE") == 0) ? 1 : 0;
    config_storage->punto_montaje = config_get_string_value(config_file, "PUNTO_MONTAJE");
    config_storage->retardo_op = config_get_int_value(config_file, "RETARDO_OPERACION");
    config_storage->retardo_accesso_bloque = config_get_int_value(config_file, "RETARDO_ACCESO_BLOQUE");
    config_storage->log_level = config_get_string_value(config_file, "LOG_LEVEL");

    logger = logger_init(LOG_PATH, MODULE_ID, config_storage->log_level);

    log_debug(logger, "Config - Puerto Escucha: %s", config_storage->puerto_escucha);
    log_debug(logger, "Config - Fresh Start: %d", config_storage->fresh_start);
    log_debug(logger, "Config - Punto Montaje: %s", config_storage->punto_montaje);
    log_debug(logger, "Config - Retardo Op: %d", config_storage->retardo_op);
    log_debug(logger, "Config - Retardo acceso bloque: %d", config_storage->retardo_accesso_bloque);
    log_debug(logger, "Config - Log Level: %s", config_storage->log_level);

    log_info(logger, "Config iniciado");
}