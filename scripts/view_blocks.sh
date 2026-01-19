#!/bin/bash

# Directorio de bloques físicos (ajustar si es necesario)
BLOCKS_DIR="/home/utnso/storage/physical_blocks"

# Parametros: [DESDE] [HASTA]
if [ "$#" -eq 1 ]; then
    START=0
    END=$1
elif [ "$#" -eq 2 ]; then
    START=$1
    END=$2
else
    # Default por defecto si no se pasan parámetros
    START=0
    END=20
fi

echo "==========================================="
echo "  MOSTRANDO BLOQUES DEL $START AL $END"
echo "  Directorio: $BLOCKS_DIR"
echo "==========================================="

count=0
for (( i=START; i<=END; i++ )); do
    # Formatear a 4 digitos: block0000.dat
    filename=$(printf "block%04d.dat" "$i")
    block="$BLOCKS_DIR/$filename"

    if [ -f "$block" ]; then
        echo "-------------------------------------------"
        echo "BLOQUE: $filename"
        
        # Leemos los primeros bytes para ver contenido (opcional)
        # content=$(head -c 16 "$block" | xxd -p)
        
        echo -n "HEXA:  "
        xxd -p -c 16 "$block"
        echo -n "TEXTO: "
        # Mostramos caracteres imprimibles, reemplazamos el resto por '.'
        cat "$block" | tr -c '[:print:]\n' '.'
        echo ""
        
        ((count++))
    fi
done

echo "-------------------------------------------"
echo "Total encontrados en rango: $count"
