#include <query_interpreter.h>

t_context *receive_context()
{
    t_list *data = receive_packet(master_socket);

    if (data == NULL || list_size(data) < 3)
    {
        if (data != NULL)
        {
            list_destroy_and_destroy_elements(data, free);
        }
        return NULL;
    }

    int data_pid, data_pc;
    char *data_file_name = NULL;

    extract_data(data, 0, &data_pid, sizeof(int));
    extract_data(data, 1, &data_pc, sizeof(int));
    extract_string(data, 2, &data_file_name);

    t_context *context = malloc(sizeof(t_context));
    context->pid = data_pid;
    context->pc = data_pc;
    context->file_name = data_file_name;

    list_destroy_and_destroy_elements(data, free);
    return context;
}

char *build_path_queries(char *file_name, int file_name_size)
{
    if (!file_name)
        return NULL;

    int path_scripts_size = strlen(config_worker->path_scripts);
    int path_queries_size = path_scripts_size + file_name_size + 2;

    char *path_queries = malloc(path_queries_size);

    if (!path_queries)
        return NULL;

    memcpy(path_queries, config_worker->path_scripts, path_scripts_size);
    path_queries[path_scripts_size] = '/';
    memcpy(path_queries + path_scripts_size + 1, file_name, file_name_size);
    path_queries[path_scripts_size + 1 + file_name_size] = '\0';

    return path_queries;
}

t_list *create_list_of_instructions(char *path_queries)
{
    FILE *queries_file = fopen(path_queries, "r");

    if (!queries_file)
    {
        log_error(logger, "No se pudo abrir archivo de Query: %s", path_queries);
        return NULL;
    }

    t_list *list_instructions = list_create();
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, queries_file)) != -1)
    {

        // Sacar el \n si existe
        if (line[read - 1] == '\n')
        {
            line[read - 1] = '\0';
        }

        // Crear copia propia para guardar en la lista
        char *instruction = strdup(line);
        list_add(list_instructions, instruction);
    }
    free(line);
    fclose(queries_file);

    return list_instructions;
}

inst_type get_instruction(char *instr)
{

    if (strcmp(instr, "CREATE") == 0)
        return CREATE;
    if (strcmp(instr, "TRUNCATE") == 0)
        return TRUNCATE;
    if (strcmp(instr, "WRITE") == 0)
        return WRITE;
    if (strcmp(instr, "READ") == 0)
        return READ;
    if (strcmp(instr, "TAG") == 0)
        return TAG;
    if (strcmp(instr, "COMMIT") == 0)
        return COMMIT;
    if (strcmp(instr, "FLUSH") == 0)
        return FLUSH;
    if (strcmp(instr, "DELETE") == 0)
        return DELETE;
    return END;
}

STATUS_QUERY_OP_CODE receive_op_status(t_context context)
{
    t_list *data = receive_packet(storage_socket);

    if (!data || list_size(data) < 1)
    {
        log_error(logger, "PID: %d - fallo recibiendo el estado de la operacion", context.pid);
        if (data)
            list_destroy_and_destroy_elements(data, free);
        return -1;
    }

    STATUS_QUERY_OP_CODE status_op;
    extract_data(data, 0, &status_op, sizeof(STATUS_QUERY_OP_CODE));
    list_destroy_and_destroy_elements(data, free);
    return status_op;
}

// executes:

STATUS_QUERY_OP_CODE execute_CREATE(char *instruction, t_context context)
{
    strtok(instruction, " ");
    char *file_tag_param = strtok(NULL, " ");
    char *name_file = strtok(file_tag_param, ":");
    char *tag = strtok(NULL, ":");

    int name_file_size = string_length(name_file);
    int tag_size = string_length(tag);

    STORAGE_OP_CODE op_code = CREATE_FILE;
    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &context.pid, sizeof(int));
    add_to_packet(packet, name_file, name_file_size + 1);
    add_to_packet(packet, tag, tag_size + 1);
    send_packet(packet, storage_socket);
    destroy_packet(packet);
    //    ERROR_FILE_TAG_ALREADY_EXISTS,    // File / Tag preexistente
    STATUS_QUERY_OP_CODE status_op = receive_op_status(context);
    if (status_op == SUCCESSFUL_INSTRUCTION)
    {
        char *file_tag_combined = malloc(strlen(name_file) + strlen(tag) + 2);
        sprintf(file_tag_combined, "%s:%s", name_file, tag);
        create_memory(file_tag_combined);
        free(file_tag_combined);
    }
    return status_op;
}

