#ifndef WORKER_H
#define WORKER_H

#include <utils/utils.h>
#include <utils/socket.h> 
#include <sharedWorker.h>
#include <query_interpreter.h>
#include "memory.h"

#define CONFIG_PATH "worker.config"

t_config *config_file;
t_worker_config *config_worker;
char* worker_id; 

void init_worker(char* id, char* config_path); 
void init_worker_sockets();

#endif