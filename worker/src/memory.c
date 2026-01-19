#include "memory.h"

void *memory_ptr = NULL;
int page_size = 0;
frame_entry_t *frames_table;
t_dictionary *file_tag_table;
int frames_table_entries;
int clock_pointer = 0;
int free_frames = 0;

void clean_frame_entry(int frame, bool log_free, int query_id)
{
    if (log_free)
    {
        char **ft_splited = split_file_tag(frames_table[frame].file_tag);
        log_info(logger, "Query %d: Se libera el Marco: %d perteneciente al - File: %s - Tag: %s", query_id, frame, ft_splited[0], ft_splited[1]);
        free_split_file_tag(ft_splited);
    }
    if (frames_table[frame].file_tag != NULL)
    {
        free(frames_table[frame].file_tag);
        frames_table[frame].file_tag = NULL;
    }
    free_frames++;
    frames_table[frame].present = false;
    frames_table[frame].page = -1;
    frames_table[frame].mod = false;
    frames_table[frame].use = false;
    frames_table[frame].last_reference = 0;
}

void init_memory(int block_size)
{
    file_tag_table = dictionary_create();
    page_size = block_size;
    memory_ptr = malloc(config_worker->tam_memoria);
    frames_table_entries = config_worker->tam_memoria / page_size;
    frames_table = calloc(frames_table_entries, sizeof(frame_entry_t));
    for (int i = 0; i < frames_table_entries; i++)
    {
        clean_frame_entry(i, false, -1);
    }
    if (memory_ptr == NULL)
    {
        log_error(logger, "Error al asignar memoria!");
    }
    else
    {
        log_debug(logger, "Memoria reservada: %d bytes", config_worker->tam_memoria);
    }
}

void destroy_page_table(void *ptr)
{
    page_table *ptable = (page_table *)ptr;
    if (ptable->entries)
    {
        free(ptable->entries);
    }
    free(ptable);
}

void free_memory()
{
    if (memory_ptr != NULL)
    {
        free(memory_ptr);
        memory_ptr = NULL;
    }

    if (frames_table != NULL)
    {
        for (int i = 0; i < frames_table_entries; i++)
        {
            if (frames_table[i].file_tag != NULL)
            {
                free(frames_table[i].file_tag);
                frames_table[i].file_tag = NULL;
            }
        }
        free(frames_table);
        frames_table = NULL;
    }

    if (file_tag_table != NULL)
    {
        dictionary_destroy_and_destroy_elements(file_tag_table, destroy_page_table);
        file_tag_table = NULL;
    }

    log_debug(logger, "Memoria y estructuras administrativas liberadas");
}

void free_translation_result(translation_result_t *result)
{
    if (result && result->segments)
    {
        for (int i = 0; i < result->segments_count; i++)
        {
            free(result->segments[i].content);
        }
        free(result->segments);
        result->segments = NULL;
        result->segments_count = 0;
        result->success = false;
    }
}

page_table *get_page_table_from_file_tag(char *file_tag, int query_id, STATUS_QUERY_OP_CODE *status)
{
    if (file_tag == NULL)
    {
        log_error(logger, "[ERROR] get_page_table_from_file_tag: file_tag es NULL");
        return NULL;
    }

    page_table *ptable = dictionary_get(file_tag_table, file_tag);

    if (ptable == NULL)
    {
        STATUS_QUERY_OP_CODE storage_status;
        int file_size = storage_get_file_tag_size(file_tag, &storage_status, query_id);
        *status = storage_status;
        if (*status == SUCCESSFUL_INSTRUCTION && file_size > 0)
        {
            create_memory(file_tag);
            truncate_memory(file_tag, file_size, query_id);
            ptable = dictionary_get(file_tag_table, file_tag);
        }
        else
        {
            log_error(logger, "[ERROR] get_page_table_from_file_tag: file_tag '%s' no encontrado", file_tag);
            return NULL;
        }
    }

    return ptable;
}

