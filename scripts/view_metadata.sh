#!/bin/bash

# Script para ver el contenido de todos los metadata.config de cada file:tag
# Uso: ./view_metadata.sh [PUNTO_MONTAJE]

PUNTO_MONTAJE="${1:-/home/utnso/storage}"

if [ ! -d "$PUNTO_MONTAJE/files" ]; then
    echo "Error: No se encontró el directorio $PUNTO_MONTAJE/files"
    exit 1
fi

echo "=========================================="
echo "  METADATA DE TODOS LOS ARCHIVOS"
echo "  Punto de Montaje: $PUNTO_MONTAJE"
echo "=========================================="
echo ""

# Encontrar y mostrar todos los metadata.config
find "$PUNTO_MONTAJE/files" -name "metadata.config" -type f | sort | while read -r metadata_file; do
    # Extraer el nombre del file:tag desde la ruta
    file_tag=$(echo "$metadata_file" | sed "s|$PUNTO_MONTAJE/files/||" | sed 's|/metadata.config||' | tr '/' ':')
    
    echo "┌─────────────────────────────────────────"
    echo "│ FILE:TAG = $file_tag"
    echo "├─────────────────────────────────────────"
    
    # Mostrar contenido del metadata
    while IFS= read -r line; do
        echo "│ $line"
    done < "$metadata_file"
    
    echo "└─────────────────────────────────────────"
    echo ""
done

# Mostrar resumen de bloques compartidos
echo "=========================================="
echo "  RESUMEN DE DEDUPLICACIÓN"
echo "=========================================="
echo ""

read -p "¿Ver deduplicación completa (todos los bloques)? [y/n] " -n 1 -r
echo ""
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Bloques compartidos (Total de referencias → bloque físico):"
    grep -h "^BLOCKS=" "$PUNTO_MONTAJE/files"/*/metadata.config "$PUNTO_MONTAJE/files"/*/*/metadata.config 2>/dev/null | \
        sed 's/BLOCKS=\[//' | sed 's/\].*//' | tr ',' '\n' | grep -v '^$' | sort | uniq -c | sort -rn | \
        while read count block; do
            if [ "$count" -gt 1 ]; then
                echo "  Bloque $block: referenciado $count veces ✓"
            fi
        done
else
    echo "Bloques compartidos (primer bloque lógico → bloque físico):"
    grep -h "^BLOCKS=" "$PUNTO_MONTAJE/files"/*/metadata.config "$PUNTO_MONTAJE/files"/*/*/metadata.config 2>/dev/null | \
        sed 's/BLOCKS=\[//' | sed 's/\].*//' | cut -d',' -f1 | sort | uniq -c | sort -rn | \
        while read count block; do
            if [ "$count" -gt 1 ]; then
                echo "  Bloque $block: compartido por $count archivos ✓"
            else
                echo "  Bloque $block: único (1 archivo)"
            fi
        done
fi

echo ""
echo "=========================================="
