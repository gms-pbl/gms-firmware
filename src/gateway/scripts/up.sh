#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${GATEWAY_ROOT}"
docker compose --profile simulator build --pull
exec docker compose --profile simulator up -d --build --pull always --force-recreate "$@"
