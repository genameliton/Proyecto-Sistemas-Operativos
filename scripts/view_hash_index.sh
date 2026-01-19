#!/bin/bash

# Ruta al archivo de índice de hashes (ajustar si es necesario)
HASH_INDEX="/home/utnso/storage/blocks_hash_index.config"

if [ ! -f "$HASH_INDEX" ]; then
    echo "Error: El archivo $HASH_INDEX no existe."
    exit 1
fi

echo "==========================================="
echo "  ÍNDICE DE HASHES (DEDUPLICACIÓN)"
echo "  Archivo: $HASH_INDEX"
echo "==========================================="

# Formato: HASH -> BLOQUE
echo "HASH                             -> BLOQUE FÍSICO"
echo "----------------------------------------------------"

# Leemos el archivo, ignoramos líneas vacías o comentarios, y formateamos
grep -v "^#" "$HASH_INDEX" | grep "=" | sed 's/=/ -> /' | while read line; do
    echo "$line"
done

echo "==========================================="
count=$(grep -c "=" "$HASH_INDEX")
echo "Total de bloques únicos indexados: $count"
