#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <config.h>
#include <filesystem.h>
#include <sys/stat.h>  // Para mkdir()
#include <unistd.h> 
#include <errno.h> 

typedef enum
{
    WORK_IN_PROGRESS,
    COMMITED
} FILE_STATE;

char* parse_file_state(FILE_STATE state);
void create_metadata_file(char* tag_path, int size, char* blocks_list, FILE_STATE state);
bool create_directory_structure(void);
bool create_initial_file(void);
bool create_physical_blocks(void);
char* get_physical_block_path(int block_num);

char* get_file_path(char* file_name, char* tag_name);
char* get_metadata_path(char* file_name, char* tag_name);
char* get_logical_block_path(char* file_name, char* tag_name, int block_num);
void save_metadata_file(t_config* metadata, char* metadata_path, int new_size, char** blocks_list);

t_list* config_array_to_list(char** array);
char* list_to_config_string(t_list* list);

#endif