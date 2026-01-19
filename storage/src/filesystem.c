#include <filesystem.h>
#include <dictionary_monitor.h>

t_config *superblock;
t_config *hash_index;
t_storage_fs *fs_cfg;
pthread_mutex_t MUTEX_HASH_INDEX;

bool create_default_superblock(char *superblock_path)
{
    // Crear el directorio del punto de montaje si no existe
    struct stat st = {0};
    if (stat(PUNTO_MONTAJE, &st) == -1)
    {
        if (mkdir(PUNTO_MONTAJE, 0777) != 0)
        {
            log_error(logger, "Error creando directorio del punto de montaje: %s", PUNTO_MONTAJE);
            return false;
        }
        log_info(logger, "Directorio del punto de montaje creado: %s", PUNTO_MONTAJE);
    }

    FILE *superblock_file = fopen(superblock_path, "w");
    if (!superblock_file)
    {
        log_error(logger, "Error creando archivo superblock.config - La ruta no existe o no es correcta");
        return false;
    }

    // Intentar leer valores desde storage.config
    t_config *main_config = config_create(system_config_path);
    int fs_size = 65536;
    int block_size = 16;

    if (main_config)
    {
        if (config_has_property(main_config, "FS_SIZE"))
        {
            fs_size = config_get_int_value(main_config, "FS_SIZE");
        }
        if (config_has_property(main_config, "BLOCK_SIZE"))
        {
            block_size = config_get_int_value(main_config, "BLOCK_SIZE");
        }
        config_destroy(main_config);
    }

    fprintf(superblock_file, "# Configuración del File System\n");
    fprintf(superblock_file, "# Archivo generado automáticamente\n\n");
    fprintf(superblock_file, "FS_SIZE=%d\n", fs_size);
    fprintf(superblock_file, "BLOCK_SIZE=%d\n", block_size);

    fclose(superblock_file);

    log_info(logger, "Superblock creado: FS_SIZE=%d, BLOCK_SIZE=%d", fs_size, block_size);
    return true;
}

bool init_superblock(void)
{
    char *superblock_path = string_from_format("%s/superblock.config", PUNTO_MONTAJE);

    if (access(superblock_path, F_OK) != 0)
    {
        log_warning(logger, "No se encontró superblock.config en %s, creando uno nuevo con valores por defecto", PUNTO_MONTAJE);

        if (!create_default_superblock(superblock_path))
        {
            log_error(logger, "Error creando superblock por defecto");
            free(superblock_path);
            return false;
        }
    }

    superblock = config_create(superblock_path);
    free(superblock_path);

    if (!superblock)
    {
        log_error(logger, "Error al leer superblock.config");
        return false;
    }

    if (!config_has_property(superblock, "FS_SIZE") || !config_has_property(superblock, "BLOCK_SIZE"))
    {
        log_error(logger, "Faltan parámetros en superblock.config");
        return false;
    }

    log_debug(logger, "Superblock con clave: %s y valor: %s", "FS_SIZE", config_get_string_value(superblock, "FS_SIZE"));
    log_debug(logger, "Superblock con clave: %s y valor: %s", "BLOCK_SIZE", config_get_string_value(superblock, "BLOCK_SIZE"));

    fs_cfg = malloc(sizeof(t_storage_fs));

    fs_cfg->fs_size = config_get_int_value(superblock, "FS_SIZE");
    fs_cfg->block_size = config_get_int_value(superblock, "BLOCK_SIZE");
    fs_cfg->total_blocks = fs_cfg->fs_size / fs_cfg->block_size;

    log_info(logger, "Superblock cargado correctamente");
    return true;
}

bool init_filesystem(void)
{
    if (FRESH_START)
    {
        log_info(logger, "FRESH_START=TRUE - Formateando filesystem...");
        return format_filesystem();
    }
    else
    {
        log_info(logger, "FRESH_START=FALSE - Cargando filesystem existente...");
        return load_existing_filesystem();
    }
}

bool format_filesystem(void)
{
    log_debug(logger, "Formateando filesystem");

    char *bitmap_path = string_from_format("%s/bitmap.bin", PUNTO_MONTAJE);
    char *hash_index_path = string_from_format("%s/blocks_hash_index.config", PUNTO_MONTAJE);
    char *physical_blocks_path = string_from_format("%s/physical_blocks", PUNTO_MONTAJE);
    char *files_path = string_from_format("%s/files", PUNTO_MONTAJE);

    remove(bitmap_path);
    remove(hash_index_path);
    remove_directory_recursive(physical_blocks_path);
    remove_directory_recursive(files_path);

    free(bitmap_path);
    free(hash_index_path);
    free(physical_blocks_path);
    free(files_path);

    if (!create_directory_structure())
    {
        log_error(logger, "Error creando estructura de directorios");
        return false;
    }

    init_block_bitmap(fs_cfg->fs_size, fs_cfg->block_size);

    if (!init_hash_index())
    {
        log_error(logger, "Error inicializando hash index");
        return false;
    }

    if (!create_physical_blocks())
    {
        log_error(logger, "Error creando bloques físicos");
        return false;
    }

    bitarray_set_bit(block_bitmap, 0);

    char *empty_block = malloc(fs_cfg->block_size);
    memset(empty_block, '0', fs_cfg->block_size);
    add_hash_block(0, empty_block);

    char *debug_initial = malloc(fs_cfg->block_size + 1);
    memcpy(debug_initial, empty_block, fs_cfg->block_size);
    debug_initial[fs_cfg->block_size] = '\0';
    log_debug(logger, "Contenido initial_file: %s", debug_initial);
    free(debug_initial);

    free(empty_block);

    if (!create_initial_file())
    {
        log_error(logger, "Error creando archivo inicial");
        return false;
    }

    log_info(logger, "Filesystem formateado correctamente");

    return true;
}

