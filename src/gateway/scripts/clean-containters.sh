#!/usr/bin/env bash
set -euo pipefail

containers=(
  gms_local_broker
  gms_cloud_broker
  gms_edge_engine
  gms_sim_home_edge
  gms_sim_home_broker
  gms_sim_thr-smoke-430_edge
  gms_sim_thr-smoke-430_broker
)

docker rm -f "${containers[@]}" 2>/dev/null || true
