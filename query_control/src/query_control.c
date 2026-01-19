#include "query_control.h"

char *module_id = NULL;
char *log_path = NULL;

void init_query_control(char *config_path)
{
    config_file = config_init(config_path);
    config_query_control = malloc(sizeof(t_query_control_config));

    config_query_control->ip_master = config_get_string_value(config_file, "IP_MASTER");
    config_query_control->puerto_master = config_get_string_value(config_file, "PUERTO_MASTER");
    config_query_control->log_level = config_get_string_value(config_file, "LOG_LEVEL");

    logger = logger_init(log_path, module_id, config_query_control->log_level);
    log_debug(logger, "Query Control iniciado");
}

void init_query_control_socket()
{
    master_socket = create_connection(config_query_control->ip_master, config_query_control->puerto_master, logger);
    handshake_result result = start_handshake(master_socket, module_id, MASTER_HANDSHAKE, logger);
    if (result != HANDSHAKE_OK)
    {
        log_error(logger, "No se pudo conectar con el Master. Saliendo...");
        if (module_id)
            free(module_id);
        if (log_path)
            free(log_path);
        free_component(1, &master_socket, logger, config_file, config_query_control);
        exit(EXIT_FAILURE);
    }
    log_info(logger, "## Conexión al Master exitosa. IP: %s, Puerto: %s", config_query_control->ip_master, config_query_control->puerto_master);
}

void send_query_request(char *query_file_path, int priority)
{
    t_packet *packet = create_packet();
    QUERY_OP_CODE op_code = QUERY_NEW;
    add_to_packet(packet, &op_code, sizeof(QUERY_OP_CODE));
    add_to_packet(packet, query_file_path, strlen(query_file_path) + 1);
    add_to_packet(packet, &priority, sizeof(int));

    send_packet(packet, master_socket);
    destroy_packet(packet);

    log_info(logger, "## Solicitud de ejecución de Query: %s, prioridad: %d", query_file_path, priority);
}

int main(int argc, char *argv[])
{
    if (argc < 4 || argc > 5)
    {
        printf("Uso: %s [archivo_config] [archivo_query] [prioridad] (OPCIONAL: [ID])\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *config_path = argv[1];
    char *query_file_path = argv[2];
    int priority = atoi(argv[3]);

    if (argc == 5)
    {
        char *qc_id = argv[4];
        module_id = string_from_format("QUERY_CONTROL_%s", qc_id);
        log_path = string_from_format("query_control_%s.log", qc_id);
    }
    else
    {
        module_id = strdup("QUERY_CONTROL");
        log_path = strdup("query_control.log");
    }

    init_query_control(config_path);
    init_query_control_socket();

    send_query_request(query_file_path, priority);

    bool waiting = true;
    while (waiting)
    {
        t_list *data = receive_packet(master_socket);

        if (data == NULL)
        {
            log_error(logger, "## Se perdió la conexión con el Master. Saliendo...");
            break;
        }

        QUERY_OP_CODE op_code;
        extract_data(data, 0, &op_code, sizeof(QUERY_OP_CODE));

        switch (op_code)
        {
        case QUERY_READ:
            char *file_name;
            char *content;
            extract_string(data, 1, &file_name);
            extract_string(data, 2, &content);
            log_info(logger, "## Lectura realizada: File %s, contenido: %s", file_name, content);
            free(file_name);
            free(content);
            break;
        case QUERY_END:
            char *reason;
            extract_string(data, 1, &reason);
            log_info(logger, "## Query Finalizada - %s", reason);
            waiting = false;
            free(reason);
            break;
        default:
            log_warning(logger, "Operación desconocida recibida del Master: %d", op_code);
            break;
        }

        list_destroy_and_destroy_elements(data, free);
    }

    free(module_id);
    free(log_path);
    free_component(1, &master_socket, logger, config_file, config_query_control);
    return 0;
}
