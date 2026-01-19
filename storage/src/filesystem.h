#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <bitarray_monitor.h>
#include <directory.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/crypto.h>
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

typedef struct
{
    int fs_size;
    int block_size;
    int total_blocks;
} t_storage_fs;

extern t_storage_fs* fs_cfg;

extern t_config* superblock;
extern t_config* hash_index;
extern pthread_mutex_t MUTEX_HASH_INDEX;

bool init_superblock(void);
bool init_filesystem(void);
bool format_filesystem(void);
bool load_existing_filesystem(void);
void remove_directory_recursive(char* path);

bool init_hash_index(void);
bool add_hash_block(int block_num, char* content);
char* get_block_md5(int block_id);
void remove_block_from_hash_index(int block_id);
char* read_physical_block(int block_id);
void remove_block_from_hash_index_unlocked(int block_id);

#endif