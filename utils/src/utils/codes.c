#include "codes.h"

const char *status_query_to_string(STATUS_QUERY_OP_CODE code)
{
    switch (code)
    {
    case SUCCESSFUL_INSTRUCTION:
        return "INSTRUCCION EXITOSA";
    case SUCCESSFUL_END:
        return "EJECUCION EXITOSA";
    case ERROR_NON_EXISTING_FILE_TAG:
        return "ERROR: EL FILE TAG NO EXISTE";
    case ERROR_FILE_TAG_ALREADY_EXISTS:
        return "ERROR: EL FILE TAG YA EXISTE";
    case ERROR_NO_SPACE_AVAILABLE:
        return "ERROR: NO HAY ESPACIO DISPONIBLE";
    case ERROR_FILE_TAG_IS_COMMITTED:
        return "ERROR: EL FILE TAG ESTA COMMITED";
    case ERROR_OUT_OF_BOUNDS:
        return "ERROR: ACCESO FUERA DE LIMITE";
    case ERROR_GENERIC:
        return "ERROR GENERICO";
    default:
        return "ERROR DESCONOCIDO";
    }
}