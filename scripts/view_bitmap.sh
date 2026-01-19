#!/bin/bash

BITMAP_PATH="/home/utnso/storage/bitmap.bin"

if [ ! -f "$BITMAP_PATH" ]; then
    echo "Error: No se encontró $BITMAP_PATH"
    exit 1
fi

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

echo "========================================"
echo " ESTADO DEL BITMAP (Bloque -> Estado)"
echo " Rango: $START - $END"
echo "========================================"

# Optimización: Calcular offset en bytes
# Cada byte contiene 8 bloques (bits)
BYTE_OFFSET=$((START / 8))
INITIAL_BLOCK_ID=$((BYTE_OFFSET * 8))

# Leemos los bits con xxd -b saltando hasta el offset necesario
# Salida de xxd -b: 0000000: 11111011 ...
BITS=$(xxd -b -s "$BYTE_OFFSET" -c 256 "$BITMAP_PATH" | cut -d: -f2 | tr -d ' \n')

BLOCK_ID=$INITIAL_BLOCK_ID
total_len=${#BITS}

# Procesar string de bits
for (( i=0; i<total_len; i+=8 )); do
    BYTE_STR=${BITS:i:8}
    
    # Recorremos el byte de derecha a izquierda (LSB = índice 7 del string = primer bloque del byte)
    # Comportamiento heredado del script original:
    # indice 7 (ultimo char) -> corresponde al bloque mas bajo del grupo de 8
    
    for (( bit=7; bit>=0; bit-- )); do
        # Verificar si hemos llegado al final del rango
        if [ "$BLOCK_ID" -gt "$END" ]; then
            break 2
        fi

        # Solo imprimir si estamos dentro del rango (start check)
        # Esto es necesario porque al saltar por bytes podemos empezar un poco antes del START
        if [ "$BLOCK_ID" -ge "$START" ]; then
            current_bit=${BYTE_STR:bit:1}
            
            # Si se acaban los bits en el string (fin de archivo real?), current_bit estara vacio
            if [ -z "$current_bit" ]; then
                break 2
            fi

            if [ "$current_bit" == "1" ]; then
                 echo -e "Bloque $(printf "%04d" $BLOCK_ID): \e[31mOCUPADO\e[0m"
            else
                 echo -e "Bloque $(printf "%04d" $BLOCK_ID): \e[32mLIBRE\e[0m"
            fi
        fi
        
        ((BLOCK_ID++))
    done
done

echo "========================================"
