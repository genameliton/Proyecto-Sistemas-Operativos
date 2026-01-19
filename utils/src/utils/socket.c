
#include "socket.h"

// Server code
int init_server(char *port)
{
    int fd_socket_sv;

    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, port, &hints, &servinfo);

    fd_socket_sv = socket(
        servinfo->ai_family,
        servinfo->ai_socktype,
        servinfo->ai_protocol);

    setsockopt(fd_socket_sv, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

    bind(fd_socket_sv, servinfo->ai_addr, servinfo->ai_addrlen);
    listen(fd_socket_sv, SOMAXCONN);

    freeaddrinfo(servinfo);

    return fd_socket_sv;
}

handshake_result receive_handshake(int fd_conection, int handshake_code, char **module_id)
{
    t_list *handshake_data = receive_packet(fd_conection);
    if (list_size(handshake_data) != 2)
    {
        int resultError = HANDSHAKE_ERROR;
        send(fd_conection, &resultError, sizeof(int), 0);
        list_destroy_and_destroy_elements(handshake_data, free);
        return HANDSHAKE_INVALID_FORMAT;
    }

    int received_code;
    memcpy(&received_code, list_get(handshake_data, 0), sizeof(int));

    if (received_code != handshake_code)
    {
        int resultError = HANDSHAKE_ERROR;
        send(fd_conection, &resultError, sizeof(int), 0);
        list_destroy_and_destroy_elements(handshake_data, free);
        return HANDSHAKE_CODE_MISMATCH;
    }

    *module_id = strdup(list_get(handshake_data, 1));
    int resultOK = HANDSHAKE_OK;
    send(fd_conection, &resultOK, sizeof(int), 0);
    // log_info(logger, "%s Handshake OK!", module_id);
    list_destroy_and_destroy_elements(handshake_data, free);
    return HANDSHAKE_OK;
}

void handle_multiple_connections_server(t_server_args *arg)
{
    int fd_server = arg->fd_server;
    void *(*client_handler)(void *) = arg->client_handler;
    free(arg);
    while (1)
    {
        pthread_t thread;

        int *fd_conexion_ptr = malloc(sizeof(int));
        *fd_conexion_ptr = accept(fd_server, NULL, NULL);
        pthread_create(&thread, NULL, client_handler, fd_conexion_ptr);
        pthread_detach(thread);
    }
}

handshake_result start_handshake(int fd_connection, char *module_id, int handshake_code, t_log *logger)
{
    handshake_result result;
    t_packet *handshake_packet = create_packet();
    add_to_packet(handshake_packet, &handshake_code, sizeof(int));
    add_to_packet(handshake_packet, module_id, strlen(module_id) + 1);
    send_packet(handshake_packet, fd_connection);
    destroy_packet(handshake_packet);

    recv(fd_connection, &result, sizeof(int), MSG_WAITALL);
    if (result != HANDSHAKE_OK)
    {
        log_error(logger, "%s %s %s", handshake_result_to_string(result), module_id, handshake_code_to_string(handshake_code));
        close(fd_connection);
    }

    return result;
}

// Client code
int create_connection(char *ip, char *port, t_log *logger)
{
    int err;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    err = getaddrinfo(ip, port, &hints, &server_info);
    if (err != 0)
    {
        log_error(logger, "Error en getaddrinfo! %s\n", gai_strerror(err));
        exit(1);
    }

    int client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (client_socket == -1)
    {
        log_error(logger, "Error en socket!");
        freeaddrinfo(server_info);
        exit(1);
    }

    err = connect(client_socket, server_info->ai_addr, server_info->ai_addrlen);
    if (err == -1)
    {
        log_error(logger, "Error en connect!");
        close(client_socket);
        freeaddrinfo(server_info);
        exit(1);
    }

    freeaddrinfo(server_info);

    return client_socket;
}

void free_connection(int fd_socket)
{
    close(fd_socket);
}

// Serializacion
void *serialize_packet(t_packet *packet, int bytes)
{
    void *magic = malloc(bytes);
    int offset = 0;
    memcpy(magic + offset, &(packet->buffer->size), sizeof(int));
    offset += sizeof(int);
    memcpy(magic + offset, packet->buffer->stream, packet->buffer->size);
    return magic;
}

t_buffer *create_buffer()
{
    t_buffer *buffer = malloc(sizeof(t_buffer));
    buffer->size = 0;
    buffer->stream = NULL;
    return buffer;
}

