#include <worker_comms.h>
#include <sharedWorker.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MOCK_BLOCK_SIZE 16

int block_size = 0;

int storage_get_block_size(){

    STORAGE_OP_CODE op_code = GET_BLOCK_SIZE; 

    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));

    send_packet(packet, storage_socket);
    destroy_packet(packet);

    t_list *data = receive_packet(storage_socket);
    if (!data || list_size(data) < 1) {
        log_error(logger, "Error recibiendo respuesta de storage_get_block_size del Storage");
        if (data) list_destroy_and_destroy_elements(data, free);
        return 0;
    }

    int size;
    extract_data(data, 0, &size, sizeof(int));
    list_destroy_and_destroy_elements(data, free);
    return size;
}

STATUS_QUERY_OP_CODE storage_write(char* file_tag, int block, void* block_content, int query_id) {
    char** ft_split = split_file_tag(file_tag);

    if (ft_split == NULL) {
        log_error(logger, "PID: %d - Error parseando File:Tag en WRITE: %s", query_id, file_tag);
        return ERROR_GENERIC;
    }

    char* name_file = ft_split[0];
    char* tag_file = ft_split[1];

    STORAGE_OP_CODE op_code = WRITE_FILE; 

    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &query_id, sizeof(int));
    
    add_to_packet(packet, name_file, strlen(name_file) + 1);
    add_to_packet(packet, tag_file, strlen(tag_file) + 1);
    
    add_to_packet(packet, &block, sizeof(int));
    add_to_packet(packet, block_content, block_size);

    free_split_file_tag(ft_split);

    send_packet(packet, storage_socket);
    destroy_packet(packet);

    t_list *data = receive_packet(storage_socket);
    if (!data || list_size(data) < 1) {
        log_error(logger, "query_id: %d - Error recibiendo respuesta de WRITE del Storage", query_id);
        if (data) list_destroy_and_destroy_elements(data, free);
        return ERROR_GENERIC;
    }

    STATUS_QUERY_OP_CODE status_op;
    extract_data(data, 0, &status_op, sizeof(STATUS_QUERY_OP_CODE));
    list_destroy_and_destroy_elements(data, free);

    return status_op;
}

void* storage_read(char* file_tag, int block, STATUS_QUERY_OP_CODE* status_code, int query_id) {
    char** ft_split = split_file_tag(file_tag);

    if (ft_split == NULL) {
        log_error(logger, "query_id: %d - Error parseando File:Tag en READ: %s", query_id, file_tag);
        *status_code = ERROR_GENERIC;
        return NULL;
    }

    char* name_file = ft_split[0];
    char* tag_file = ft_split[1];

    STORAGE_OP_CODE op_code = READ_FILE; 

    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &query_id, sizeof(int));
    
    add_to_packet(packet, name_file, strlen(name_file) + 1);
    add_to_packet(packet, tag_file, strlen(tag_file) + 1);
    
    add_to_packet(packet, &block, sizeof(int));

    free_split_file_tag(ft_split);

    send_packet(packet, storage_socket);
    destroy_packet(packet);

    t_list *data = receive_packet(storage_socket);
    if (!data || list_size(data) < 1) {
        log_error(logger, "query_id: %d - Error recibiendo respuesta de READ del Storage", query_id);
        if (data) list_destroy_and_destroy_elements(data, free);
        *status_code = ERROR_GENERIC;
        return NULL;
    }

    STATUS_QUERY_OP_CODE status_op;
    extract_data(data, 0, &status_op, sizeof(STATUS_QUERY_OP_CODE));
    *status_code = status_op;

    void* buffer = NULL;

    if (*status_code == SUCCESSFUL_INSTRUCTION) { 
        if (list_size(data) == 2) {
            buffer = malloc(block_size);
            extract_data(data, 1, buffer, block_size);
        } else {
            log_error(logger, "query_id: %d - Storage reportó éxito en READ pero no envió datos", query_id);
            *status_code = ERROR_GENERIC;
        }
    }

    list_destroy_and_destroy_elements(data, free);
    return buffer;
}

int storage_get_file_tag_size(char* file_tag, STATUS_QUERY_OP_CODE* status_code, int query_id) {
    char** ft_split = split_file_tag(file_tag);

    if (ft_split == NULL) {
        log_error(logger, "query_id: %d - Error parseando File:Tag en READ: %s", query_id, file_tag);
        *status_code = ERROR_GENERIC;
        return -1;
    }

    char* name_file = ft_split[0];
    char* tag_file = ft_split[1];

    STORAGE_OP_CODE op_code = GET_FILE_TAG_SIZE; 

    t_packet *packet = create_packet();
    add_to_packet(packet, &op_code, sizeof(int));
    add_to_packet(packet, &query_id, sizeof(int));
    
    add_to_packet(packet, name_file, strlen(name_file) + 1);
    add_to_packet(packet, tag_file, strlen(tag_file) + 1);
    
    send_packet(packet, storage_socket);
    destroy_packet(packet);
    free_split_file_tag(ft_split);

    t_list *data = receive_packet(storage_socket);
    
    if (!data || list_size(data) < 2) {
        log_error(logger, "query_id: %d - Error recibiendo respuesta de GET_FILE_TAG del Storage", query_id);
        if (data) list_destroy_and_destroy_elements(data, free);
        *status_code = ERROR_GENERIC;
        return -1;
    }

    STATUS_QUERY_OP_CODE status_op;
    extract_data(data, 0, &status_op, sizeof(STATUS_QUERY_OP_CODE));
    *status_code = status_op;

    int file_size = -1;

    if (*status_code == SUCCESSFUL_INSTRUCTION) { 
        extract_data(data, 1, &file_size, sizeof(int));
        log_debug(logger, "query_id: %d - Tamaño recibido del Storage: %d", query_id, file_size);
    } else {
        log_error(logger, "query_id: %d - Storage reportó error en GET_FILE_TAG. Status: %d", query_id, *status_code);
    }

    list_destroy_and_destroy_elements(data, free);
    
    return file_size;
}



void master_send_context(int fd_connection, int query_id, int pc, QUERY_OP_CODE interrupt_op_code)
{
    t_packet *packet = create_packet();
    add_to_packet(packet, &interrupt_op_code, sizeof(QUERY_OP_CODE));
    add_to_packet(packet, &query_id, sizeof(int));
    add_to_packet(packet, &pc, sizeof(int));

    send_packet(packet, fd_connection);
    destroy_packet(packet);
}


void master_send_end(int fd_connection, int query_id, STATUS_QUERY_OP_CODE status_code)
{
    t_packet *packet = create_packet();

    QUERY_OP_CODE op_code = QUERY_END;
    add_to_packet(packet, &op_code, sizeof(QUERY_OP_CODE));
    add_to_packet(packet, &query_id, sizeof(int));
    add_to_packet(packet, &status_code, sizeof(STATUS_QUERY_OP_CODE));


    send_packet(packet, fd_connection);
    destroy_packet(packet);
}