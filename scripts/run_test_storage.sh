#!/bin/bash
source ./test_utils.sh

CONF_QUERY="$PROJECT_ROOT/query_control/query_control.config"
CONF_MASTER="$PROJECT_ROOT/master/configs/storage_m.config"
CONF_STORAGE="$PROJECT_ROOT/storage/configs/basic_s.config"
CONF_WORKER="$PROJECT_ROOT/worker/configs/storage_w.config"

preparar_entorno

start_storage "$CONF_STORAGE"
start_master  "$CONF_MASTER"
start_worker  "$CONF_WORKER" "worker"
sleep 2

log_header "Ejecutando STORAGE_1 a STORAGE_4"
run_query "$CONF_QUERY" "STORAGE_1" "0" "QC-1"
sleep 0.5
run_query "$CONF_QUERY" "STORAGE_2" "2" "QC-2"
sleep 0.5
run_query "$CONF_QUERY" "STORAGE_3" "4" "QC-3"
sleep 0.5
run_query "$CONF_QUERY" "STORAGE_4" "6" "QC-4"

wait_queries

log_header "VALIDACION 1: Estado del FS tras STORAGE_1-4"

./view_metadata.sh

echo -e "\n${YELLOW}>>> Verifica el uso de bloques arriba.${NC}"
echo -e "${YELLOW}>>> Presiona ENTER para lanzar STORAGE_5${NC}"
read -r

log_header "Ejecutando STORAGE_5"
run_query "$CONF_QUERY" "STORAGE_5" "0" "QC-5"

wait_queries

log_header "VALIDACION 2: Estado del FS tras STORAGE_5"

./view_metadata.sh

echo -e "\n${CYAN}*** PRUEBA STORAGE COMPLETADA ***${NC}"
echo -e "${YELLOW}Presiona ENTER para finalizar y limpiar procesos...${NC}"
read -r

cleanup