#!/bin/bash
source ./test_utils.sh

CONF_QUERY="$PROJECT_ROOT/query_control/query_control.config"
CONF_MASTER="$PROJECT_ROOT/master/configs/estabilidad_m.config"
CONF_STORAGE="$PROJECT_ROOT/storage/configs/estabilidad_s.config"
CONF_WORKER_1="$PROJECT_ROOT/worker/configs/estabilidad_w1.config"
CONF_WORKER_2="$PROJECT_ROOT/worker/configs/estabilidad_w2.config"
CONF_WORKER_3="$PROJECT_ROOT/worker/configs/estabilidad_w3.config"
CONF_WORKER_4="$PROJECT_ROOT/worker/configs/estabilidad_w4.config"
CONF_WORKER_5="$PROJECT_ROOT/worker/configs/estabilidad_w5.config"
CONF_WORKER_6="$PROJECT_ROOT/worker/configs/estabilidad_w6.config"

preparar_entorno
log_header "INICIANDO PRUEBA DE ESTABILIDAD"

start_storage "$CONF_STORAGE" "VALGRIND"
start_master  "$CONF_MASTER" "VALGRIND"

valgrind --leak-check=full --track-origins=yes --log-file="worker1.valgrind.log" $BIN_WORKER "$CONF_WORKER_1" "worker1" > /dev/null &
PID_W1=$!
sleep 0.5
valgrind --leak-check=full --track-origins=yes --log-file="worker2.valgrind.log" $BIN_WORKER "$CONF_WORKER_2" "worker2" > /dev/null &
PID_W2=$!
sleep 0.5

SCRIPT_NOMBRE="AGING_" 
PRIORIDAD=20

for i in {1..4}; do
    for j in {1..25}; do
        run_query "$CONF_QUERY" "$SCRIPT_NOMBRE$i" "$PRIORIDAD" "QC-$i-$j"
        sleep 0.1
    done
done

sleep 45
start_worker "$CONF_WORKER_3" "worker3"
sleep 0.5
start_worker "$CONF_WORKER_4" "worker4"

sleep 45
echo -e "${RED}   -> Matando proceso $PID_W1 (Worker 1)...${NC}"
kill -9 $PID_W1
echo -e "${RED}   -> Matando proceso $PID_W2 (Worker 2)...${NC}"
kill -9 $PID_W2

sleep 45
start_worker "$CONF_WORKER_5" "worker5"
sleep 0.5
start_worker "$CONF_WORKER_6" "worker6"

wait_queries

echo -e "\n${GREEN}*** PRUEBA DE ESTABILIDAD COMPLETADA ***${NC}"

cleanup