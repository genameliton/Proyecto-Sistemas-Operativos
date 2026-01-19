#include <storage_comms.h>

int connected_workers = 0;
pthread_mutex_t mutex_connected_workers;

void send_status_response(int fd, STATUS_QUERY_OP_CODE status)
{
    t_packet* response = create_packet();
    add_to_packet(response, &status, sizeof(STATUS_QUERY_OP_CODE));
    send_packet(response, fd);
    destroy_packet(response);
}

void* process_comms(void* args)
{
    int fd_connection = *((int*) args);
    free(args);

    char* module_id = NULL;
    int result = receive_handshake(fd_connection, STORAGE_HANDSHAKE, &module_id);

    if (result != HANDSHAKE_OK)
    {
        log_error(logger, "Error en el handshake con el cliente %s", module_id);
        if (module_id) free(module_id);
        close(fd_connection);
        return NULL;
    }

    pthread_mutex_lock(&mutex_connected_workers);
    connected_workers++;
    log_info(logger, "##Se conecta el Worker %s - Cantidad de Workers: %d", module_id, connected_workers);
    pthread_mutex_unlock(&mutex_connected_workers);

    while (1)
    {
        t_list* data = receive_packet(fd_connection);

        if (!data || list_size(data) == 0)
        {
            log_warning(logger, "Conexión cerrada por el cliente %s", module_id);
            break;
        }

        STORAGE_OP_CODE operation_code;
        extract_data(data, 0, &operation_code, sizeof(STORAGE_OP_CODE));

        switch (operation_code)
        {
            case CREATE_FILE:
            {
                usleep(RETARDO_OP * 1000);

                uint32_t query_id;
                char* file_name = NULL;
                char* tag_name = NULL;

                extract_data(data, 1, &query_id, sizeof(uint32_t));
                extract_string(data, 2, &file_name);
                extract_string(data, 3, &tag_name);

                log_debug(logger, "Ejecutando CREATE_FILE de %s:%s", file_name, tag_name);

                char* file_path = string_from_format("%s/files/%s", PUNTO_MONTAJE, file_name);
                char* tag_path = string_from_format("%s/%s", file_path, tag_name);
                char* logical_blocks_path = string_from_format("%s/logical_blocks", tag_path);
                char* metadata_path = string_from_format("%s/metadata.config", tag_path);

                if (access(file_path, F_OK) == 0)
                {
                    log_error(logger, "El File %s ya existe", file_name);
                    send_status_response(fd_connection, ERROR_FILE_TAG_ALREADY_EXISTS);
                    free(file_path); free(tag_path); free(logical_blocks_path); free(metadata_path);
                    break;
                }

                bool result = create_lock_file_tag(file_name, tag_name);

                if (!result)
                {
                    log_error(logger, "El File:Tag %s:%s ya tenía un lock asociado.", file_name, tag_name);
                    send_status_response(fd_connection, ERROR_FILE_TAG_ALREADY_EXISTS);
                    free(file_path); free(tag_path); free(logical_blocks_path); free(metadata_path);
                    break;
                }
                
                lock_file_tag(file_name, tag_name);

                mkdir(file_path, 0777);

                if (mkdir(tag_path, 0777) != 0)
                {
                    log_error(logger, "Error creando directorio de Tag %s", tag_name);
                    send_status_response(fd_connection, ERROR_GENERIC);
                    unlock_file_tag(file_name, tag_name);
                    free(file_path); free(tag_path); free(logical_blocks_path); free(metadata_path);
                    break;
                }

                if (mkdir(logical_blocks_path, 0777) != 0)
                {
                    log_error(logger, "Error creando directorio logical_blocks");
                    send_status_response(fd_connection, ERROR_GENERIC);
                    unlock_file_tag(file_name, tag_name);
                    free(file_path); free(tag_path); free(logical_blocks_path); free(metadata_path);
                    break;
                }

                create_metadata_file(tag_path, 0, "[]", WORK_IN_PROGRESS);

                log_info(logger, "##%d - File Creado %s:%s", query_id, file_name, tag_name);
                send_status_response(fd_connection, SUCCESSFUL_INSTRUCTION);
                unlock_file_tag(file_name, tag_name);

                free(file_path); free(tag_path); free(logical_blocks_path); free(metadata_path);

                break;
            }
            case TRUNCATE_FILE:
            {
                usleep(RETARDO_OP * 1000);

                uint32_t query_id;
                char* file_name = NULL;
                char* tag_name = NULL;
                int new_size;

                extract_data(data, 1, &query_id, sizeof(uint32_t));
                extract_string(data, 2, &file_name);
                extract_string(data, 3, &tag_name);
                extract_data(data, 4, &new_size, sizeof(int));

                char* file_path = string_from_format("%s/files/%s", PUNTO_MONTAJE, file_name);
                char* tag_path = string_from_format("%s/%s", file_path, tag_name);

                log_debug(logger, "Ejecutando TRUNCATE_FILE de %s:%s", file_name, tag_name);

                if (access(file_path, F_OK) != 0)
                {
                    log_error(logger, "Error en TRUNCATE_FILE - El File %s no existe", file_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path); free(tag_path);
                    break;
                }

                if (access(tag_path, F_OK) != 0)
                {
                    log_error(logger, "Error en TRUNCATE_FILE - El File:Tag %s:%s no existe", file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path); free(tag_path);
                    break;
                }

                if (new_size % fs_cfg->block_size != 0)
                {
                    log_error(logger, "Error en TRUNCATE_FILE - El nuevo tamaño debe ser múltiplo de %d", fs_cfg->block_size);
                    send_status_response(fd_connection, ERROR_OUT_OF_BOUNDS);
                    break;
                }

                char* path_metadata = get_metadata_path(file_name, tag_name);
                
                if (access(path_metadata, F_OK) != 0)
                {
                    log_error(logger, "Error en TRUNCATE_FILE - No se pudo acceder al archivo metadata.config");
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(path_metadata);
                    break;
                }

                lock_file_tag(file_name, tag_name);

                t_config* metadata = config_create(path_metadata);

                char* estado = config_get_string_value(metadata, "ESTADO");
                if (strcmp(estado, "COMMITED") == 0)
                {
                    config_destroy(metadata);
                    free(path_metadata);
                    log_error(logger,"Error en TRUNCATE_FILE - El File:Tag %s:%s ya se encuentra en estado COMMITED", file_name, tag_name);
                    send_status_response(fd_connection, ERROR_FILE_TAG_IS_COMMITTED);
                    unlock_file_tag(file_name, tag_name);
                    break;
                }

                int current_size = config_get_int_value(metadata, "TAMAÑO");
                char** blocks = config_get_array_value(metadata, "BLOCKS");
                t_list* blocks_list = config_array_to_list(blocks);
                string_array_destroy(blocks);  // Liberar el array devuelto por config

                int current_blocks_count = current_size / fs_cfg->block_size;
                int new_blocks_count = new_size / fs_cfg->block_size;

                if (new_blocks_count > current_blocks_count)
                {
                    char* physical_zero_path = get_physical_block_path(0);
                    
                    for (int i = current_blocks_count; i < new_blocks_count; i++)
                    {
                        char* logical_block_path = get_logical_block_path(file_name, tag_name, i);
                        link(physical_zero_path, logical_block_path);
                        log_info(logger, "##%d - %s:%s Se agregó el hard link del bloque lógico %d al bloque físico 0", query_id, file_name, tag_name, i);
                        free(logical_block_path);

                        list_add(blocks_list, strdup("0"));
                    }
                    free(physical_zero_path);
                }

                else if (new_blocks_count < current_blocks_count)
                {
                    int blocks_to_remove = current_blocks_count - new_blocks_count;

                    for (int i = 0; i < blocks_to_remove; i++)
                    {
                        int last_index = list_size(blocks_list) - 1;
                        int physical_id = atoi(list_get(blocks_list, last_index));

                        char* physical_block_path = get_physical_block_path(physical_id);
                        
                        char* logical_block_path = get_logical_block_path(file_name, tag_name, i);
                        unlink(logical_block_path);
                        log_info(logger, "##%d - %s:%s Se eliminó el hard link del bloque lógico %d al bloque físico %d", query_id, file_name, tag_name, i, physical_id);
                        free(logical_block_path);

                        struct stat st;
                        stat(physical_block_path, &st);
                        
                        if (st.st_nlink == 1)
                        {
                            remove_block_from_hash_index(physical_id);
                            mark_physical_block_as_free(physical_id);
                            log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d", query_id, physical_id);
                        }

                        free(physical_block_path);
                        list_remove_and_destroy_element(blocks_list, last_index, free);
                    }
                }

                char* new_blocks_string = list_to_config_string(blocks_list);

                config_set_value(metadata, "BLOCKS", new_blocks_string);
                char* size_string = string_itoa(new_size);
                config_set_value(metadata, "TAMAÑO", size_string);
                free(size_string);
                config_save(metadata);
                free(new_blocks_string);
                unlock_file_tag(file_name, tag_name);
                
                // Liberar memoria
                list_destroy_and_destroy_elements(blocks_list, free);
                config_destroy(metadata);
                free(path_metadata);

                log_info(logger, "##%d - File Truncado %s:%s - Tamaño: %d", query_id, file_name, tag_name, new_size);
                send_status_response(fd_connection, SUCCESSFUL_INSTRUCTION);

                free(file_name); free(tag_name);

                break;
            }
            case CREATE_TAG:
            {
                usleep(RETARDO_OP * 1000);
                uint32_t query_id;
                char* file_src = NULL;
                char* tag_src = NULL;
                char* file_dst = NULL;
                char* tag_dst = NULL;

                extract_data(data, 1, &query_id, sizeof(uint32_t));
                extract_string(data, 2, &file_src);
                extract_string(data, 3, &tag_src);
                extract_string(data, 4, &file_dst);
                extract_string(data, 5, &tag_dst);

                log_debug(logger, "Ejecutando CREATE_TAG de %s:%s a %s:%s", file_src, tag_src, file_dst, tag_dst);

                char* path_metadata_src = get_metadata_path(file_src, tag_src);

                char* file_src_path = string_from_format("%s/files/%s", PUNTO_MONTAJE, file_src);
                char* tag_src_path = string_from_format("%s/%s", file_src_path, tag_src);
        
                if (access(file_src_path, F_OK) != 0)
                {
                    log_error(logger, "Error en CREATE_TAG - El File %s no existe", file_src);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_src_path); free(tag_src_path);
                    break;
                }

                

                if (access(tag_src_path, F_OK) != 0)
                {
                    log_error(logger, "Error en CREATE_TAG - El File:Tag %s:%s no existe", file_src, tag_src);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_src_path); free(tag_src_path);
                    break;
                }

                

                char* path_file_dst = string_from_format("%s/files/%s", PUNTO_MONTAJE, file_dst);
                char* path_tag_dst = string_from_format("%s/%s", path_file_dst, tag_dst);

                if (access(path_tag_dst, F_OK) == 0)
                {
                    log_error(logger, "Error en CREATE_TAG - El File:Tag %s:%s ya existe", file_dst, tag_dst);
                    free(path_metadata_src);
                    free(file_src_path); free(tag_src_path);
                    free(path_file_dst);
                    free(path_tag_dst);
                    send_status_response(fd_connection, ERROR_FILE_TAG_ALREADY_EXISTS);
                    break;
                }

                bool result = create_lock_file_tag(file_dst, tag_dst);
                if (!result)
                {
                    log_error(logger, "Error en CREATE_TAG - No se pudo crear el lock del File:Tag %s:%s", file_dst, tag_dst);
                    send_status_response(fd_connection, ERROR_FILE_TAG_ALREADY_EXISTS);
                    free(path_metadata_src);
                    free(file_src_path); free(tag_src_path);
                    free(path_file_dst);
                    free(path_tag_dst);
                    break;
                }

                // Evita deadlock usando comparación de keys completas file:tag
                char* key_src = get_lock_key(file_src, tag_src);
                char* key_dst = get_lock_key(file_dst, tag_dst);
                
                // Guardar si son el mismo file:tag para unlock condicional después
                bool same_file_tag = (strcmp(key_src, key_dst) == 0);
                
                if (strcmp(key_src, key_dst) < 0)
                {
                    lock_file_tag(file_src, tag_src);
                    lock_file_tag(file_dst, tag_dst);
                }
                else if (strcmp(key_src, key_dst) > 0)
                {
                    lock_file_tag(file_dst, tag_dst);
                    lock_file_tag(file_src, tag_src);
                }
                else
                {
                    // Mismo file:tag, solo hacer lock una vez
                    lock_file_tag(file_src, tag_src);
                }
                
                free(key_src);
                free(key_dst);

                mkdir(path_file_dst, 0777);
                mkdir(path_tag_dst, 0777);

                char* path_logical_dst = string_from_format("%s/logical_blocks", path_tag_dst);
                mkdir(path_logical_dst, 0777);

                t_config* metadata_src = config_create(path_metadata_src);
                int size = config_get_int_value(metadata_src, "TAMAÑO");
                char** blocks = config_get_array_value(metadata_src, "BLOCKS");
                t_list* blocks_list = config_array_to_list(blocks);
                string_array_destroy(blocks);  // Liberar el array devuelto por config

                char* blocks_string = list_to_config_string(blocks_list);
                
                // Nota: create_metadata_file espera el path del tag, no el path del metadata
                create_metadata_file(path_tag_dst, size, blocks_string, WORK_IN_PROGRESS);

                free(blocks_string);
                
                for (int i = 0; i < list_size(blocks_list); i++)
                {
                    char* physical_id_str = (char*)list_get(blocks_list, i);
                    int physical_id = atoi(physical_id_str);
                    
                    char* path_physical_block = get_physical_block_path(physical_id);
                    char* path_logical_block = get_logical_block_path(file_dst, tag_dst, i);

                    link(path_physical_block, path_logical_block);

                    free(path_physical_block);
                    free(path_logical_block);
                }

                log_info(logger, "##%d - Tag creado %s:%s", query_id, file_dst, tag_dst);
                unlock_file_tag(file_src, tag_src);
                // Solo unlock del dst si es diferente del src (evita unlock doble del mismo mutex)
                if (!same_file_tag)
                {
                    unlock_file_tag(file_dst, tag_dst);
                }

                list_destroy_and_destroy_elements(blocks_list, free);
                config_destroy(metadata_src);
                free(path_metadata_src);
                free(path_file_dst);
                free(path_tag_dst);
                free(path_logical_dst);
                
                // Enviar status Y tamaño del archivo
                t_packet* response = create_packet();
                STATUS_QUERY_OP_CODE status = SUCCESSFUL_INSTRUCTION;
                add_to_packet(response, &status, sizeof(STATUS_QUERY_OP_CODE));
                add_to_packet(response, &size, sizeof(int));
                send_packet(response, fd_connection);
                destroy_packet(response);

                free(file_src); free(tag_src); free(file_dst); free(tag_dst);

                break;
            }
            case COMMIT_TAG:
            {
                usleep(RETARDO_OP * 1000);
                uint32_t query_id;
                char* file_name = NULL;
                char* tag_name = NULL;

                extract_data(data, 1, &query_id, sizeof(uint32_t));
                extract_string(data, 2, &file_name);
                extract_string(data, 3, &tag_name);

                log_debug(logger, "Ejecutando COMMIT_TAG de %s:%s", file_name, tag_name);

                char* file_path = string_from_format("%s/files/%s", PUNTO_MONTAJE, file_name);
                char* tag_path = string_from_format("%s/%s", file_path, tag_name);

                if (access(file_path, F_OK) != 0)
                {
                    log_error(logger, "Error en COMMIT_TAG - El File %s no existe", file_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path); free(tag_path);
                    break;
                }

                if (access(tag_path, F_OK) != 0)
                {
                    log_error(logger, "Error en COMMIT_TAG - El File:Tag %s:%s no existe", file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path); free(tag_path);
                    break;
                }

                char* path_metadata = get_metadata_path(file_name, tag_name);
                if (access(path_metadata, F_OK) == -1)
                {
                    log_error(logger, "Error en COMMIT_TAG - No se pudo acceder al archivo metadata.config de %s:%s", file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path); free(tag_path); free(path_metadata);
                    break;
                }

                lock_file_tag(file_name, tag_name);

                t_config* metadata = config_create(path_metadata);
                char* estado = config_get_string_value(metadata, "ESTADO");
                
                if (strcmp(estado, "COMMITED") == 0)
                {
                    config_destroy(metadata);
                    free(path_metadata);
                    log_info(logger, "##%d - El archivo %s:%s ya se encuentra en estado COMMITED", query_id, file_name, tag_name);
                    send_status_response(fd_connection, SUCCESSFUL_INSTRUCTION);
                    unlock_file_tag(file_name, tag_name);
                    break;
                }

                char** blocks_array = config_get_array_value(metadata, "BLOCKS");
                t_list* blocks_list = config_array_to_list(blocks_array);
                string_array_destroy(blocks_array);  // Liberar el array devuelto por config

                pthread_mutex_lock(&MUTEX_HASH_INDEX);

                for (int i = 0; i < list_size(blocks_list); i++)
                {
                    char* physical_id_str = (char*)list_get(blocks_list, i);
                    int current_physical_id = atoi(physical_id_str);
                    
                    char* current_hash = get_block_md5(current_physical_id);
                    if (config_has_property(hash_index, current_hash))
                    {
                        char* confirmed_block_name = config_get_string_value(hash_index, current_hash);
                        int confirmed_phys_id;

                        sscanf(confirmed_block_name, "block%d", &confirmed_phys_id);
                        
                        if (confirmed_phys_id != current_physical_id)
                        {
                            log_info(logger, "##%d - %s:%s Bloque Lógico %d se reasigna de %d a %d", query_id, file_name, tag_name, i, current_physical_id, confirmed_phys_id);

                            char* logical_path = get_logical_block_path(file_name, tag_name, i);
                            unlink(logical_path);
                            log_info(logger, "##%d - %s:%s Se eliminó el hard link del bloque lógico %d al bloque físico %d", query_id, file_name, tag_name, i, current_physical_id);
                            
                            char* confirmed_path = get_physical_block_path(confirmed_phys_id);
                            link(confirmed_path, logical_path);
                            log_info(logger, "##%d - %s:%s Se agregó el hard link del bloque lógico %d al bloque físico %d", query_id, file_name, tag_name, i, confirmed_phys_id);
                            
                            free(logical_path);
                            free(confirmed_path);

                            char* current_physical_path = get_physical_block_path(current_physical_id);
                            struct stat st;
                            stat(current_physical_path, &st);

                            if (st.st_nlink == 1)
                            {
                                remove_block_from_hash_index_unlocked(current_physical_id);
                                mark_physical_block_as_free(current_physical_id);
                                log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d", query_id, current_physical_id);
                            }
                            free(current_physical_path);

                            free(list_get(blocks_list, i));

                            char* new_id_str = string_itoa(confirmed_phys_id);
                            list_replace(blocks_list, i, new_id_str);
                            
                        }
                    }
                    else
                    {
                        char* block_name = string_from_format("block%04d", current_physical_id);
                        config_set_value(hash_index, current_hash, block_name);
                        mark_physical_block_as_occupied(current_physical_id);
                        free(block_name);
                    }
                    free(current_hash);
                }

                config_save(hash_index);
                pthread_mutex_unlock(&MUTEX_HASH_INDEX);

                char* blocks_str = list_to_config_string(blocks_list);
                config_set_value(metadata, "BLOCKS", blocks_str);
                config_set_value(metadata, "ESTADO", "COMMITED");
                config_save(metadata);
                unlock_file_tag(file_name, tag_name);
                
                free(blocks_str);
                list_destroy_and_destroy_elements(blocks_list, free);
                config_destroy(metadata);
                free(path_metadata);

                log_info(logger, "##%d - Commit de File:Tag %s:%s", query_id, file_name, tag_name);
                send_status_response(fd_connection, SUCCESSFUL_INSTRUCTION);

                break;
            }
            case WRITE_FILE:
            {
                usleep(RETARDO_OP * 1000);
                
                int query_id;
                char* file_name = NULL;
                char* tag_name = NULL;
                int block_number;
                void* block_content = NULL;

                extract_data(data, 1, &query_id, sizeof(int));

                extract_string(data, 2, &file_name);
                extract_string(data, 3, &tag_name);

                extract_data(data, 4, &block_number, sizeof(int));
                block_content = malloc(fs_cfg->block_size);
                extract_data(data, 5, block_content, fs_cfg->block_size);

                log_debug(logger, "Ejecutando WRITE_FILE de %s:%s", file_name, tag_name);

                char* file_path = get_file_path(file_name, tag_name);

                if (access(file_path, F_OK) == -1)
                {
                    log_error(logger, "##%d - WRITE_FILE - El archivo %s:%s no existe", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path); free(block_content);
                    break;
                }

                lock_file_tag(file_name, tag_name);

                char* metadata_path = get_metadata_path(file_name, tag_name);

                if (access(metadata_path, F_OK) == -1)
                {
                    log_error(logger, "##%d - WRITE_FILE - Error abriendo metadata del archivo %s:%s", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    unlock_file_tag(file_name, tag_name);
                    free(file_path); free(block_content); free(metadata_path);
                    break;
                }

                t_config* metadata = config_create(metadata_path);
                char* estado = config_get_string_value(metadata, "ESTADO");

                if (strcmp(estado, "COMMITED") == 0)
                {
                    log_error(logger, "##%d - WRITE_FILE - El archivo %s:%s no se puede escribir debido a que se encuentra en estado COMMITED", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_FILE_TAG_IS_COMMITTED);
                    unlock_file_tag(file_name, tag_name);
                    free(file_path); free(block_content); free(metadata_path);
                    break;
                }
                
                char** blocks_array = config_get_array_value(metadata, "BLOCKS");
                t_list* blocks_list = config_array_to_list(blocks_array);
                string_array_destroy(blocks_array);

                if (block_number < 0 || block_number >= list_size(blocks_list))
                {
                    log_error(logger, "##%d - WRITE_FILE - El archivo %s:%s no se puede escribir debido a que el bloque %d no existe", query_id, file_name, tag_name, block_number);
                    config_destroy(metadata); // CAPAZ ESTÁ MAL
                    send_status_response(fd_connection, ERROR_OUT_OF_BOUNDS);
                    unlock_file_tag(file_name, tag_name);
                    free(file_path); free(block_content); free(metadata_path); list_destroy_and_destroy_elements(blocks_list, free);
                    break;
                }

                usleep(RETARDO_BLOQUE * 1000);

                // Obtenemos el bloque físico
                char* phys_id_str = (char*)list_get(blocks_list, block_number);
                int current_phys_id = atoi(phys_id_str);
                char* current_phys_path = get_physical_block_path(current_phys_id);
                
                struct stat st;
                stat(current_phys_path, &st);
                
                int target_phys_id = current_phys_id;
                bool metadata_changed = false;

                // Caso COMPARTIDO
                if (st.st_nlink > 1)
                {
                    int new_phys_id = get_free_block();
                    if (new_phys_id == -1)
                    {
                        log_error(logger, "##%d - WRITE_FILE - No hay bloques libres para compartir el archivo %s:%s", query_id, file_name, tag_name);
                        send_status_response(fd_connection, ERROR_NO_SPACE_AVAILABLE);
                        unlock_file_tag(file_name, tag_name);
                        free(file_path); free(block_content); free(metadata_path); list_destroy_and_destroy_elements(blocks_list, free);
                        break;
                    }

                    log_info(logger, "##%d - Bloque Físico Reservado - Número de Bloque: %d", query_id, new_phys_id);
                    target_phys_id = new_phys_id;
                    
                    // Eliminamos hard link viejo
                    char* logical_path = get_logical_block_path(file_name, tag_name, block_number);
                    unlink(logical_path);

                    log_info(logger, "##%d - %s:%s Se eliminó el hard link del bloque lógico %d al bloque físico %d", query_id, file_name, tag_name, block_number, current_phys_id);

                    // Verificar si el bloque anterior debe liberarse (ya no tiene más referencias)
                    struct stat st_old;
                    stat(current_phys_path, &st_old);
                    if (st_old.st_nlink == 1 && current_phys_id != 0)
                    {
                        remove_block_from_hash_index(current_phys_id);
                        mark_physical_block_as_free(current_phys_id);
                        log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d", query_id, current_phys_id);
                    }

                    // Creamos hard link nuevo
                    char* new_phys_path_link = get_physical_block_path(new_phys_id);
                    link(new_phys_path_link, logical_path);

                    log_info(logger, "##%d - %s:%s Se agregó el hard link del bloque lógico %d al bloque físico %d", query_id, file_name, tag_name, block_number, new_phys_id);

                    free(logical_path); free(new_phys_path_link);
                    free(list_get(blocks_list, block_number));
                    list_replace(blocks_list, block_number, string_itoa(target_phys_id));
                    metadata_changed = true;
                }

                free(current_phys_path);

                char* target_path = get_physical_block_path(target_phys_id);
                FILE* f_target = fopen(target_path, "wb");
                if (f_target)
                {
                    fwrite(block_content, 1, fs_cfg->block_size, f_target);
                    fclose(f_target);
                }
                else
                {
                    log_error(logger, "##%d - WRITE_FILE - Error abriendo el archivo %s:%s", query_id, file_name, tag_name);
                }
                
                free(target_path);

                log_info(logger, "##%d - Bloque Lógico Escrito %s:%s - Número de Bloque: %d", query_id, file_name, tag_name, block_number);
                
                // Debug content: ensure null-termination for safe logging if it's text
                char* debug_content = malloc(fs_cfg->block_size + 1);
                memcpy(debug_content, block_content, fs_cfg->block_size);
                debug_content[fs_cfg->block_size] = '\0';
                log_debug(logger, "File:Tag %s:%s de Número de Bloque %d que referencia a bloque físico %d - Contenido escrito: %s", file_name, tag_name, block_number, target_phys_id, debug_content);
                free(debug_content);

                if (metadata_changed)
                {
                    char* blocks_str = list_to_config_string(blocks_list);
                    config_set_value(metadata, "BLOCKS", blocks_str);
                    config_save(metadata);
                    free(blocks_str);
                }

                list_destroy_and_destroy_elements(blocks_list, free);
                config_destroy(metadata);
                free(file_path); free(block_content); free(metadata_path);

                unlock_file_tag(file_name, tag_name);
                send_status_response(fd_connection, SUCCESSFUL_INSTRUCTION);
                break;
            }
            case READ_FILE:
            {
                usleep(RETARDO_OP * 1000);
                
                int query_id;
                char* file_name;
                char* tag_name;
                int block_number;

                extract_data(data, 1, &query_id, sizeof(int));
                extract_string(data, 2, &file_name);
                extract_string(data, 3, &tag_name);
                extract_data(data, 4, &block_number, sizeof(int));  

                log_debug(logger, "Ejecutando READ_FILE de %s:%s", file_name, tag_name);

                char* file_path = get_file_path(file_name, tag_name);
                
                if (access(file_path, F_OK) == -1)
                {
                    log_error(logger, "##%d - READ_FILE - El archivo %s:%s no existe", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path);
                    break;
                }

                lock_file_tag(file_name, tag_name);

                char* metadata_path = get_metadata_path(file_name, tag_name);
                if (access(metadata_path, F_OK) == -1)
                {
                    log_error(logger, "##%d - READ_FILE - El archivo %s:%s no existe", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    unlock_file_tag(file_name, tag_name);
                    free(file_path); free(metadata_path);
                    break;
                }

                t_config* metadata = config_create(metadata_path);

                char** blocks_array = config_get_array_value(metadata, "BLOCKS");
                t_list* blocks_list = config_array_to_list(blocks_array);
                string_array_destroy(blocks_array);

                if (block_number < 0 || block_number >= list_size(blocks_list))
                {
                    log_error(logger, "##%d - READ_FILE - El bloque %d no existe", query_id, block_number);
                    send_status_response(fd_connection, ERROR_OUT_OF_BOUNDS);
                    unlock_file_tag(file_name, tag_name);
                    config_destroy(metadata);
                    list_destroy_and_destroy_elements(blocks_list, free);
                    free(file_path); free(metadata_path);
                    break;
                }

                usleep(RETARDO_BLOQUE * 1000);

                char* phys_id_str = (char*)list_get(blocks_list, block_number);
                int phys_id = atoi(phys_id_str);
                
                void* buffer = read_physical_block(phys_id);

                if (buffer)
                {
                    log_info(logger, "##%d - Bloque Lógico Leído %s:%s - Número de Bloque: %d", query_id, file_name, tag_name, block_number);

                    // Debug content: ensure null-termination
                    char* debug_buffer = malloc(fs_cfg->block_size + 1);
                    memcpy(debug_buffer, buffer, fs_cfg->block_size);
                    debug_buffer[fs_cfg->block_size] = '\0';
                    log_debug(logger, "File:Tag %s:%s de Número de Bloque %d que referencia a bloque físico %d - Contenido leído: %s", file_name, tag_name, block_number, phys_id, debug_buffer);
                    free(debug_buffer);

                    STATUS_QUERY_OP_CODE status = SUCCESSFUL_INSTRUCTION;
                    t_packet* response = create_packet();
                    add_to_packet(response, &status, sizeof(STATUS_QUERY_OP_CODE));
                    add_to_packet(response, buffer, fs_cfg->block_size);
                    send_packet(response, fd_connection);
                    destroy_packet(response);
                    
                    free(buffer);
                }
                else
                {
                    log_error(logger, "##%d - READ_FILE - Error leyendo bloque físico %d", query_id, phys_id);
                    // Usamos un código de error genérico u otro adecuado si existe
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG); 
                }
                unlock_file_tag(file_name, tag_name);
                free(file_name); free(tag_name); free(file_path); free(metadata_path);
                config_destroy(metadata);
                list_destroy_and_destroy_elements(blocks_list, free);
                break;
            }
            case DELETE_FILE:
            {
                usleep(RETARDO_OP * 1000);
                
                uint32_t query_id;
                char* file_name = NULL;
                char* tag_name = NULL;

                extract_data(data, 1, &query_id, sizeof(uint32_t));
                extract_string(data, 2, &file_name);
                extract_string(data, 3, &tag_name);

                log_debug(logger, "Ejecutando DELETE_FILE de %s:%s", file_name, tag_name);

                // Proteger initial_file:BASE de ser eliminado (requerimiento del enunciado)
                if (strcmp(file_name, "initial_file") == 0 && strcmp(tag_name, "BASE") == 0)
                {
                    log_error(logger, "##%d - DELETE_FILE - No se puede eliminar initial_file:BASE", query_id);
                    send_status_response(fd_connection, ERROR_GENERIC);
                    free(file_name); free(tag_name);
                    break;
                }

                char* file_path = get_file_path(file_name, tag_name);
                
                if (access(file_path, F_OK) == -1)
                {
                    log_error(logger, "##%d - DELETE_FILE - El archivo %s:%s no existe", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    free(file_path);
                    break;
                }

                lock_file_tag(file_name, tag_name);

                char* metadata_path = get_metadata_path(file_name, tag_name);
                if (access(metadata_path, F_OK) == -1)
                {
                    log_error(logger, "##%d - DELETE_FILE - El archivo %s:%s no existe", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    unlock_file_tag(file_name, tag_name);
                    free(file_path); free(metadata_path);
                    break;
                }

                t_config* metadata = config_create(metadata_path);
                char** blocks_array = config_get_array_value(metadata, "BLOCKS");
                t_list* blocks_list = config_array_to_list(blocks_array);
                string_array_destroy(blocks_array);

                for (int i = 0; i < list_size(blocks_list); i++)
                {
                    char* phys_id_str = (char*)list_get(blocks_list, i);
                    int phys_id = atoi(phys_id_str);
                    
                    char* logical_path = get_logical_block_path(file_name, tag_name, i);
                    unlink(logical_path);
                    free(logical_path);

                    char* phys_path = get_physical_block_path(phys_id);
                    struct stat st;
                    stat(phys_path, &st);
                    if (st.st_nlink == 1)
                    {
                        remove_block_from_hash_index(phys_id);
                        mark_physical_block_as_free(phys_id);
                        log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d", query_id, phys_id);
                    }
                    free(phys_path);
                }

                list_destroy_and_destroy_elements(blocks_list, free);
                config_destroy(metadata);
                
                remove(metadata_path);
                free(metadata_path);
                
                char* tag_path = string_from_format("%s/%s", file_path, tag_name);
                char* path_logical_dir = string_from_format("%s/logical_blocks", tag_path);

                rmdir(path_logical_dir);
                rmdir(tag_path);
                rmdir(file_path);

                free(file_path); free(tag_path); free(path_logical_dir);

                unlock_file_tag(file_name, tag_name);
                log_info(logger, "##%d - Tag Eliminado %s:%s", query_id, file_name, tag_name);
                send_status_response(fd_connection, SUCCESSFUL_INSTRUCTION);

                break;
            }
            case GET_BLOCK_SIZE:
            {
                uint32_t block_size = fs_cfg->block_size;
                t_packet* response = create_packet();
                add_to_packet(response, &block_size, sizeof(uint32_t));
                send_packet(response, fd_connection);
                destroy_packet(response);
                break;
            }
            case GET_FILE_TAG_SIZE:
            {
                uint32_t query_id;
                char* file_name = NULL;
                char* tag_name = NULL;

                extract_data(data, 1, &query_id, sizeof(uint32_t));
                extract_string(data, 2, &file_name);
                extract_string(data, 3, &tag_name);

                log_debug(logger, "Ejecutando GET_FILE_TAG_SIZE de %s:%s", file_name, tag_name);

                char* file_path = get_file_path(file_name, tag_name);
                char* metadata_path = get_metadata_path(file_name, tag_name);

                if (access(file_path, F_OK) != 0 || access(metadata_path, F_OK) != 0)
                {
                    log_error(logger, "##%d - GET_FILE_TAG_SIZE - El archivo %s:%s no existe", query_id, file_name, tag_name);
                    send_status_response(fd_connection, ERROR_NON_EXISTING_FILE_TAG);
                    
                    free(file_path); free(metadata_path);
                    free(file_name); free(tag_name);
                    break;
                }

                lock_file_tag(file_name, tag_name);
                
                t_config* metadata = config_create(metadata_path);
                int file_size = config_get_int_value(metadata, "TAMAÑO");
                
                config_destroy(metadata);
                unlock_file_tag(file_name, tag_name);

                log_debug(logger, "##%d - GET_FILE_TAG_SIZE %s:%s - Tamaño: %d", query_id, file_name, tag_name, file_size);

                STATUS_QUERY_OP_CODE status = SUCCESSFUL_INSTRUCTION;
                t_packet* response = create_packet();
                
                add_to_packet(response, &status, sizeof(STATUS_QUERY_OP_CODE));
                add_to_packet(response, &file_size, sizeof(int));
                
                send_packet(response, fd_connection);
                destroy_packet(response);

                free(file_path); free(metadata_path);
                free(file_name); free(tag_name);

                break;
            }
        }
        list_destroy_and_destroy_elements(data, free);
    }

    pthread_mutex_lock(&mutex_connected_workers);
    connected_workers--;
    log_info(logger, "##Se desconecta el Worker %s - Cantidad de Workers: %d", module_id, connected_workers);
    free(module_id);
    pthread_mutex_unlock(&mutex_connected_workers);
    
    return NULL;
}