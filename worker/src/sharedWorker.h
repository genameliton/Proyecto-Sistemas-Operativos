#ifndef SHAREDWORKER_H
#define SHAREDWORKER_H

#include <utils/utils.h>
#include <worker_comms.h>
#include <sys/time.h>

typedef struct
{
    char *ip_master;
    char *puerto_master;
    char *ip_storage;
    char *puerto_storage;
    int tam_memoria;
    int retardo_memoria;
    char *algoritmo_de_reemplazo;
    char *path_scripts;
    char *log_level;
} t_worker_config;

typedef struct
{
    int pid;
    int pc;
    char *file_name;
} t_context;

typedef enum
{
    CREATE,
    TRUNCATE,
    WRITE,
    READ,
    TAG,
    COMMIT,
    FLUSH,
    DELETE,
    END
} inst_type;

// variables externas:
extern t_log *logger;
extern int storage_socket;
extern int master_socket;
extern t_worker_config *config_worker;
extern int block_size;

char **split_file_tag(const char *original_string);
void free_split_file_tag(char **result);
u_int64_t get_timestamp();

#endif