#ifndef MASTER_H
#define MASTER_H

#define MODULE_ID "MASTER"
#define LOG_PATH "master.log"

#include "shared.h"
#include "scheduler.h"

void init_master(char *config_path);
void start_listener_server();

#endif