#ifndef QUERY_CONTROL_H
#define QUERY_CONTROL_H

#include <utils/utils.h> 
#include <utils/socket.h>
#include <commons/string.h>

typedef struct
{
    char *ip_master;
    char *puerto_master;
    char *log_level;
} t_query_control_config;

t_log *logger;
int master_socket;
t_config *config_file;
t_query_control_config *config_query_control;

extern char *module_id; 
extern char *log_path;

#endif