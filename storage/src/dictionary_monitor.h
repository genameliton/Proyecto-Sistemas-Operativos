#ifndef DICTIONARY_MONITOR_H
#define DICTIONARY_MONITOR_H

#include <commons/collections/dictionary.h>
#include <pthread.h>
#include <config.h>

extern t_dictionary* file_locks;
extern pthread_mutex_t MUTEX_DICTIONARY;

void init_file_locks(void);
void destroy_mutex_element(void* element);
void destroy_file_locks(void);
void lock_file_tag(char* file_name, char* tag_name);
void unlock_file_tag(char* file_name, char* tag_name);
bool create_lock_file_tag(char* file_name, char* tag_name);
char* get_lock_key(char* file_name, char* tag_name);

#endif