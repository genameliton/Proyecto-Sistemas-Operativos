#include <bitarray_monitor.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

t_bitarray* block_bitmap;
pthread_mutex_t MUTEX_BLOCK_BITMAP;

static void* bitmap_mmap = NULL;
static int bitmap_size = 0;

void init_block_bitmap(int fs_size, int block_size)
{
    pthread_mutex_init(&MUTEX_BLOCK_BITMAP, NULL);
    bitmap_size = (fs_cfg->total_blocks + 7) / 8;

    char* bitmap_path = string_from_format("%s/bitmap.bin", PUNTO_MONTAJE);

    // Abrir o crear el archivo
    int fd = open(bitmap_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        log_error(logger, "Error abriendo bitmap.bin");
        free(bitmap_path);
        exit(1);
    }

    // Ajustar tamaño del archivo
    if (ftruncate(fd, bitmap_size) == -1)
    {
        log_error(logger, "Error ajustando tamaño del bitmap");
        close(fd);
        free(bitmap_path);
        exit(1);
    }

    // Mapear archivo a memoria
    bitmap_mmap = mmap(NULL, bitmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bitmap_mmap == MAP_FAILED)
    {
        log_error(logger, "Error en mmap del bitmap");
        close(fd);
        free(bitmap_path);
        exit(1);
    }

    // Crear el bitarray con el mapeo
    block_bitmap = bitarray_create_with_mode(bitmap_mmap, bitmap_size, LSB_FIRST);

    close(fd); // El mmap sigue existiendo
    free(bitmap_path);

    log_info(logger, "Bitmap inicializado con mmap (%d bytes)", bitmap_size);
}

void load_bitmap(void)
{
    pthread_mutex_init(&MUTEX_BLOCK_BITMAP, NULL);
    bitmap_size = (fs_cfg->total_blocks + 7) / 8;

    char* bitmap_path = string_from_format("%s/bitmap.bin", PUNTO_MONTAJE);

    int fd = open(bitmap_path, O_RDWR);
    if (fd == -1)
    {
        log_error(logger, "Error abriendo bitmap.bin para cargar");
        free(bitmap_path);
        return;
    }

    // Mapear archivo existente a memoria
    bitmap_mmap = mmap(NULL, bitmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bitmap_mmap == MAP_FAILED)
    {
        log_error(logger, "Error en mmap al cargar bitmap");
        close(fd);
        free(bitmap_path);
        return;
    }

    block_bitmap = bitarray_create_with_mode(bitmap_mmap, bitmap_size, LSB_FIRST);

    close(fd);
    free(bitmap_path);

    log_info(logger, "Bitmap cargado con mmap desde disco");
}

void sync_bitmap(void)
{
    if (bitmap_mmap && bitmap_size > 0)
    {
        msync(bitmap_mmap, bitmap_size, MS_SYNC);
    }
}

void destroy_block_bitmap(void)
{
    if (block_bitmap)
    {
        // Sincronizar antes de destruir
        sync_bitmap();

        // Destruir el bitarray (no liberar el buffer, es mmap)
        bitarray_destroy(block_bitmap);
        block_bitmap = NULL;

        // Desmapear la memoria
        if (bitmap_mmap && bitmap_size > 0)
        {
            munmap(bitmap_mmap, bitmap_size);
            bitmap_mmap = NULL;
        }

        pthread_mutex_destroy(&MUTEX_BLOCK_BITMAP);
    }
}

void mark_physical_block_as_free(uint32_t block_number)
{
    pthread_mutex_lock(&MUTEX_BLOCK_BITMAP);
    bitarray_clean_bit(block_bitmap, block_number);
    // mmap con MAP_SHARED persiste automáticamente
    pthread_mutex_unlock(&MUTEX_BLOCK_BITMAP);
}

void mark_physical_block_as_occupied(uint32_t block_number)
{
    pthread_mutex_lock(&MUTEX_BLOCK_BITMAP);
    bitarray_set_bit(block_bitmap, block_number);
    // mmap con MAP_SHARED persiste automáticamente
    pthread_mutex_unlock(&MUTEX_BLOCK_BITMAP);
}

int get_free_block(void)
{
    pthread_mutex_lock(&MUTEX_BLOCK_BITMAP);

    int result = -1;
    
    for (int i = 0; i < fs_cfg->total_blocks; i++)
    {
        if (bitarray_test_bit(block_bitmap, i) == 0)
        {
            result = i;
            bitarray_set_bit(block_bitmap, i);
            break;
        }
    }
    pthread_mutex_unlock(&MUTEX_BLOCK_BITMAP);

    return result;
}