STATUS_QUERY_OP_CODE execute_TRUNCATE(char *instruction, t_context context)
{
    strtok(instruction, " ");
    char *file_tag_param = strtok(NULL, " ");
    char *modification_size = strtok(NULL, " ");
    char *name_file = strtok(file_tag_param, ":");
    char *tag = strtok(NULL, ":");

    int new_size = atoi(modification_size);
    int name_file_size = string_length(name_file);
    int tag_size = string_length(tag);

    STORAGE_OP_CODE op_code = TRUNCATE_FILE;
    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &context.pid, sizeof(int));
    add_to_packet(packet, name_file, name_file_size + 1);
    add_to_packet(packet, tag, tag_size + 1);
    add_to_packet(packet, &new_size, sizeof(int));

    send_packet(packet, storage_socket);
    destroy_packet(packet);

    STATUS_QUERY_OP_CODE status_op = receive_op_status(context);
    if (status_op == SUCCESSFUL_INSTRUCTION)
    {
        char *file_tag_combined = malloc(strlen(name_file) + strlen(tag) + 2);
        sprintf(file_tag_combined, "%s:%s", name_file, tag);
        truncate_memory(file_tag_combined, new_size, context.pid);
        free(file_tag_combined);
    }
    return status_op;
}

STATUS_QUERY_OP_CODE execute_TAG(char *instruction, t_context context)
{
    strtok(instruction, " ");
    char *file_tag_origen_param = strtok(NULL, " ");
    char *file_tag_destino_param = strtok(NULL, " ");

    char *name_file_origen = strtok(file_tag_origen_param, ":");
    char *tag_origen = strtok(NULL, ":");

    char *name_file_destino = strtok(file_tag_destino_param, ":");
    char *tag_destino = strtok(NULL, ":");

    int name_file_origen_size = string_length(name_file_origen);
    int tag_origen_size = string_length(tag_origen);
    int name_file_destino_size = string_length(name_file_destino);
    int tag_destino_size = string_length(tag_destino);

    STORAGE_OP_CODE op_code = CREATE_TAG;
    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &context.pid, sizeof(int));
    add_to_packet(packet, name_file_origen, name_file_origen_size + 1);
    add_to_packet(packet, tag_origen, tag_origen_size + 1);
    add_to_packet(packet, name_file_destino, name_file_destino_size + 1);
    add_to_packet(packet, tag_destino, tag_destino_size + 1);
    send_packet(packet, storage_socket);
    destroy_packet(packet);

    STATUS_QUERY_OP_CODE status_op = ERROR_GENERIC;
    int file_size = 0;

    t_list *data = receive_packet(storage_socket);

    if (!data || list_size(data) < 1)
    {
        log_error(logger, "PID: %d - Falló respuesta de TAG desde Storage (Desconexión o Error)", context.pid);
        if (data)
            list_destroy_and_destroy_elements(data, free);
        return ERROR_GENERIC;
    }

    extract_data(data, 0, &status_op, sizeof(STATUS_QUERY_OP_CODE));

    //    ERROR_FILE_TAG_ALREADY_EXISTS,    // File / Tag preexistente
    if (status_op == SUCCESSFUL_INSTRUCTION)
    {
        if (list_size(data) >= 2)
        {
            extract_data(data, 1, &file_size, sizeof(int));
            char *file_tag_combined_dest = malloc(strlen(name_file_destino) + strlen(tag_destino) + 2);
            sprintf(file_tag_combined_dest, "%s:%s", name_file_destino, tag_destino);
            create_memory(file_tag_combined_dest);
            truncate_memory(file_tag_combined_dest, file_size, context.pid);
            free(file_tag_combined_dest);
        }
        else
        {
            log_error(logger, "Storage reportó éxito en TAG pero no envió tamaño");
            status_op = ERROR_GENERIC;
        }
    }
    list_destroy_and_destroy_elements(data, free);
    return status_op;
}