int get_target_frame()
{
    int frame_to_be_replaced = -1;

    if (free_frames > 0)
    {
        for (int i = 0; i < frames_table_entries; i++)
        {
            frame_entry_t frame = frames_table[i];
            if (!frame.present)
            {
                // FIRST FIT
                frame_to_be_replaced = i;
                free_frames--;
                if (strcmp(config_worker->algoritmo_de_reemplazo, "CLOCK-M") == 0)
                {
                    clock_pointer = frame_to_be_replaced + 1 > frames_table_entries - 1 ? 0 : frame_to_be_replaced + 1;
                }
                return frame_to_be_replaced;
            }
        }
        return -1;
    }

    // LRU
    if (strcmp(config_worker->algoritmo_de_reemplazo, "LRU") == 0)
    {
        for (int i = 0; i < frames_table_entries; i++)
        {
            frame_entry_t frame = frames_table[i];
            if (frame_to_be_replaced == -1 || frame.last_reference < frames_table[frame_to_be_replaced].last_reference)
            {
                frame_to_be_replaced = i;
            }
        }
        return frame_to_be_replaced;
    }

    // CLOCK-M
    int clock_cycle = 0;
    int checked_frames = 0;
    if (strcmp(config_worker->algoritmo_de_reemplazo, "CLOCK-M") == 0)
    {
        // CLOCK-M
        // clock_pointer puntero clock
        // 0 primera vuelta busca 0 - 0
        // 1 segunda vuelta busca 0 - 1 y modifica uso por 0
        // 2 tercera vuelta busca 0 - 0
        // 3 cuarta vuelta busca 0 - 1 y modifica uso por 0
        while (true)
        {
            frame_entry_t frame = frames_table[clock_pointer];
            if (clock_cycle == 0 || clock_cycle == 2)
            {
                if (!frame.use && !frame.mod)
                {
                    frame_to_be_replaced = clock_pointer;
                }
            }
            else if (clock_cycle == 1 || clock_cycle == 3)
            {
                if (!frame.use && frame.mod)
                {
                    frame_to_be_replaced = clock_pointer;
                }
                frames_table[clock_pointer].use = false;
            }
            else if (clock_cycle > 3)
            {
                return -1;
            }
            clock_pointer = clock_pointer + 1 > frames_table_entries - 1 ? 0 : clock_pointer + 1;
            if (frame_to_be_replaced != -1)
            {
                return frame_to_be_replaced;
            }

            checked_frames++;
            if (checked_frames == frames_table_entries)
            {
                clock_cycle++;
                checked_frames = 0;
            }
        }
    }
    return -1;
}

