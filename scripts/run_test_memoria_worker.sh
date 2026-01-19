#!/bin/bash
source ./test_utils.sh

CONF_QUERY="$PROJECT_ROOT/query_control/query_control.config"
CONF_MASTER="$PROJECT_ROOT/master/configs/basic_m.config"
CONF_STORAGE="$PROJECT_ROOT/storage/configs/basic_s.config"
CONF_WORKER_1="$PROJECT_ROOT/worker/configs/mem_test_w1.config"
CONF_WORKER_2="$PROJECT_ROOT/worker/configs/mem_test_w2.config"

preparar_entorno

# CLOCK
log_header "MEMORIA CLOCK"
start_storage "$CONF_STORAGE"
start_master  "$CONF_MASTER"
start_worker  "$CONF_WORKER_1" "worker1"
sleep 2

run_query "$CONF_QUERY" "MEMORIA_WORKER" "0" "1"
wait_queries

cleanup
sleep 3

# LRU
log_header "MEMORIA LRU"
start_storage "$CONF_STORAGE"
start_master  "$CONF_MASTER"
start_worker  "$CONF_WORKER_2" "worker2"

run_query "$CONF_QUERY" "MEMORIA_WORKER_2" "0" "2"
wait_queries
cleanup
echo -e "${CYAN}*** PRUEBA DE MEMORIA COMPLETADA ***${NC}"