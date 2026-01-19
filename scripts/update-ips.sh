#!/bin/bash

IP_MASTER=$1
IP_STORAGE=$2

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "Actualizando IPs en todas las configuraciones..."
echo "IP_MASTER: $IP_MASTER"
echo "IP_STORAGE: $IP_STORAGE"
echo "Directorio: $PROJECT_ROOT"

update_config() {
    local file=$1
    local ip_type=$2
    local new_ip=$3
    
    if [ -f "$file" ]; then
        sed -i "s/^${ip_type}=.*/${ip_type}=${new_ip}/" "$file"
        echo "Actualizado: $(basename $file)"
    fi
}

cd "$PROJECT_ROOT"

for config in query_control/configs/*.config query_control/*.config; do
    if [ -f "$config" ]; then
        update_config "$config" "IP_MASTER" "$IP_MASTER"
    fi
done

for config in worker/configs/*.config worker/*.config; do
    if [ -f "$config" ]; then
        update_config "$config" "IP_MASTER" "$IP_MASTER"
        update_config "$config" "IP_STORAGE" "$IP_STORAGE"
    fi
done

echo "IPs actualizadas correctamente"
