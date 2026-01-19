#ifndef MEMORY_H
#define MEMORY_H
#include <sharedWorker.h>

typedef struct
{
    bool present;             // bit de presencia
    bool mod;                 // bit modificado
    bool use;                 // bit de uso
    int page;                 // a que pagina hace referencia
    char *file_tag;           // que FILE:TAG lo tiene asignado
    u_int64_t last_reference; // última referencia
} frame_entry_t;

extern void *memory_ptr;
extern frame_entry_t *frames_table;
// se deberá mantener una tabla de páginas por cada File:Tag de los que tenga al menos una página presente.
// Value del diccionaro -> Array de page_entry_t
extern t_dictionary *file_tag_table;

typedef struct
{
    bool present;
    int frame; // Número de marco físico
    bool mod;
} page_entry_t;

typedef struct
{
    int entries_length;
    page_entry_t *entries;
} page_table;

typedef struct {
    int size;
    void* content;      
    int page_num;    
} physical_segment_t;

typedef struct
{
    physical_segment_t *segments;
    int segments_count;
    bool success;
    STATUS_QUERY_OP_CODE status_code;
} translation_result_t;

typedef struct
{
    int page;
    int offset;
} dir_tuple_t;

void init_memory(int block_size);
void free_memory();

bool truncate_memory(char *file_tag, int size, int query_id);
void *read_memory(char *file_tag, int base_dir, int size, int query_id, STATUS_QUERY_OP_CODE *error_code);
bool write_memory(char *file_tag, int base_dir, void *bytes, int size, int query_id, STATUS_QUERY_OP_CODE *error_code);
STATUS_QUERY_OP_CODE flush_memory(char *file_tag, int query_id);
bool free_file_tag(char *file_tag, int query_id);
bool create_memory(char *file_tag);
void flush_all_file_tags(int query_id);
dir_tuple_t dir_decimal_to_tuple(int decimal_dir, int page_size);

void run_memory_tests();

#endif