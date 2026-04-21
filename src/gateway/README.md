# GMS Gateway Stack

Gateway runtime for local edge processing, broker bridging, and simulator management.

## Modes

### 1) Single gateway mode (`docker-compose.yml`)

Services:

- `mosquitto` -> local MQTT LAN broker
- `edge_engine` -> local processing, registry cache, MQTT bridge
- `simulator_hw` -> optional virtual hardware UI (profile: `simulator`)

Start/stop from `firmware/src/gateway`:

```bash
./scripts/up.sh
./scripts/down.sh
```

Optional selective start:

```bash
./scripts/up.sh edge_engine simulator_hw
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
- `1883` -> shared cloud broker (cluster mode) or local broker (single mode)
- per-gateway local broker host ports -> auto-assigned unique ports (default base `18831`) in cluster mode

The cluster manager UI shows these ports as `Local MQTT: internal:<port>`.

## What Edge Engine Does

- Subscribes to local device topics:
  - `edge/{greenhouse}/zone/+/registry/announce`
  - `edge/{greenhouse}/zone/+/telemetry/raw`
- Applies local logic (example irrigation rule from soil moisture).
- Caches zone assignments in local SQLite.
- Publishes normalized uplink to backend topics.
- Consumes backend downlink registry/command messages.
- Forwards config/commands to local device topics.

## Topic Mapping

### Local

- announce: `edge/{greenhouse}/zone/{device}/registry/announce`
- telemetry: `edge/{greenhouse}/zone/{device}/telemetry/raw`
- config: `edge/{greenhouse}/zone/{device}/config`
- output command: `edge/{greenhouse}/zone/{device}/command/output`

### Backend-facing

- uplink: `gms/{tenant}/{greenhouse}/uplink/{telemetry|registry|status|command_ack}`
- downlink: `gms/{tenant}/{greenhouse}/downlink/{registry|command}`

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

Cluster manager tuning:

- `GATEWAY_HOST_MQTT_PORT_BASE` (default `18831`)

## Persistence

Edge engine stores local state in SQLite volume:

- outbound buffer (offline uplink queue)
- zone registry cache (device -> zone assignment)

## Typical Operator Flow

1. Device/simulator announces on local edge topic.
2. Backend shows discovered device in `GET /v1/g/{greenhouse_id}/zones/registry` (tenant-scoped by session).
3. User assigns zone from frontend `/g/{greenhouse_id}`.
4. Backend publishes downlink registry command.
5. Edge applies assignment, publishes local `.../config`, emits registry ack/event.
6. Device telemetry appears with assigned context and command ACK feedback.

## Hardware Integration Rule

Portenta devices must connect to gateway local broker endpoints only (`MQTT_BROKER` + node-specific `MQTT_PORT`), never directly to public/cloud brokers.

## Debug Tips

Single gateway mode:

```bash
docker logs -f gms_edge_engine
docker logs -f gms_local_broker
docker logs -f gms_simulator_hw
```

Cluster mode (example gateway `test1`):

```bash
docker logs -f gms_sim_cluster_manager
docker logs -f gms_sim_test1_edge
docker logs -f gms_sim_test1_broker
```
