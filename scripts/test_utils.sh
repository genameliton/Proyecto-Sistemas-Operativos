#!/bin/bash
# test_utils.sh

# ==============================================================================
# CONFIGURACIÓN GLOBAL
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colores
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
NC='\033[0m'

# Binarios
BIN_MASTER="$PROJECT_ROOT/master/bin/master"
BIN_WORKER="$PROJECT_ROOT/worker/bin/worker"
BIN_STORAGE="$PROJECT_ROOT/storage/bin/storage"
BIN_QUERY="$PROJECT_ROOT/query_control/bin/query_control"

PIDS_QUERIES=()

# ==============================================================================
# GESTIÓN DE PROCESOS
# ==============================================================================

cleanup() {
    echo -e "\n${RED}[!] Deteniendo todos los procesos...${NC}"
    pkill -f "$BIN_WORKER"
    pkill -f "$BIN_MASTER"
    pkill -f "$BIN_STORAGE"
    pkill -f "$BIN_QUERY"
    # También matamos procesos de valgrind si quedan colgados
    pkill -f "valgrind"
    PIDS_QUERIES=()
}

trap "cleanup; echo -e '\n[!] Interrumpido por usuario'; exit 1" SIGINT

preparar_entorno() {
    echo -e "${YELLOW}>>> Compilando y limpiando logs...${NC}"
    make -C "$PROJECT_ROOT" > /dev/null
    rm *.log *.valgrind.log *.helgrind.log 2> /dev/null
    cleanup
}

# ==============================================================================
# LÓGICA DE EJECUCIÓN (CORE)
# ==============================================================================

# Función interna para manejar Valgrind/Helgrind y redirecciones
_ejecutar_binario() {
    local BIN=$1
    local ARGS=$2
    local NOMBRE_PROC=$3
    local MODO_DEBUG=$4  # Puede ser: "VALGRIND", "HELGRIND", "VERBOSE" o vacío

    local COMANDO="$BIN $ARGS"

    case "$MODO_DEBUG" in
        "VALGRIND")
            echo -e "${RED}    [DEBUG] Ejecutando $NOMBRE_PROC con Valgrind...${NC}"
            valgrind --leak-check=full --track-origins=yes --log-file="${NOMBRE_PROC}.valgrind.log" $COMANDO &
            ;;
        "HELGRIND")
            echo -e "${RED}    [DEBUG] Ejecutando $NOMBRE_PROC con Helgrind...${NC}"
            valgrind --tool=helgrind --log-file="${NOMBRE_PROC}.helgrind.log" $COMANDO &
            ;;
        "VERBOSE")
            echo -e "${RED}    [DEBUG] Ejecutando $NOMBRE_PROC con salida en consola...${NC}"
            $COMANDO &
            ;;
        *)
            # Ejecución normal silenciosa
            $COMANDO > /dev/null &
            ;;
    esac
}

# ==============================================================================
# LANZAMIENTO DE MÓDULOS (Wrappers)
# ==============================================================================
# Ahora aceptan un argumento opcional al final para el modo debug

start_storage() {
    local CONF=$1
    local DEBUG_MODE=$2 
    echo "  -> Storage ($CONF)..."
    _ejecutar_binario "$BIN_STORAGE" "$CONF" "storage" "$DEBUG_MODE"
    sleep 2
}

start_master() {
    local CONF=$1
    local DEBUG_MODE=$2
    echo "  -> Master ($CONF)..."
    _ejecutar_binario "$BIN_MASTER" "$CONF" "master" "$DEBUG_MODE"
    sleep 2
}

start_worker() {
    local CONF=$1
    local NAME=$2
    local DEBUG_MODE=$3
    echo "  -> Worker: $NAME ($CONF)..."
    _ejecutar_binario "$BIN_WORKER" "$CONF $NAME" "$NAME" "$DEBUG_MODE"
    sleep 0.5
}

# ... (El resto de run_query y wait_queries se mantiene igual) ...

run_query() {
    local CONF=$1
    local OP=$2
    local ARG1=$3
    local ARG2=$4
    
    echo "  -> Query: $OP ($ARG1, $ARG2)"
    $BIN_QUERY "$CONF" "$OP" "$ARG1" "$ARG2" &
    PIDS_QUERIES+=($!) 
}

wait_queries() {
    echo "  ... Esperando finalización de queries ..."
    for pid in "${PIDS_QUERIES[@]}"; do
        wait $pid
    done
    PIDS_QUERIES=()
    echo -e "${GREEN}[OK] Queries finalizadas.${NC}\n"
}

log_header() {
    echo -e "\n${YELLOW}>>> INICIANDO FASE: $1 <<<${NC}"
}