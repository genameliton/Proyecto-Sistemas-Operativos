#!/bin/bash
source ./test_utils.sh

CONF_QUERY="$PROJECT_ROOT/query_control/query_control.config"
CONF_W1="$PROJECT_ROOT/worker/configs/plani_w1.config"
CONF_W2="$PROJECT_ROOT/worker/configs/plani_w2.config"
CONF_M_FIFO="$PROJECT_ROOT/master/configs/fifo_m.config"
CONF_S_FIFO="$PROJECT_ROOT/storage/configs/fifo_s.config"
CONF_M_AG="$PROJECT_ROOT/master/configs/aging_m.config"
CONF_S_AG="$PROJECT_ROOT/storage/configs/aging_s.config"

preparar_entorno

# FIFO
log_header "PLANIFICACION FIFO"
start_storage "$CONF_S_FIFO"
start_master  "$CONF_M_FIFO"
start_worker  "$CONF_W1" "worker1"
start_worker  "$CONF_W2" "worker2"
sleep 2

run_query "$CONF_QUERY" "FIFO_1" "4" "1"
sleep 0.1
run_query "$CONF_QUERY" "FIFO_2" "3" "2"
sleep 0.1
run_query "$CONF_QUERY" "FIFO_3" "5" "3"
sleep 0.1
run_query "$CONF_QUERY" "FIFO_4" "1" "4"

wait_queries
cleanup
sleep 3

# AGING
log_header "PLANIFICACION AGING"
start_storage "$CONF_S_AG"
start_master  "$CONF_M_AG"
start_worker  "$CONF_W1" "worker1"
start_worker  "$CONF_W2" "worker2"
sleep 2

run_query "$CONF_QUERY" "AGING_1" "4" "1"
sleep 0.1
run_query "$CONF_QUERY" "AGING_2" "3" "2"
sleep 0.1
run_query "$CONF_QUERY" "AGING_3" "5" "3"
sleep 0.1
run_query "$CONF_QUERY" "AGING_4" "1" "4"

wait_queries
cleanup
echo -e "${CYAN}*** PRUEBA PLANIFICACION COMPLETADA ***${NC}"