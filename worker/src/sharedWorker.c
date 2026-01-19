#include <sharedWorker.h>

char **split_file_tag(const char *original_string)
{
    if (original_string == NULL)
        return NULL;

    const char *separador = strchr(original_string, ':');
    if (separador == NULL)
        return NULL;

    char **result = malloc(2 * sizeof(char *));

    int len_nombre = separador - original_string;
    result[0] = malloc(len_nombre + 1);
    memcpy(result[0], original_string, len_nombre);
    result[0][len_nombre] = '\0';

    result[1] = strdup(separador + 1);
    return result;
}

void free_split_file_tag(char **result)
{
    if (result != NULL)
    {
        free(result[0]);
        free(result[1]);
        free(result);
    }
}

u_int64_t get_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u_int64_t)tv.tv_sec * 1000 + (u_int64_t)tv.tv_usec / 1000; // tiempo en milisegundos
}