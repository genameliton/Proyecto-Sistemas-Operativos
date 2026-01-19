#ifndef SOCKET_H_
#define SOCKET_H_

#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef enum
{
	MASTER_HANDSHAKE,
	STORAGE_HANDSHAKE
} handshake_code;

typedef enum
{
	HANDSHAKE_ERROR = -1,
	HANDSHAKE_OK,
	HANDSHAKE_INVALID_FORMAT,
	HANDSHAKE_CODE_MISMATCH
} handshake_result;

typedef struct
{
	int fd_server;
	void *(*client_handler)(void *);
} t_server_args;
typedef struct
{
	int size;
	void *stream;
} t_buffer;

typedef struct
{
	t_buffer *buffer;
} t_packet;

// Server code
int init_server(char *port);
handshake_result receive_handshake(int fd_conection, int handshake_code, char **module_id);
void handle_multiple_connections_server(t_server_args *arg);
handshake_result start_handshake(int fd_connection, char *module_id, int handshake_code, t_log *logger);
// Client code
int create_connection(char *ip, char *port, t_log *logger);
void free_connection(int fd_socket);
// Serializacion
void *serialize_packet(t_packet *packet, int bytes);
t_buffer *create_buffer();
t_packet *create_packet();
void destroy_packet(t_packet *packet);
void add_to_packet(t_packet *packet, void *stream, int size);
void send_packet(t_packet *packet, int fd_socket);
void *receive_buffer(int *size, int fd_socket);
t_list *receive_packet(int fd_socket);
void *peek_buffer(int *size, int fd_socket);
t_list *peek_packet(int fd_socket);
void extract_data(t_list *data, int index, void *dest, size_t size);
void extract_string(t_list *data, int index, char **dest);
const char *handshake_code_to_string(handshake_code code);
const char *handshake_result_to_string(handshake_result result);

#endif