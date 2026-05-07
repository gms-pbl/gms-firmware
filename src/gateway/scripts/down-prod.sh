#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${GATEWAY_ROOT}"
docker compose down --remove-orphans "$@"
"${SCRIPT_DIR}/clean-containters.sh"
exec "${SCRIPT_DIR}/verify-ports.sh"
