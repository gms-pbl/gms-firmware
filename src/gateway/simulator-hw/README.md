# simulator-hw

Gateway cluster manager and virtual Portenta simulator for GMS.

## What It Does

This app now has two layers:

- cluster manager (`/`) for multiple simulated gateways
- per-gateway virtual device simulator (`/g/{gateway_id}`)

For each virtual device:

- publish announce to `edge/{greenhouse_id}/zone/{device_id}/registry/announce`
- publish raw telemetry to `edge/{greenhouse_id}/zone/{device_id}/telemetry/raw`
- receive commands on `edge/{greenhouse_id}/zone/{device_id}/command/output`
- receive config on `edge/{greenhouse_id}/zone/{device_id}/config`

Telemetry includes numeric sensor keys plus digital IO (`din_*`, `dout_*`).

## UI Routes

- `/` - gateway cluster manager
- `/g/{gateway_id}` - simulator controls for one gateway
- `/{gateway_id}` - compatibility redirect to `/g/{gateway_id}`

## Cluster Manager Features (`/`)

- add/update/remove gateways (tenant, greenhouse, gateway id)
- start/stop gateway containers from UI
- inspect runtime health (broker + edge)
- auto-assign unique local MQTT host ports per gateway
- display local MQTT endpoint inline as `internal:<port>`

## Per-Gateway Simulator Features (`/g/{gateway_id}`)

- add/remove/select virtual instances
- connect/disconnect simulator instances
- edit active instance sensors/digital IO
- explicit `Save Sensors` and `Reset State` controls
- broker/connection profile save/load/delete/clear

## Run Locally (Bun)

From `firmware/src/gateway/simulator-hw`:

```bash
bun install
bun run dev
```

Default URL (local Bun): `http://localhost:5173`

## Run in Gateway Cluster Stack

From `firmware/src/gateway`:

```bash
./scripts/up-cluster.sh
```

Cluster manager URL: `http://localhost:4173`

Stop:

```bash
./scripts/down-cluster.sh
```

## Optional `.env` Configuration (Local Bun)

Create `firmware/src/gateway/simulator-hw/.env` if needed:

```env
MQTT_HOST=127.0.0.1
MQTT_PORT=1883
GREENHOUSE_ID=greenhouse-demo
PUBLISH_INTERVAL_MS=10000
MQTT_USERNAME=
MQTT_PASSWORD=
AUTO_CONNECT=true
SIM_DATA_DIR=./.sim-data
```

Cluster manager-specific envs are provided by `docker-compose.cluster.yml` (for example `SIM_DOCKER_SOCKET`, `SIM_CLUSTER_NETWORK`, `SIM_EDGE_IMAGE`).

## Persisted Data

Simulator state is persisted and survives restarts.

Local Bun default: `./.sim-data/`

Cluster container default: `/app/sim-data/`

Key files:

- `gateways.json`
- `gateways/{gateway_id}/connection.json`
- `gateways/{gateway_id}/instances.json`
- `gateways/{gateway_id}/profiles/*.json`

## Low-Level Command Payload

Edge engine sends relay-style commands:

```json
{ "channel": 2, "state": 1 }
```

- `channel`: `0..7`
- `state`: `0` (LOW) or `1` (HIGH)

## Useful Checks

```bash
bun run check
bun run build
```