// base_dir es dir logica que esta en modo decimal
translation_result_t translate_and_get_physical_ptr(char *file_tag, int base_dir, int size, bool mod, int query_id, bool is_write, void *bytes)
{
    translation_result_t result = {
        .segments = NULL,
        .segments_count = 0,
        .success = false,
        .status_code = ERROR_GENERIC};

    if (size <= 0)
    {
        log_error(logger, "[ERROR] Tamaño inválido: %d", size);
        return result;
    }

    page_table *ptable = get_page_table_from_file_tag(file_tag, query_id, &result.status_code);
    if (!ptable)
    {
        log_error(logger, "[ERROR] No se encontró tabla de páginas para %s", file_tag);
        return result;
    }

    int max_address = (ptable->entries_length * page_size) - 1;
    if (base_dir < 0 || base_dir + size - 1 > max_address)
    {
        log_error(logger, "[ERROR] Dirección fuera de rango: base=%d size=%d max=%d",
                  base_dir, size, max_address);
        // ERROR_OUT_OF_BOUNDS,  // Lectura o escritura fuera de límite
        free(result.segments);
        result.segments = NULL;
        result.segments_count = 0;
        result.success = false;
        result.status_code = ERROR_OUT_OF_BOUNDS;
        return result;
    }

    int start_page = base_dir / page_size;
    // int end_page = (base_dir + size - 1) / page_size;

    dir_tuple_t start_logic_dir_t = dir_decimal_to_tuple(base_dir, page_size);
    dir_tuple_t end_logic_dir_t = dir_decimal_to_tuple((base_dir + size - 1), page_size);
    int total_pages = (end_logic_dir_t.page + 1) - start_logic_dir_t.page;

    log_debug(logger, "[MEM] translate | %s | dir=%d size=%d | páginas %d-%d (%d páginas)",
              file_tag, base_dir, size, start_logic_dir_t.page, end_logic_dir_t.page, total_pages);

    result.segments = malloc(total_pages * sizeof(physical_segment_t));

    int bytes_processed = 0;
    int offset_in_page = base_dir % page_size;

    for (int i = 0; i < total_pages; i++)
    {
        int current_page = start_page + i;
        page_entry_t *page = &ptable->entries[current_page];

        if (!page->present)
        {
            log_debug(logger, "[MEM] Page fault en página %d de %s", current_page, file_tag);

            char **current_ft_splited = split_file_tag(file_tag);
            log_info(logger, "Query %d: - Memoria Miss - File: %s - Tag: %s - Pagina: %d", query_id, current_ft_splited[0], current_ft_splited[1], current_page);
            int target_frame = get_target_frame();
            if (target_frame == -1)
            {
                log_error(logger, "[ERROR] No funciono el remplazo de paginas");
                free_split_file_tag(current_ft_splited);
                free(result.segments);
                result.segments = NULL;
                result.segments_count = 0;
                result.success = false;
                result.status_code = ERROR_GENERIC;
                return result;
            }

            log_debug(logger, "[DEBUG] Frame %d state: present=%d, mod=%d, use=%d, page=%d, file_tag=%s",
                      target_frame, frames_table[target_frame].present, frames_table[target_frame].mod,
                      frames_table[target_frame].use, frames_table[target_frame].page, frames_table[target_frame].file_tag ? frames_table[target_frame].file_tag : "(null)");
            // target_frame page
            page_table *victim_page_pt = frames_table[target_frame].present
                                             ? get_page_table_from_file_tag(frames_table[target_frame].file_tag, query_id, &result.status_code)
                                             : NULL;
            if (victim_page_pt != NULL)
            {
                int victim_page_num = frames_table[target_frame].page;
                victim_page_pt->entries[victim_page_num].present = false;
                log_info(logger, "Query %d: Se libera el Marco: %d perteneciente al - File: %s - Tag: %s", query_id, target_frame, current_ft_splited[0], current_ft_splited[1]);
                victim_page_pt->entries[victim_page_num].frame = -1;
                log_info(logger, "## Query %d: Se reemplaza la página %s/%d por la %s/%d", query_id, frames_table[target_frame].file_tag, victim_page_num, file_tag, current_page);
            }

            if (frames_table[target_frame].mod)
            {
                void *block_content = (char *)memory_ptr + (target_frame * page_size);
                int block_num = frames_table[target_frame].page;
                char *victim_file_tag = frames_table[target_frame].file_tag;
                STATUS_QUERY_OP_CODE status_code = storage_write(victim_file_tag, block_num, block_content, query_id);
                log_debug(logger, "[MEM] FLUSH: Enviando bloque %d de %s al Storage",
                          block_num, victim_file_tag);
                if (status_code != SUCCESSFUL_INSTRUCTION)
                {
                    log_error(logger, "[MEM] WRITE: Error escribiendo bloque %d de %s al Storage",
                              block_num, victim_file_tag);
                    // ERROR_NO_SPACE_AVAILABLE,         // Espacio Insuficiente (sin bloques físicos)
                    // ERROR_FILE_TAG_IS_COMMITTED,      // Escritura no permitida (Archivo está COMMITED)
                    // ERROR_OUT_OF_BOUNDS,              // Lectura o escritura fuera de límite
                    free_split_file_tag(current_ft_splited);
                    free(result.segments);
                    result.segments = NULL;
                    result.segments_count = 0;
                    result.success = false;
                    result.status_code = status_code;
                    return result;
                }
            }

            STATUS_QUERY_OP_CODE status_code;
            void *block_content = storage_read(file_tag, current_page, &status_code, query_id);
            log_debug(logger, "[MEM] READ: Pidiendo bloque %d de %s al Storage",
                      current_page, file_tag);
            if (block_content != NULL)
            {
                log_info(logger, "Query %d: - Memoria Add - File: %s - Tag: %s - Pagina: %d - Marco: %d", query_id, current_ft_splited[0], current_ft_splited[1], current_page, target_frame);
                memcpy((char *)memory_ptr + (target_frame * page_size), block_content, page_size);
                free(block_content);
            }
            else
            {
                log_error(logger, "[MEM] READ: Error pidiendo bloque %d de %s al Storage",
                          current_page, file_tag);
                // ERROR_OUT_OF_BOUNDS,  // Lectura o escritura fuera de límite
                free_split_file_tag(current_ft_splited);
                free(result.segments);
                result.segments = NULL;
                result.segments_count = 0;
                result.success = false;
                result.status_code = status_code;
                return result;
            }
            log_info(logger, "Query %d: Se asigna el Marco: %d a la Página: %d perteneciente al - File: %s - Tag: %s", query_id, target_frame, current_page, current_ft_splited[0], current_ft_splited[1]);
            free_split_file_tag(current_ft_splited);

            page->present = true;
            page->frame = target_frame;
            page->mod = false;
            frames_table[target_frame].present = true;
            frames_table[target_frame].page = current_page;
            if (frames_table[target_frame].file_tag != NULL)
                free(frames_table[target_frame].file_tag);
            frames_table[target_frame].file_tag = strdup(file_tag);
            frames_table[target_frame].use = true;
            frames_table[target_frame].mod = false;
        }

        frame_entry_t *frame = &frames_table[page->frame];
        frame->last_reference = get_timestamp();
        frame->use = true;
        if (mod)
        {
            page->mod = true;
            frame->mod = true;
        }

        int available_bytes = page_size - offset_in_page;
        int remaing_bytes = size - bytes_processed;
        int bytes_in_current_page = (remaing_bytes <= available_bytes)
                                        ? (remaing_bytes)
                                        : available_bytes;

        int physical_addr = (page->frame * page_size) + offset_in_page;

        result.segments[i].size = bytes_in_current_page;
        result.segments[i].page_num = current_page;
        result.segments[i].content = malloc(bytes_in_current_page);

        if(is_write){
            memcpy((char *)memory_ptr + physical_addr, bytes + bytes_processed, bytes_in_current_page);
            memcpy(result.segments[i].content, (char *)memory_ptr + physical_addr, bytes_in_current_page);
            log_info(logger, "Query %d: Acción: ESCRIBIR - Dirección Física: %d - Valor: %.*s", 
                     query_id, physical_addr, bytes_in_current_page, (char *)result.segments[i].content);
        }else{
            memcpy(result.segments[i].content, (char *)memory_ptr + physical_addr, bytes_in_current_page);
            log_info(logger, "Query %d: Acción: LEER - Dirección Física: %d - Valor: %.*s", 
                     query_id, physical_addr, bytes_in_current_page, (char *)result.segments[i].content);
        }


        log_debug(logger, "[MEM] Segmento %d: página %d → frame %d, offset %d, size %d",
                  i, current_page, page->frame, offset_in_page, bytes_in_current_page);

        bytes_processed += bytes_in_current_page;
        offset_in_page = 0;
    }

    result.segments_count = total_pages;
    result.success = true;
    result.status_code = SUCCESSFUL_INSTRUCTION;

    return result;
}

