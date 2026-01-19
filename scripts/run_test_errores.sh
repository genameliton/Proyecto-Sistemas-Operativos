#!/bin/bash
source ./test_utils.sh

CONF_QUERY="$PROJECT_ROOT/query_control/query_control.config"
CONF_MASTER="$PROJECT_ROOT/master/configs/basic_m.config"
CONF_STORAGE="$PROJECT_ROOT/storage/configs/basic_s.config"
CONF_WORKER="$PROJECT_ROOT/worker/configs/errores_w.config"

preparar_entorno

# CLOCK
log_header "TEST ERRORES"
start_storage "$CONF_STORAGE"
start_master  "$CONF_MASTER"
start_worker  "$CONF_WORKER" "worker"
sleep 2

run_query "$CONF_QUERY" "ESCRITURA_ARCHIVO_COMMITED" "1" "1"
sleep 0.1
run_query "$CONF_QUERY" "FILE_EXISTENTE" "1" "2"
sleep 0.1
run_query "$CONF_QUERY" "LECTURA_FUERA_DEL_LIMITE" "1" "3"
sleep 0.1
run_query "$CONF_QUERY" "TAG_EXISTENTE" "1" "4"

wait_queries
cleanup
echo -e "${CYAN}*** PRUEBA DE ERRORES COMPLETADA ***${NC}"