STATUS_QUERY_OP_CODE execute_COMMIT(char *instruction, t_context context)
{

    STATUS_QUERY_OP_CODE flush_result = execute_FLUSH(instruction, context);
    if (flush_result != SUCCESSFUL_INSTRUCTION)
    {
        log_error(logger, "## Query %d: Fallo el FLUSH implicito antes de COMMIT", context.pid);
        return flush_result;
    }

    strtok(instruction, " ");
    char *file_tag_param = strtok(NULL, " ");
    char *name_file = strtok(file_tag_param, ":");
    char *tag = strtok(NULL, ":");

    int name_file_size = string_length(name_file);
    int tag_size = string_length(tag);

    STORAGE_OP_CODE op_code = COMMIT_TAG;
    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &context.pid, sizeof(int));
    add_to_packet(packet, name_file, name_file_size + 1);
    add_to_packet(packet, tag, tag_size + 1);
    send_packet(packet, storage_socket);
    destroy_packet(packet);

    STATUS_QUERY_OP_CODE status_op = receive_op_status(context);
    return status_op;
}

STATUS_QUERY_OP_CODE execute_DELETE(char *instruction, t_context context)
{
    strtok(instruction, " ");
    char *file_tag_param = strtok(NULL, " ");
    char *name_file = strtok(file_tag_param, ":");
    char *tag = strtok(NULL, ":");

    int name_file_size = string_length(name_file);
    int tag_size = string_length(tag);

    STORAGE_OP_CODE op_code = DELETE_FILE;
    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &context.pid, sizeof(int));
    add_to_packet(packet, name_file, name_file_size + 1);
    add_to_packet(packet, tag, tag_size + 1);

    // 3. Enviar el paquete al Storage
    send_packet(packet, storage_socket);
    destroy_packet(packet);

    STATUS_QUERY_OP_CODE status_op = receive_op_status(context);

    if (status_op == SUCCESSFUL_INSTRUCTION)
    {
        log_debug(logger, "## Query %d: DELETE exitoso, liberando recursos de memoria para %s:%s",
                  context.pid, name_file, tag);

        char *file_tag_combined = malloc(strlen(name_file) + strlen(tag) + 2);
        sprintf(file_tag_combined, "%s:%s", name_file, tag);

        bool free_result = free_file_tag(file_tag_combined, context.pid);

        free(file_tag_combined);
        if (!free_result)
        {
            log_debug(logger, "## Query %d: DELETE error, liberando recursos de memoria para %s:%s",
                      context.pid, name_file, tag);
            return ERROR_GENERIC;
        }
    }

    return status_op;
}

STATUS_QUERY_OP_CODE execute_END(char *instruction, t_context context)
{
    // let query interpreter flow handle end send packet, master_send_end
    return SUCCESSFUL_END;
}

STATUS_QUERY_OP_CODE execute_WRITE(char *instruction, t_context context)
{
    // Formato: WRITE <NOMBRE_FILE>:<TAG> <DIRECCIÓN BASE> <CONTENIDO>
    usleep(config_worker->retardo_memoria * 1000);

    char *copy = strdup(instruction);
    strtok(copy, " ");

    char *file_tag = strtok(NULL, " ");
    char *dir_str = strtok(NULL, " ");

    char *content = strtok(NULL, "\n");

    if (!file_tag || !dir_str || !content)
    {
        log_error(logger, "## Query %d: Error de formato en WRITE", context.pid);
        free(copy);
        return -1;
    }

    int base_dir = atoi(dir_str);
    int size = strlen(content);
    STATUS_QUERY_OP_CODE status_code;
    bool result = write_memory(file_tag, base_dir, content, size, context.pid, &status_code);

    free(copy);

    if (result)
    {
        return 0;
    }
    else
    {
        log_error(logger, "## Query %d: Fallo al escribir en memoria", context.pid);
        return -1;
    }
}

