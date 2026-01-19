#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <utils/utils.h>
#include <string.h>

typedef struct
{
    char *puerto_escucha;
    int fresh_start;
    char *punto_montaje;
    int retardo_op;
    int retardo_accesso_bloque;
    char *log_level;
} t_storage_config;

extern t_storage_config *config_storage;
extern t_config *config_file;
extern t_log *logger;

extern char *system_config_path;
void init_config(char *path);

#define LOG_PATH "storage.log"
#define MODULE_ID "STORAGE"

#define PUERTO_ESCUCHA (config_storage->puerto_escucha)
#define FRESH_START (config_storage->fresh_start)
#define PUNTO_MONTAJE (config_storage->punto_montaje)
#define RETARDO_OP (config_storage->retardo_op)
#define RETARDO_BLOQUE (config_storage->retardo_accesso_bloque)

#endif