// base_dir es dir logica que esta en modo decimal
void *read_memory(char *file_tag, int base_dir, int size, int query_id, STATUS_QUERY_OP_CODE *status_code)
{
    translation_result_t translations = translate_and_get_physical_ptr(file_tag, base_dir, size, false, query_id, false, NULL);
    *status_code = translations.status_code;
    if (!translations.success)
    {
        log_error(logger, "[ERROR] Falló la traducción de direcciones");
        return NULL;
    }

    void *buffer = malloc(size);
    if (buffer == NULL)
    {
        log_error(logger, "[ERROR] No se pudo asignar buffer de lectura de %d bytes (Out of Memory)", size);
        free_translation_result(&translations);
        *status_code = ERROR_GENERIC;
        return NULL;
    }

    // Leer cada segmento y copiar al buffer
    int buffer_offset = 0;

    for (int i = 0; i < translations.segments_count; i++)
    {
        physical_segment_t *seg = &translations.segments[i];
        memcpy((char *)buffer + buffer_offset, seg->content , seg->size);
        buffer_offset += seg->size;
    }
    free_translation_result(&translations);

    log_info(logger, "Contenido leído de memoria para READ %s dir_base=%d tam=%d: %.*s",
             file_tag, base_dir, size, size, (char *)buffer);

    return buffer;
}

