#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${GATEWAY_ROOT}"

docker build -t gms-edge-engine:cluster "./engine"

if ! docker network inspect gms-sim-cluster >/dev/null 2>&1; then
	docker network create gms-sim-cluster >/dev/null
fi

exec docker compose -f docker-compose.cluster.yml up -d --build --force-recreate "$@"