bool load_existing_filesystem(void)
{
    log_debug(logger, "Cargando filesystem existente");

    load_bitmap();

    char *hash_index_path = string_from_format("%s/blocks_hash_index.config", PUNTO_MONTAJE);
    hash_index = config_create(hash_index_path);
    pthread_mutex_init(&MUTEX_HASH_INDEX, NULL);
    free(hash_index_path);

    if (!hash_index)
    {
        log_error(logger, "Error al cargar el hash_index existente");
        return false;
    }

    // Recrear locks para todos los file:tag existentes
    char *files_path = string_from_format("%s/files", PUNTO_MONTAJE);
    DIR *files_dir = opendir(files_path);
    
    if (files_dir)
    {
        struct dirent *file_entry;
        while ((file_entry = readdir(files_dir)) != NULL)
        {
            if (strcmp(file_entry->d_name, ".") == 0 || strcmp(file_entry->d_name, "..") == 0)
                continue;

            char *file_path = string_from_format("%s/%s", files_path, file_entry->d_name);
            struct stat st;
            
            if (stat(file_path, &st) == 0 && S_ISDIR(st.st_mode))
            {
                // Es un directorio de archivo, buscar tags dentro
                DIR *tags_dir = opendir(file_path);
                if (tags_dir)
                {
                    struct dirent *tag_entry;
                    while ((tag_entry = readdir(tags_dir)) != NULL)
                    {
                        if (strcmp(tag_entry->d_name, ".") == 0 || strcmp(tag_entry->d_name, "..") == 0)
                            continue;

                        char *tag_path = string_from_format("%s/%s", file_path, tag_entry->d_name);
                        struct stat tag_st;
                        
                        if (stat(tag_path, &tag_st) == 0 && S_ISDIR(tag_st.st_mode))
                        {
                            // Crear lock para este file:tag
                            create_lock_file_tag(file_entry->d_name, tag_entry->d_name);
                            log_debug(logger, "Lock recreado para %s:%s", file_entry->d_name, tag_entry->d_name);
                        }
                        free(tag_path);
                    }
                    closedir(tags_dir);
                }
            }
            free(file_path);
        }
        closedir(files_dir);
    }
    free(files_path);

    log_info(logger, "Filesystem existente cargado correctamente");

    return true;
}

void remove_directory_recursive(char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char *full_path = string_from_format("%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                remove_directory_recursive(full_path);
            }
            else
            {
                remove(full_path);
            }
        }
        free(full_path);
    }
    closedir(dir);
    rmdir(path);
}

bool init_hash_index(void)
{
    char *hash_index_path = string_from_format("%s/blocks_hash_index.config", PUNTO_MONTAJE);

    FILE *hash_file = fopen(hash_index_path, "w");
    if (!hash_file)
    {
        log_error(logger, "Error creando hash index");
        free(hash_index_path);
        return false;
    }
    fclose(hash_file);

    pthread_mutex_init(&MUTEX_HASH_INDEX, NULL);
    hash_index = config_create(hash_index_path);
    free(hash_index_path);

    log_info(logger, "Hash index inicializado");
    return hash_index != NULL;
}

bool add_hash_block(int block_num, char *content)
{
    char *hash = crypto_md5(content, fs_cfg->block_size);
    char *block_name = string_from_format("block%04d", block_num);

    config_set_value(hash_index, hash, block_name);
    config_save(hash_index);

    log_debug(logger, "Hash agregado: %s = %s", hash, block_name);

    free(hash);
    free(block_name);

    return true;
}

char *read_physical_block(int block_id)
{
    char *path = get_physical_block_path(block_id);
    FILE *file = fopen(path, "rb");

    if (!file)
    {
        log_error(logger, "Error al abrir el bloque físico %d en %s", block_id, path);
        free(path);
        return NULL;
    }

    // Usamos calloc para inicializar en 0 binario.
    // El contenido (incluyendo padding '0') debe venir del archivo físico si fue escrito correctamente.
    char *buffer = calloc(1, fs_cfg->block_size);
    if (!buffer)
    {
        log_error(logger, "Error de memoria al leer bloque %d", block_id);
        fclose(file);
        free(path);
        return NULL;
    }

    fread(buffer, 1, fs_cfg->block_size, file);
    fclose(file);
    free(path);
    return buffer;
}

char *get_block_md5(int block_id)
{
    char *path = get_physical_block_path(block_id);
    FILE *file = fopen(path, "r");
    if (!file)
    {
        log_error(logger, "Error al abrir el bloque físico %d", block_id);
        free(path);
        return NULL;
    }

    void *buffer = malloc(fs_cfg->block_size);
    fread(buffer, 1, fs_cfg->block_size, file);
    fclose(file);
    free(path);

    char *hash = crypto_md5(buffer, fs_cfg->block_size);
    free(buffer);

    return hash;
}

void remove_block_from_hash_index_unlocked(int block_id)
{
    char *block_hash = get_block_md5(block_id);
    if (block_hash && config_has_property(hash_index, block_hash))
    {
        char *stored_block = config_get_string_value(hash_index, block_hash);
        int stored_id;
        sscanf(stored_block, "block%d", &stored_id);
        
        if (stored_id == block_id)
        {
            config_remove_key(hash_index, block_hash);
            config_save(hash_index);
            log_debug(logger, "Hash removido del hash_index para bloque %d", block_id);
        }
    }
    
    if (block_hash)
        free(block_hash);
}

void remove_block_from_hash_index(int block_id)
{
    pthread_mutex_lock(&MUTEX_HASH_INDEX);
    remove_block_from_hash_index_unlocked(block_id);
    pthread_mutex_unlock(&MUTEX_HASH_INDEX);
}
