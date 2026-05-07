# GMS Gateway Stack

Gateway runtime for local edge processing, broker bridging, and simulator management.

## Modes

### 1) Single gateway mode (`docker-compose.yml`)

Services:

- `cloud_broker` -> cloud/global MQTT entrypoint for backend <-> gateway traffic on host `1883`
- `local_broker` -> local greenhouse MQTT broker for Portenta/simulator traffic on host `18831`
- `edge_engine` -> local processing, registry cache, MQTT bridge
- `simulator_hw` -> optional virtual hardware UI (profile: `simulator`)

Start/stop from `firmware/src/gateway`:

```bash
./scripts/up.sh
./scripts/down.sh

# hardware only (no simulator)
./scripts/up-prod.sh
./scripts/down-prod.sh
```

### 2) Cluster simulation mode (`docker-compose.cluster.yml`)

Services:

- `gms_sim_cluster_manager` (`simulator_hw`) -> manages many gateway nodes
- `gms_sim_cloud_broker` -> shared cloud-side MQTT entrypoint for backend integration

Start/stop:

```bash
./scripts/up-cluster.sh
./scripts/down-cluster.sh
```

Cluster manager creates per-gateway containers on demand:

- broker: `gms_sim_<gateway_id>_broker`
- edge: `gms_sim_<gateway_id>_edge`

## Ports

- `4173` -> simulator / cluster manager UI
- `1883` -> cloud/global broker in single-gateway mode and shared cloud broker in cluster mode
- `18831` -> local greenhouse broker in single-gateway mode
- per-gateway local broker host ports -> auto-assigned unique ports (default base `18831`) in cluster mode

The cluster manager UI shows these ports as `Local MQTT: internal:<port>`.

## What Edge Engine Does

- Subscribes to local device topics:
  - `edge/{greenhouse}/zone/+/registry/announce`
  - `edge/{greenhouse}/zone/+/telemetry/raw`
- Applies local logic (example irrigation rule from soil moisture).
- Caches zone assignments in local SQLite.
- Caches zone threshold configs in local SQLite and evaluates telemetry offline.
- Publishes normalized uplink to backend topics.
- Consumes backend downlink registry/command/threshold messages.
- Forwards config/commands to local device topics.

## Topic Mapping

### Local

- announce: `edge/{greenhouse}/zone/{device}/registry/announce`
- telemetry: `edge/{greenhouse}/zone/{device}/telemetry/raw`
- config: `edge/{greenhouse}/zone/{device}/config`
- output command: `edge/{greenhouse}/zone/{device}/command/output`

### Backend-facing

- uplink: `gms/{tenant}/{greenhouse}/uplink/{telemetry|registry|status|command_ack|alert}`
- downlink: `gms/{tenant}/{greenhouse}/downlink/{registry|command|threshold}`

Telemetry notes:

- gateway forwards raw numeric keys from device payloads
- expected keys include sensor values plus binary IO (`din_*`, `dout_*`)

## Key Environment Variables

Edge engine variables (in compose or per-gateway runtime config):

- `TENANT_ID`
- `GREENHOUSE_ID`
- `GATEWAY_ID`
- `LOCAL_BROKER_HOST`, `LOCAL_BROKER_PORT`
- `CLOUD_BROKER_HOST`, `CLOUD_BROKER_PORT`
- `CLOUD_USERNAME`, `CLOUD_PASSWORD`
- `CONFIG_REHYDRATE_COOLDOWN_SEC`

Single-gateway `.env` lives at `firmware/src/gateway/.env` and currently drives both broker layers.

Cluster manager tuning:

- `GATEWAY_HOST_MQTT_PORT_BASE` (default `18831`)

## Persistence

Edge engine stores local state in SQLite volume:

- outbound buffer (offline uplink queue)
- zone registry cache (device -> zone assignment)
- threshold config cache and alert transition state

## Typical Operator Flow

1. Device/simulator announces on local edge topic.
2. Backend shows discovered device in `GET /v1/g/{greenhouse_id}/zones/registry` (tenant-scoped by session).
3. User assigns zone from frontend `/g/{greenhouse_id}`.
4. Backend publishes downlink registry command.
5. Edge applies assignment, publishes local `.../config`, emits registry ack/event.
6. Device telemetry appears with assigned context and command ACK feedback.

## Hardware Integration Rule

Portenta devices must connect to gateway local broker endpoints only (`MQTT_BROKER` + node-specific `MQTT_PORT`), never directly to public/cloud brokers.

For the current single-gateway hardware topology that means:

- backend/cloud broker: host `1883`
- Portenta/local broker: host `18831`

Use the cleanup helpers before switching from cluster mode to single-gateway hardware mode so stale `gms_sim_*` brokers do not keep old ports alive.

## Helper Scripts

```bash
./scripts/clean-containters.sh
./scripts/verify-ports.sh
```

- `clean-containters.sh` removes known stale single-gateway and dynamic cluster containers.
- `verify-ports.sh` reports open listeners on `1883`, `18831`, `18832`, and `4173`.

## Debug Tips

Single gateway mode:

```bash
./scripts/verify-ports.sh
docker logs -f gms_edge_engine
docker logs -f gms_cloud_broker
docker logs -f gms_local_broker
docker logs -f gms_simulator_hw
```

Cluster mode (example gateway `test1`):

```bash
docker logs -f gms_sim_cluster_manager
docker logs -f gms_sim_test1_edge
docker logs -f gms_sim_test1_broker
```
