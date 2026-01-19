#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PIDS_QUERIES=()

CONF_QUERY="$PROJECT_ROOT/query_control.config"
BIN_QUERY="$PROJECT_ROOT/bin/query_control"

for i in {1..25}; do
    $BIN_QUERY "$CONF_QUERY" "AGING_1" "20" &
    PIDS_QUERIES+=($!)
    $BIN_QUERY "$CONF_QUERY" "AGING_2" "20" &
    PIDS_QUERIES+=($!)
    $BIN_QUERY "$CONF_QUERY" "AGING_3" "20" &
    PIDS_QUERIES+=($!)
    $BIN_QUERY "$CONF_QUERY" "AGING_4" "20" &
    PIDS_QUERIES+=($!)
done

for PID in "${PIDS_QUERIES[@]}"; do
    wait "$PID"
done