bool write_memory(char *file_tag, int base_dir, void *bytes, int size, int query_id, STATUS_QUERY_OP_CODE *status_code)
{
    if (!bytes || size <= 0)
    {
        log_error(logger, "[ERROR] Parámetros inválidos para write_memory");
        return false;
    }

    // Traducir y obtener todas las direcciones físicas (con mod=true)
    translation_result_t trans = translate_and_get_physical_ptr(file_tag, base_dir, size, true, query_id, true, bytes);
    *status_code = trans.status_code;
    if (!trans.success)
    {
        log_error(logger, "[ERROR] Falló la traducción de direcciones");
        return false;
    }

    // Escribir cada segmento desde el buffer
    // Ya se escribio en translate_and_get_physical_ptr
    void *buffer = malloc(size); // Malloc para el log
    int buffer_offset = 0;
    for (int i = 0; i < trans.segments_count; i++)
    {
        physical_segment_t *seg = &trans.segments[i];
        memcpy((char *)buffer + buffer_offset, seg->content , seg->size);
        buffer_offset += seg->size;
    }

    log_info(logger, "Contenido escrito en memoria para WRITE %s dir_base=%d tam=%d: %.*s", 
             file_tag, base_dir, size, size, (char *)buffer);

    free_translation_result(&trans);

    log_debug(logger, "[MEM] write_memory completo: %d bytes escritos", size);
    return true;
}

STATUS_QUERY_OP_CODE flush_memory(char *file_tag, int query_id)
{
    log_debug(logger, "[MEM] FLUSH iniciado para %s", file_tag);

    STATUS_QUERY_OP_CODE status_code = SUCCESSFUL_INSTRUCTION;
    page_table *ptable = get_page_table_from_file_tag(file_tag, query_id, &status_code);
    if (!ptable)
    {
        log_error(logger, "[ERROR] flush_memory: No se encontró tabla de páginas para %s", file_tag);
        return status_code;
    }

    int pages_flushed = 0;

    for (int i = 0; i < ptable->entries_length; i++)
    {
        page_entry_t *page = &ptable->entries[i];

        if (page->present && page->mod)
        {
            int frame_num = page->frame;
            frame_entry_t *frame = &frames_table[frame_num];

            void *block_content = (char *)memory_ptr + (frame_num * page_size);
            int block_num = i;

            status_code = storage_write(file_tag, block_num, block_content, query_id);
            log_debug(logger, "[MEM] FLUSH: Enviando bloque %d de %s al Storage (frame %d)",
                      block_num, file_tag, frame_num);
            if (status_code != SUCCESSFUL_INSTRUCTION)
            {
                log_error(logger, "[MEM] FLUSH: Error  escribiendo bloque %d de %s al Storage (frame %d)",
                          block_num, file_tag, frame_num);
                return status_code;
            }

            page->mod = false;
            frame->mod = false;

            pages_flushed++;
        }
    }

    log_debug(logger, "[MEM] FLUSH completado para %s: %d página(s) escrita(s) a Storage",
              file_tag, pages_flushed);

    return status_code;
}

// primero hacer flush para persistir los cambios en storage
bool free_file_tag(char *file_tag, int query_id)
{
    log_debug(logger, "[MEM] Iniciando liberación de recursos para: %s", file_tag);

    page_table *ptable = dictionary_get(file_tag_table, file_tag);

    if (ptable == NULL)
    {
        log_error(logger, "[ERROR] free_file_tag: file_tag '%s' no encontrado en el sistema.", file_tag);
        return false;
    }

    if (ptable->entries != NULL)
    {
        for (int i = 0; i < ptable->entries_length; i++)
        {
            if (ptable->entries[i].present)
            {
                int frame_idx = ptable->entries[i].frame;
                clean_frame_entry(frame_idx, true, query_id);

                log_debug(logger, "[MEM] Frame %d liberado (pertenecía a página %d de %s)",
                          frame_idx, i, file_tag);
            }
        }
        free(ptable->entries);
        ptable->entries = NULL;
    }

    free(ptable);

    dictionary_remove(file_tag_table, file_tag);
    log_debug(logger, "[MEM] Estructuras de '%s' eliminadas y memoria liberada.", file_tag);
    return true;
}

