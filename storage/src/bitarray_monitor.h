#ifndef BITARRAY_MONITOR_STRG_H
#define BITARRAY_MONITOR_STRG_H

#include <commons/bitarray.h>
#include <config.h>
#include <filesystem.h>

extern t_bitarray* block_bitmap;
extern pthread_mutex_t MUTEX_BLOCK_BITMAP;

void init_block_bitmap(int fs_size, int block_size);
void destroy_block_bitmap(void);
void load_bitmap(void);
void sync_bitmap(void);
void mark_physical_block_as_free(uint32_t block_number);
void mark_physical_block_as_occupied(uint32_t block_number);
int get_free_block(void);

#endif