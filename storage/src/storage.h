#ifndef STORAGE_H
#define STORAGE_H

#include <utils/utils.h> 
#include <utils/socket.h>
#include <config.h>
#include <storage_comms.h>
#include <bitarray_monitor.h>
#include <filesystem.h>
#include <directory.h>
#include <dictionary_monitor.h>

extern int fd_server_storage;
extern pthread_t thread_conexiones;

void init_storage(char* config_path);

void init_query_control_socket();

#endif