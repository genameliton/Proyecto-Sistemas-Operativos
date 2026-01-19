#ifndef STR_COMMS_H
#define STR_COMMS_H

#include <utils/codes.h>
#include <utils/utils.h>
#include <utils/socket.h>
#include <config.h>
#include <filesystem.h>
#include <commons/collections/list.h>
#include <dictionary_monitor.h> 
#include <bitarray_monitor.h>

void* process_comms(void* args);

extern int connected_workers;
extern pthread_mutex_t mutex_connected_workers;

#endif
