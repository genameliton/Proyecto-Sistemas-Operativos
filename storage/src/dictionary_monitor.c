#include <dictionary_monitor.h>

t_dictionary* file_locks;
pthread_mutex_t MUTEX_DICTIONARY;

void init_file_locks(void)
{
    file_locks = dictionary_create();
    pthread_mutex_init(&MUTEX_DICTIONARY, NULL);
}

void destroy_mutex_element(void* element)
{
    pthread_mutex_t* mutex = (pthread_mutex_t*) element;
    pthread_mutex_destroy(mutex);
    free(mutex);
}

void destroy_file_locks(void)
{
    pthread_mutex_lock(&MUTEX_DICTIONARY);
    dictionary_destroy_and_destroy_elements(file_locks, destroy_mutex_element);
    pthread_mutex_unlock(&MUTEX_DICTIONARY);
    pthread_mutex_destroy(&MUTEX_DICTIONARY);
}

char* get_lock_key(char* file_name, char* tag_name)
{
    return string_from_format("%s:%s", file_name, tag_name);
}

void lock_file_tag(char* file_name, char* tag_name)
{
    char* key = get_lock_key(file_name, tag_name);
    pthread_mutex_t* file_mutex;

    pthread_mutex_lock(&MUTEX_DICTIONARY);
    
    if (dictionary_has_key(file_locks, key))
    {
        log_debug(logger, "Se adquiere el lock para el archivo %s", key);
        file_mutex = dictionary_get(file_locks, key);
    }
    else
    {
        log_error(logger, "No se encontro el lock para el archivo %s", key);
        file_mutex = NULL;
    }

    pthread_mutex_unlock(&MUTEX_DICTIONARY);
    
    if (file_mutex)
    {
        pthread_mutex_lock(file_mutex);
    }

    free(key);
}

void unlock_file_tag(char* file_name, char* tag_name)
{
    char* key = get_lock_key(file_name, tag_name);
    pthread_mutex_t* file_mutex;

    pthread_mutex_lock(&MUTEX_DICTIONARY);
    file_mutex = dictionary_get(file_locks, key);
    pthread_mutex_unlock(&MUTEX_DICTIONARY);

    if (file_mutex)
    {
        log_debug(logger, "Se libera el lock para el archivo %s", key);
        pthread_mutex_unlock(file_mutex);
    }
    else
    {
        log_error(logger, "No se encontro el lock para el archivo %s", key);
    }

    free(key);
}

bool create_lock_file_tag(char* file_name, char* tag_name)
{
    char* key = get_lock_key(file_name, tag_name);
    pthread_mutex_t* file_mutex;
    bool status = true;

    pthread_mutex_lock(&MUTEX_DICTIONARY);
    
    if (!dictionary_has_key(file_locks, key))
    {
        log_debug(logger, "Se crea el lock para el archivo %s", key);
        file_mutex = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(file_mutex, NULL);
        dictionary_put(file_locks, key, file_mutex);
    }
    else
    {
        log_error(logger, "El archivo %s ya tiene un lock", key);
        status = false;
    }

    pthread_mutex_unlock(&MUTEX_DICTIONARY);

    free(key);
    return status;
}