#ifndef CODES_H_
#define CODES_H_

typedef enum
{
    CREATE_FILE,
    TRUNCATE_FILE,
    WRITE_FILE,
    READ_FILE,
    CREATE_TAG,
    COMMIT_TAG,
    DELETE_FILE,
    GET_BLOCK_SIZE,
    GET_FILE_TAG_SIZE
} STORAGE_OP_CODE;

typedef enum
{
    QUERY_NEW,
    QUERY_END,
    QUERY_INTERRUPT_PRIORITY,
    QUERY_INTERRUPT_DISCONNECT,
    QUERY_READ
} QUERY_OP_CODE;

typedef enum
{
    SUCCESSFUL_INSTRUCTION,
    SUCCESSFUL_END,
    ERROR_NON_EXISTING_FILE_TAG,   // File / Tag inexistente, en todos las llamadas a storage
    ERROR_FILE_TAG_ALREADY_EXISTS, // File / Tag preexistente
    ERROR_NO_SPACE_AVAILABLE,      // Espacio Insuficiente (sin bloques físicos)
    ERROR_FILE_TAG_IS_COMMITTED,   // Escritura no permitida (Archivo está COMMITED)
    ERROR_OUT_OF_BOUNDS,           // Lectura o escritura fuera de límite
    ERROR_GENERIC = -1
} STATUS_QUERY_OP_CODE;

const char *status_query_to_string(STATUS_QUERY_OP_CODE code);

#endif