bool create_memory(char *file_tag)
{
    if (file_tag == NULL)
    {
        log_error(logger, "[ERROR] create_memory: file_tag es NULL");
        return false;
    }

    page_table *ptable = malloc(sizeof(page_table));
    ptable->entries_length = 0;
    ptable->entries = NULL;

    dictionary_put(file_tag_table, file_tag, ptable);
    return true;
}

bool truncate_memory(char *file_tag, int size, int query_id)
{
    // + page_size-1, round ceil
    int pages_needed = (size + page_size - 1) / page_size;

    page_table *ptable = dictionary_get(file_tag_table, file_tag);
    if (ptable == NULL)
    {
        log_error(logger, "[ERROR] truncate_memory: file_tag %s no encontrado", file_tag);
        return false;
    }

    int old_pages = ptable->entries_length;
    page_entry_t *entries = (page_entry_t *)ptable->entries;

    if (pages_needed < old_pages)
    {
        for (int i = pages_needed; i < old_pages; i++)
        {
            if (entries[i].present)
                clean_frame_entry(entries[i].frame, true, query_id);
        }
    }

    page_entry_t *temp_entries = realloc(entries, sizeof(page_entry_t) * pages_needed);

    if (temp_entries == NULL && pages_needed > 0)
    {
        log_error(logger, "Falló realloc en truncate_memory. Sin memoria.");
        return false;
    }

    entries = temp_entries;
    ptable->entries = entries;
    ptable->entries_length = pages_needed;

    if (pages_needed > old_pages)
    {
        for (int i = old_pages; i < pages_needed; i++)
        {
            entries[i].present = false;
            entries[i].frame = -1;
            entries[i].mod = false;
        }
    }

    log_debug(logger, "[MEM] truncate_memory | %s | old=%d new=%d",
              file_tag, old_pages, pages_needed);

    return true;
}

dir_tuple_t dir_decimal_to_tuple(int decimal_dir, int page_size)
{
    dir_tuple_t result;
    result.page = decimal_dir / page_size;
    result.offset = decimal_dir % page_size;
    return result;
}

int dir_tuple_to_decimal(dir_tuple_t logic_dir, int page_size)
{
    return (logic_dir.page * page_size) + logic_dir.offset;
}

int logic_dir_to_real(dir_tuple_t logic_dir, page_entry_t page)
{
    return (page_size * page.frame) + logic_dir.offset;
}

void test_clock_m_replacement()
{
    log_debug(logger, "\n[TEST] --- Iniciando Prueba CLOCK-M ---");

    // Variable para capturar errores
    STATUS_QUERY_OP_CODE status_code;

    // 1. Configurar Algoritmo
    config_worker->algoritmo_de_reemplazo = "CLOCK-M";
    log_debug(logger, "[TEST] Algoritmo seteado a: %s", config_worker->algoritmo_de_reemplazo);

    char *file_tag = "CLOCK_TEST:0";
    create_memory(file_tag);

    // 2. Truncar (espacio lógico)
    truncate_memory(file_tag, 128, 0);

    // 3. LLENAR MEMORIA (Ocupar Frames Físicos)
    // Usamos READ primero para cargar todos con (Use=1, Mod=0)
    log_debug(logger, "[TEST] Llenando memoria con READs (Estado inicial U=1, M=0)...");
    for (int i = 0; i < 8; i++)
    {
        read_memory(file_tag, i * 16, 1, 0, &status_code);
    }

    // 4. Configurar escenario CLOCK-M
    // Queremos diferenciar las páginas.
    // Modificamos (Write) las paginas 0, 2, 3, 4, 5, 6, 7.
    // Dejamos la Pagina 1 SOLO LEIDA (U=1, M=0).

    log_debug(logger, "[TEST] Modificando Frames 0 y 2-7 (Pasan a U=1, M=1). Frame 1 queda intacto.");
    char *data = "MOD";
    write_memory(file_tag, 0, data, 4, 0, &status_code); // Pag 0 -> (1, 1)
    // Pag 1 se salta -> Queda (1, 0)
    for (int i = 2; i < 8; i++)
    {
        write_memory(file_tag, i * 16, data, 4, 0, &status_code); // Pag 2-7 -> (1, 1)
    }

    // ESTADO ESPERADO DE FRAMES (Asumiendo asignacion 0..7 lineal):
    // F0: (1,1)
    // F1: (1,0)  <-- Debería ser la víctima más probable tras limpiar bits
    // F2: (1,1)
    // ...
    // F7: (1,1)

    // ANÁLISIS DE PASOS DEL ALGORITMO:
    // Paso 1 (Busca 0,0): No encuentra (todos tienen U=1).
    // Paso 2 (Busca 0,1 + baja U=0):
    //    - F0 (1,1) -> U=0. Queda (0,1).
    //    - F1 (1,0) -> U=0. Queda (0,0).
    //    - F2..F7 (1,1) -> U=0. Quedan (0,1).
    // Vuelta completa.
    // Paso 3 (Busca 0,0 de nuevo):
    //    - F0 es (0,1). No.
    //    - F1 es (0,0). VICTIMA.

    // 5. Forzar reemplazo
    log_debug(logger, "[TEST] Forzando reemplazo CLOCK-M (pidiendo pagina 9)...");
    truncate_memory(file_tag, 144, 0);

    log_debug(logger, "[TEST] EXPECTATIVA: Frame 1 debería ser la víctima (único que no fue modificado).");
    write_memory(file_tag, 128, "VICTIMA", 8, 0, &status_code);
}

