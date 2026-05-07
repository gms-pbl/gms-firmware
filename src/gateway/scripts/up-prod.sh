#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

"${SCRIPT_DIR}/clean-containters.sh"
"${SCRIPT_DIR}/verify-ports.sh"

cd "${GATEWAY_ROOT}"
docker compose build --pull cloud_broker local_broker edge_engine
exec docker compose up -d --build --pull always --force-recreate --remove-orphans cloud_broker local_broker edge_engine "$@"
