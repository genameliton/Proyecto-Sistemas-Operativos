#include <directory.h>

char* parse_file_state(FILE_STATE state)
{
    switch (state)
    {
        case WORK_IN_PROGRESS:
            return "WORK_IN_PROGRESS";
        case COMMITED:
            return "COMMITED";
        default:
            return "UNDEFINED_STATE";
    }
}

void create_metadata_file(char* tag_path, int size, char* blocks_list, FILE_STATE state)
{
    char* metadata_path = string_from_format("%s/metadata.config", tag_path);

    log_debug(logger, "Creando file metadata en %s", metadata_path);
    log_debug(logger, "Tamaño: %d", size);
    log_debug(logger, "Blocks: %s", blocks_list);
    log_debug(logger, "State: %s", parse_file_state(state));
    
    FILE* f = fopen(metadata_path, "w");
    if (f)
    {
        fprintf(f, "TAMAÑO=%d\n", size);
        fprintf(f, "BLOCKS=%s\n", blocks_list);
        fprintf(f, "ESTADO=%s\n", parse_file_state(state));
        fclose(f);
    }
    else
    {
        log_error(logger, "Error creando archivo metadata.config en %s: %s", metadata_path, strerror(errno));
    }
    free(metadata_path);
}

bool create_directory_structure(void)
{
    char* physical_blocks_path = string_from_format("%s/physical_blocks", PUNTO_MONTAJE);
    char* files_path = string_from_format("%s/files", PUNTO_MONTAJE);

    mkdir(PUNTO_MONTAJE, 0777);

    if (mkdir(physical_blocks_path, 0777) == -1)
    {
        log_error(logger, "Error creando directorio physical_blocks");
        free(physical_blocks_path);
        free(files_path);
        return false;
    }

    if (mkdir(files_path, 0777) == -1)
    {
        log_error(logger, "Error creando directorio files_path");
        free(physical_blocks_path);
        free(files_path);
        return false;
    }

    free(physical_blocks_path);
    free(files_path);

    log_info(logger, "Estructura de directorios creada");
    return true;
}

bool create_initial_file(void)
{
    log_info(logger, "Creando archivo inicial");

    char* file_dir = string_from_format("%s/files/initial_file", PUNTO_MONTAJE);
    char* tag_dir = string_from_format("%s/BASE", file_dir); // -> MOUNT_DIR/files/initial_file/BASE
    char* logical_blocks_dir = string_from_format("%s/logical_blocks", tag_dir); // -> MOUNT_DIR/files/initial_files/BASE/logical_blocks;

    mkdir(file_dir, 0777);
    mkdir(tag_dir, 0777);
    mkdir(logical_blocks_dir, 0777);

    create_metadata_file(tag_dir, fs_cfg->block_size, "[0]", COMMITED);

    char* physical_0 = get_physical_block_path(0);
    char* logical_0 = string_from_format("%s/000000.dat", logical_blocks_dir);

    if (link(physical_0, logical_0) == -1)
    {
        log_error(logger, "Error creando enlace entre bloque físico 0 y bloque lógico 0");
        free(physical_0);
        free(logical_0);
        free(file_dir);
        free(tag_dir);
        free(logical_blocks_dir);
        return false;
    }

    log_debug(logger, "Initial File creado con Hard Link al bloque físico 0");
    log_info(logger, "##0 - initial_file:BASE Se agregó el hard link del bloque lógico 0 al bloque físico 0");

    free(physical_0);
    free(logical_0);
    free(file_dir);
    free(tag_dir);
    free(logical_blocks_dir);

    return true;
}

bool create_physical_blocks(void)
{
    log_debug(logger, "Creando %d bloques físicos", fs_cfg->total_blocks);

    for (int i = 0; i < fs_cfg->total_blocks; i++)
    {
        char* block_path = get_physical_block_path(i);

        FILE* block_file = fopen(block_path, "wb");
        if (!block_file)
        {
            log_error(logger, "Error creando bloque físico número: %d", i);
            free(block_path);
            return false;
        }

        char* empty_block;
        if (i == 0)
        {
            empty_block = malloc(fs_cfg->block_size);
            memset(empty_block, '0', fs_cfg->block_size);
        }
        else
        {
            empty_block = calloc(fs_cfg->block_size, 1);
        }
        
        fwrite(empty_block, 1, fs_cfg->block_size, block_file);
        free(empty_block);

        fclose(block_file);
        free(block_path);
    }
    
    log_info(logger, "Bloques físicos creados correctamente");
    return true;
}

char* get_physical_block_path(int block_num)
{
    return string_from_format("%s/physical_blocks/block%04d.dat", PUNTO_MONTAJE, block_num);
}

char* get_file_path(char* file_name, char* tag_name)
{
    return string_from_format("%s/files/%s/%s", PUNTO_MONTAJE, file_name, tag_name);
}

char* get_metadata_path(char* file_name, char* tag_name)
{
    return string_from_format("%s/files/%s/%s/metadata.config", PUNTO_MONTAJE, file_name, tag_name);
}

char* get_logical_block_path(char* file_name, char* tag_name, int block_num)
{
    return string_from_format("%s/files/%s/%s/logical_blocks/%06d.dat", PUNTO_MONTAJE, file_name, tag_name, block_num);
}

/*void save_metadata_file(t_config* metadata, char* metadata_path, int new_size, char** blocks_list)
{
    FILE* f = fopen(metadata_path, "w");
    if (!f)
    {
        log_error(logger, "Error creando archivo metadata.config");
        return;
    }

    fprintf(f, "TAMAÑO=%d\n", new_size);
    fprintf(f, "BLOCKS=[%s]\n", blocks_list);
    fclose(f);
}*/

// Convierte el char** que devuelve la config en una t_list
t_list* config_array_to_list(char** array) {
    t_list* list = list_create();
    if (!array) return list; // Lista vacía si no hay bloques

    for (int i = 0; array[i] != NULL; i++) {
        list_add(list, strdup(array[i]));
        if (strcmp(array[i], "0") != 0) log_debug(logger, "Se guarda el bloque %s en la lista", array[i]);
    }
    log_debug(logger, "Se guardaron %d bloques en la lista", list_size(list));
    return list;
}

// Convierte la t_list en el string "[1,2,3]" para guardar en config
char* list_to_config_string(t_list* list) {
    char* result = string_new();
    string_append(&result, "[");

    for (int i = 0; i < list_size(list); i++) {
        char* val = (char*)list_get(list, i);
        string_append(&result, val);
        if (i < list_size(list) - 1) {
            string_append(&result, ",");
        }
    }
    string_append(&result, "]");
    log_debug(logger, "Se convierte la lista en el string %s", result);
    return result;
}