void test_complex_clock_m_replacement()
{
    log_debug(logger, "\n[TEST] --- Iniciando Prueba CLOCK-M - Compleja ---");

    STATUS_QUERY_OP_CODE status_code;

    // 1. Configurar Algoritmo
    config_worker->algoritmo_de_reemplazo = "CLOCK-M";
    log_debug(logger, "[TEST] Algoritmo seteado a: %s", config_worker->algoritmo_de_reemplazo);

    char *file_tag_A = "CLOCK_TEST_A:0";
    create_memory(file_tag_A);
    char *file_tag_B = "CLOCK_TEST_B:0";
    create_memory(file_tag_B);
    char *data = "MOD";
    int data_size = 4;

    truncate_memory(file_tag_A, 256, 0);
    truncate_memory(file_tag_B, 1024, 0);

    // instante -1
    // TAG A- r-w-w-w
    read_memory(file_tag_A, 0 * page_size, data_size, 0, &status_code);
    write_memory(file_tag_A, 1 * page_size, data, data_size, 0, &status_code);
    write_memory(file_tag_A, 2 * page_size, data, data_size, 0, &status_code);
    write_memory(file_tag_A, 3 * page_size, data, data_size, 0, &status_code);
    // TAG B- r
    read_memory(file_tag_B, 0 * page_size, data_size, 0, &status_code);
    // TAG A- w
    write_memory(file_tag_A, 4 * page_size, data, data_size, 0, &status_code);
    // TAG B- w -w
    write_memory(file_tag_B, 1 * page_size, data, data_size, 0, &status_code);
    write_memory(file_tag_B, 2 * page_size, data, data_size, 0, &status_code);

    // instante 0 W B4 - deberia remplazar frame 0
    write_memory(file_tag_B, 3 * page_size, data, data_size, 0, &status_code);
    // instante 1 R B1 - no remplaza, cambia bit uso de 0 a 1
    read_memory(file_tag_B, 0 * page_size, data_size, 0, &status_code);
    // instante 2 R A2 - no remplaza, cambia bit uso de 0 a 1
    read_memory(file_tag_A, 1 * page_size, data_size, 0, &status_code);
    // instante 3 R A6 - tiene que remplazar, en frame 2 sacando al A3, osea pagina 2 de filte:tag A
    read_memory(file_tag_A, 5 * page_size, data_size, 0, &status_code);

    // anda :d
}

