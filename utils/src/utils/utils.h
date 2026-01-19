#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <netdb.h>
#include <string.h>
#include "socket.h"
#include "codes.h"
#include <commons/string.h>
#include <readline/readline.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>

t_config *config_init(char *ruta);
t_log *logger_init(char *path, char *process_name, char *log_level);
void free_component(int socket_size, int socket[], t_log *logger, t_config *config_file, void *config_component);

#endif