STATUS_QUERY_OP_CODE execute_READ(char *instruction, t_context context)
{
    // Formato: READ <NOMBRE_FILE>:<TAG> <DIRECCIÓN BASE> <TAMAÑO>
    usleep(config_worker->retardo_memoria * 1000);

    char *copy = strdup(instruction);
    strtok(copy, " ");

    char *file_tag = strtok(NULL, " ");
    char *dir_str = strtok(NULL, " ");
    char *size_str = strtok(NULL, " ");

    if (!file_tag || !dir_str || !size_str)
    {
        log_error(logger, "## Query %d: Error de formato en READ", context.pid);
        free(copy);
        return -1;
    }

    int base_dir = atoi(dir_str);
    int size = atoi(size_str);

    STATUS_QUERY_OP_CODE status_code;
    void *buffer = read_memory(file_tag, base_dir, size, context.pid, &status_code);

    if (buffer == NULL || status_code != SUCCESSFUL_INSTRUCTION)
    {
        log_error(logger, "## Query %d: Fallo al leer memoria", context.pid);
        free(copy);
        return status_code;
    }

    QUERY_OP_CODE op_code = QUERY_READ;

    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &context.pid, sizeof(int));
    add_to_packet(packet, file_tag, strlen(file_tag) + 1);
    add_to_packet(packet, &size, sizeof(int));
    add_to_packet(packet, buffer, size);

    send_packet(packet, master_socket);
    destroy_packet(packet);

    free(buffer);
    free(copy);

    return SUCCESSFUL_INSTRUCTION;
}

STATUS_QUERY_OP_CODE execute_FLUSH(char *instruction, t_context context)
{
    // Formato: FLUSH <NOMBRE_FILE>:<TAG>

    char *copy = strdup(instruction);
    strtok(copy, " "); // Saltamos "FLUSH"

    char *file_tag = strtok(NULL, " ");

    if (!file_tag)
    {
        log_error(logger, "## Query %d: Error de formato en FLUSH", context.pid);
        free(copy);
        return -1;
    }

    STATUS_QUERY_OP_CODE result = flush_memory(file_tag, context.pid);

    free(copy);
    if (result == SUCCESSFUL_INSTRUCTION)
    {
        return result;
    }
    else
    {
        log_error(logger, "## Query %d: Fallo al realizar FLUSH", context.pid);
        return result;
    }
}

STATUS_QUERY_OP_CODE execute_instruction(char *instruction, t_context context)
{

    // DECODE
    char *copy_instruction = malloc(string_length(instruction) + 1);
    strncpy(copy_instruction, instruction, string_length(instruction) + 1);
    char *intruction_type = strtok(copy_instruction, " ");
    inst_type instruction_name = get_instruction(intruction_type);

    free(copy_instruction);

    STATUS_QUERY_OP_CODE result = -1;

    switch (instruction_name)
    {
    case CREATE:
        result = execute_CREATE(instruction, context);
        return result;
        break;
    case TRUNCATE:
        result = execute_TRUNCATE(instruction, context);
        return result;
        break;
    case WRITE:
        result = execute_WRITE(instruction, context);
        return result;
        break;
    case READ:
        result = execute_READ(instruction, context);
        return result;
        break;
    case TAG:
        result = execute_TAG(instruction, context);
        return result;
        break;
    case COMMIT:
        result = execute_COMMIT(instruction, context);
        return result;
        break;
    case FLUSH:
        result = execute_FLUSH(instruction, context);
        return result;
        break;
    case DELETE:
        result = execute_DELETE(instruction, context);
        return result;
        break;
    case END:
        result = execute_END(instruction, context);
        return result;
        break;
    }
    return result;
}

