#!/usr/bin/env bash
set -euo pipefail

OUTPUT="$(ss -ltnp | grep -E ':1883|:18831|:18832|:4173' || true)"

if [[ -z "${OUTPUT}" ]]; then
  printf 'No matching ports are open.\n'
  exit 0
fi

printf 'Open or stale listeners found on monitored ports:\n'
printf '%s\n' "${OUTPUT}"