void test_read_and_write()
{

    char *file_tag = "SCRIPTS:0";
    STATUS_QUERY_OP_CODE status_code;

    // 1. Crear estructuras administrativas para un archivo
    log_debug(logger, "\n[TEST] 1. Creando archivo: %s", file_tag);
    create_memory(file_tag);

    // 2. Truncar archivo (Asignar páginas)
    // Pedimos 40 bytes. Si pag_size=16, necesitamos 3 páginas (16+16+8)
    log_debug(logger, "[TEST] 2. Truncando a 40 bytes...");
    truncate_memory(file_tag, 40, 0);

    // 3. Prueba de Escritura Simple (Dentro de una misma página)
    log_debug(logger, "\n[TEST] 3. Escritura Simple (Pagina 0)");
    char *data_simple = "HOLA MUNDO";
    // Escribimos en dir lógica 0
    write_memory(file_tag, 0, data_simple, strlen(data_simple) + 1, 0, &status_code);

    // 4. Prueba de Lectura Simple
    log_debug(logger, "[TEST] 4. Lectura Simple");
    void *leido = read_memory(file_tag, 0, strlen(data_simple) + 1, 0, &status_code);
    if (leido)
    {
        log_debug(logger, "   -> Leído: '%s' (Esperado: '%s')", (char *)leido, data_simple);
        free(leido);
    }

    // 5. Prueba Cross-Page (Escritura que cruza de pág 0 a pág 1)
    log_debug(logger, "\n[TEST] 5. Escritura Cross-Page (Cruzando límites de página)");
    // Pag size es 16. Escribimos en dir 10.
    // Pag 0: bytes 10 al 15 (6 bytes)
    // Pag 1: bytes 16 al ...
    char *data_cross = "DATA_QUE_CRUZA_PAGINAS";
    write_memory(file_tag, 10, data_cross, strlen(data_cross) + 1, 0, &status_code);

    // 6. Lectura Cross-Page
    log_debug(logger, "[TEST] 6. Lectura Cross-Page");
    leido = read_memory(file_tag, 10, strlen(data_cross) + 1, 0, &status_code);
    if (leido)
    {
        log_debug(logger, "   -> Leído: '%s'", (char *)leido);
        if (strcmp((char *)leido, data_cross) == 0)
            log_debug(logger, "## Lectura OK: '%s'", (char *)leido);
        else
            log_error(logger, "## Lectura FALLIDA: leido='%s' esperado='%s'", (char *)leido, data_cross);
        free(leido);
    }

    // 7. Modificación (Write sobre datos existentes)
    log_debug(logger, "\n[TEST] 7. Modificación (Overwrite)");
    // Modificamos al final de la pagina 0, e inicio de la pagina 1 (dir 15), sobrecribimos "QUE" por "XXX"
    write_memory(file_tag, 15, "XXX", 3, 0, &status_code);

    leido = read_memory(file_tag, 10, strlen(data_cross) + 1, 0, &status_code);
    log_debug(logger, "   -> Datos después de modificar: '%s'", (char *)leido);
    // Debería verse algo como "DATA_XXX_CRUZA_PAGINAS"
    free(leido);

    log_debug(logger, "\n[TEST] 8. Prueba de Acceso Fuera de Límites (Segmentation Fault simulado)");

    char *data_fail = "E";
    log_debug(logger, "   -> Intentando escribir en dirección lógica %d (Fuera de rango)...", 48);
    write_memory(file_tag, 48, data_fail, 1, 0, &status_code);

    log_debug(logger, "\n[TEST] 9. Intentando FLUSH");

    flush_memory(file_tag, 0);

    log_debug(logger, "\n[TEST] 10. Intentando Free/Remove FilteTAG");
    free_file_tag(file_tag, 0);
}

void run_memory_tests()
{

    // Page size 16
    // Size Memory 128
    log_debug(logger, "\n[TEST] --- Iniciando Pruebas de Memoria Worker---");

    // test_read_and_write();
    // test_lru_replacement();
    // test_clock_m_replacement();
    test_complex_clock_m_replacement();

    log_debug(logger, "\n[TEST] --- Fin de Pruebas ---");
}

static int _flush_all_pid_aux = -1;

void _iterator_flush_helper(char *key, void *value)
{

    if (key != NULL)
    {
        flush_memory(key, _flush_all_pid_aux);
    }
}

void flush_all_file_tags(int query_id)
{
    log_debug(logger, "## Query %d: Ejecutando FLUSH de todos los archivos modificados por desalojo...", query_id);

    _flush_all_pid_aux = query_id;

    dictionary_iterator(file_tag_table, _iterator_flush_helper);

    _flush_all_pid_aux = -1;
}