bool check_interrupt(t_context *context, QUERY_OP_CODE *op_code_status)
{
    t_list *peeked_packet = peek_packet(master_socket);

    if (peeked_packet == NULL)
    {
        return false;
    }

    if (list_size(peeked_packet) < 2)
    {
        list_destroy_and_destroy_elements(peeked_packet, free);
        return false;
    }

    QUERY_OP_CODE op_code;
    int query_id;
    extract_data(peeked_packet, 0, &op_code, sizeof(QUERY_OP_CODE));
    extract_data(peeked_packet, 1, &query_id, sizeof(int));
    *op_code_status = op_code;
    if (op_code == QUERY_INTERRUPT_PRIORITY || op_code == QUERY_INTERRUPT_DISCONNECT)
    {
        t_list *consumed_packet = receive_packet(master_socket);
        if (consumed_packet)
            list_destroy_and_destroy_elements(consumed_packet, free);

        if (query_id == context->pid)
        {
            log_debug(logger, "## Query %d: Interrupción recibida y aceptada", query_id);
            list_destroy_and_destroy_elements(peeked_packet, free);
            return true;
        }
        else
            log_debug(logger, "## Query %d: Interrupción descartada. Era para PID: %d", context->pid, query_id);
    }
    else
        log_error(logger, "[CRITICO] Se detectó OpCode %d en check_interrupt que NO es interrupción.", op_code);

    list_destroy_and_destroy_elements(peeked_packet, free);
    return false;
}

void query_interpreter()
{
    while (true)
    {
        t_context *context = receive_context();
        if (!context)
        {
            log_error(logger, "Error recibiendo contexto");
            break;
        }

        int file_name_size = string_length(context->file_name);
        char *path_queries = build_path_queries(context->file_name, file_name_size);
        if (!path_queries)
        {
            log_error(logger, "## Query %d: Error construyendo path para %s", context->pid, context->file_name);
            free(context->file_name);
            free(context);
            continue;
        }
        log_info(logger, "## Query %d: Se recibe la Query. El path de operaciones es: %s", context->pid, path_queries);

        t_list *list_instructions = create_list_of_instructions(path_queries);

        if (!list_instructions)
        {
            log_error(logger, "## Query %d: No se pudo leer instrucciones", context->pid);
            free(path_queries);
            free(context->file_name);
            free(context);
            continue;
        }
        log_debug(logger, "## Query %d: Se cargaron %d instrucciones", context->pid, list_size(list_instructions));

        bool finished = false;
        STATUS_QUERY_OP_CODE result = 0;

        while (!finished)
        {
            char *instruction = list_get(list_instructions, context->pc);

            if (instruction == NULL)
            {
                log_error(logger, "## Query %d: Error de PC, fuera de límites en PC %d", context->pid, context->pc);
                result = ERROR_GENERIC; // Forzar finalización por error
                break;
            }

            log_info(logger, "## Query %d: FETCH - Program Counter: %d - %s", context->pid, context->pc, instruction);

            result = execute_instruction(instruction, *context);

            if (result == SUCCESSFUL_INSTRUCTION)
            { // Instrucción exitosa (NO es END)
                log_info(logger, "## Query %d: Instrucción realizada: %s", context->pid, instruction);
                context->pc++; // Avanza el PC para la siguiente instrucción
            }
            else if (result == SUCCESSFUL_END)
            { // Instrucción END exitosa
                log_info(logger, "## Query %d: Instrucción realizada: %s", context->pid, instruction);
                context->pc++;
                finished = true; // Finaliza la Query
                master_send_end(master_socket, context->pid, result);
            }
            else
            {
                log_debug(logger, "## Query %d: Finalizada por error al ejecutar instrucción en PC %d", context->pid, context->pc);
                finished = true; // Finaliza la Query por error
                master_send_end(master_socket, context->pid, result);
                break;
            }
            // Hay que hacer check interrupt por si justo llega una interrupcion y tambien finaliza consumir el buffer de recv
            QUERY_OP_CODE interrupt_op_code;
            if (check_interrupt(context, &interrupt_op_code))
            {
                if (finished)
                {
                    log_warning(logger, "## Query %d: Se ignoro la interrupcion porque ya termino la query", context->pid);
                    break;
                }
                log_info(logger, "## Query %d: Desalojada por pedido del Master", context->pid);
                // Hacer FLUSH de todos los File:Tag modificados
                flush_all_file_tags(context->pid);
                master_send_context(master_socket, context->pid, context->pc, interrupt_op_code);
                break;
            }
        }
        list_destroy_and_destroy_elements(list_instructions, free);
        free(path_queries);
        free(context->file_name);
        free(context);
    }
}