t_packet *create_packet()
{
    t_packet *packet = malloc(sizeof(t_packet));
    packet->buffer = create_buffer();
    return packet;
}

void destroy_packet(t_packet *packet)
{
    if (packet->buffer->stream != NULL)
        free(packet->buffer->stream);
    free(packet->buffer);
    free(packet);
}

void add_to_packet(t_packet *packet, void *stream, int size)
{
    packet->buffer->stream = realloc(packet->buffer->stream, packet->buffer->size + size + sizeof(int));
    memcpy(packet->buffer->stream + packet->buffer->size, &size, sizeof(int));
    memcpy(packet->buffer->stream + packet->buffer->size + sizeof(int), stream, size);
    packet->buffer->size += size + sizeof(int);
}

void send_packet(t_packet *packet, int fd_socket)
{
    int bytes = packet->buffer->size + sizeof(int);
    void *to_send = serialize_packet(packet, bytes);
    send(fd_socket, to_send, bytes, 0);
    free(to_send);
}

void *receive_buffer(int *size, int fd_socket)
{
    void *buffer;

    if (recv(fd_socket, size, sizeof(int), MSG_WAITALL) <= 0)
        return NULL;

    buffer = malloc(*size);
    if (recv(fd_socket, buffer, *size, MSG_WAITALL) <= 0)
    {
        free(buffer);
        return NULL;
    }

    return buffer;
}

t_list *receive_packet(int fd_socket)
{
    int size;
    int offset = 0;
    void *buffer;
    t_list *values = list_create();
    int size_2;

    buffer = receive_buffer(&size, fd_socket);

    if (buffer == NULL)
    {
        list_destroy(values);
        return NULL;
    }

    while (offset < size)
    {
        memcpy(&size_2, buffer + offset, sizeof(int));
        offset += sizeof(int);
        char *value = malloc(size_2);
        memcpy(value, buffer + offset, size_2);
        offset += size_2;
        list_add(values, value);
    }
    free(buffer);

    return values;
}

void *peek_buffer(int *size, int fd_socket)
{
    void *buffer;

    if (recv(fd_socket, size, sizeof(int), MSG_PEEK | MSG_DONTWAIT) <= 0)
        return NULL;

    int total_size = sizeof(int) + *size;
    buffer = malloc(total_size);

    if (recv(fd_socket, buffer, total_size, MSG_PEEK | MSG_DONTWAIT) < total_size)
    {
        free(buffer);
        return NULL;
    }

    void *data = malloc(*size);
    memcpy(data, buffer + sizeof(int), *size);
    free(buffer);

    return data;
}

t_list *peek_packet(int fd_socket)
{
    int size;
    int offset = 0;
    void *buffer;
    t_list *values = list_create();
    int size_2;

    buffer = peek_buffer(&size, fd_socket);

    if (buffer == NULL)
    {
        list_destroy(values);
        return NULL;
    }

    while (offset < size)
    {
        memcpy(&size_2, buffer + offset, sizeof(int));
        offset += sizeof(int);
        char *value = malloc(size_2);
        memcpy(value, buffer + offset, size_2);
        offset += size_2;
        list_add(values, value);
    }
    free(buffer);

    return values;
}

void extract_data(t_list *data, int index, void *dest, size_t size)
{
    void *src = list_get(data, index);
    if (src)
        memcpy(dest, src, size);
}

void extract_string(t_list *data, int index, char **dest)
{
    char *src = list_get(data, index);
    if (!src)
        return;

    *dest = strdup(src);
}

const char *handshake_code_to_string(handshake_code code)
{
    switch (code)
    {
    case MASTER_HANDSHAKE:
        return "MASTER_HANDSHAKE";
    case STORAGE_HANDSHAKE:
        return "STORAGE_HANDSHAKE";
    default:
        return "UNKNOWN_HANDSHAKE_CODE";
    }
}

const char *handshake_result_to_string(handshake_result result)
{
    switch (result)
    {
    case HANDSHAKE_OK:
        return "HANDSHAKE_OK";
    case HANDSHAKE_ERROR:
        return "HANDSHAKE_ERROR";
    case HANDSHAKE_INVALID_FORMAT:
        return "HANDSHAKE_INVALID_FORMAT";
    case HANDSHAKE_CODE_MISMATCH:
        return "HANDSHAKE_CODE_MISMATCH";
    default:
        return "UNKNOWN_HANDSHAKE_CODE";
    }
}