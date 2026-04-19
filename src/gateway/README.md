# GMS Gateway Stack

Local edge stack that runs on the greenhouse Mini PC.

Services in `docker-compose.yml`:

- `mosquitto` -> local MQTT LAN broker
- `edge_engine` -> local processing, registry cache, MQTT bridge
- `simulator_hw` -> optional virtual hardware UI (profile: `simulator`)

## Start / Stop

From `firmware/src/gateway`:

```bash
./scripts/up.sh
./scripts/down.sh
```

You can also target only selected services:

```bash
./scripts/up.sh edge_engine simulator_hw
```

## Ports

- `1883` -> Mosquitto (MQTT)
- `4173` -> simulator UI (when simulator profile is active)

## What Edge Engine Does

- Subscribes to local device topics:
  - `edge/{greenhouse}/zone/+/registry/announce`
  - `edge/{greenhouse}/zone/+/telemetry/raw`
- Applies local safety logic (example irrigation rule from soil moisture).
- Caches zone assignments in local SQLite.
- Publishes normalized uplink to backend MQTT topics.
- Consumes backend downlink registry/command messages.
- Forwards device config/commands back to local zone topics.

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

Configured in `docker-compose.yml` for `edge_engine`:

- `TENANT_ID`
- `GREENHOUSE_ID`
- `GATEWAY_ID` (defaults semantically to greenhouse)
- `LOCAL_BROKER_HOST`, `LOCAL_BROKER_PORT`
- `CLOUD_BROKER_HOST`, `CLOUD_BROKER_PORT`
- `CONFIG_REHYDRATE_COOLDOWN_SEC` (prevents config resend storms)

## Persistence

Edge engine stores local state in SQLite volume:

- outbound buffer (offline uplink queue)
- zone registry cache (device -> zone assignment)

Volume name: `gms_edge_sqlite_db`

## Typical Operator Flow (Implemented)

1. Device/simulator announces itself.
2. Backend sees discovered device in `/v1/zones/registry`.
3. User assigns zone from frontend.
4. Backend publishes downlink registry command.
5. Edge engine applies assignment, sends local `.../config`, emits registry ack/event.
6. Device telemetry includes assigned zone context via gateway normalization.

## Debug Tips

- Edge engine logs:

```bash
docker logs -f gms_edge_engine
```

- Simulator logs:

```bash
docker logs -f gms_simulator_hw
```

- Broker logs:

```bash
docker logs -f gms_local_broker
```

Expected normal cadence in logs:

- registry announce events roughly every 10-30s (depends on device/simulator settings)
- gateway status heartbeat every 30s by default
- telemetry snapshots/deltas at configured publish intervals
