# simulator-hw

Virtual hardware simulator for the GMS multi-zone flow.

It emulates multiple Portenta zone devices with per-instance state and unique GUID IDs.

## What It Simulates

For each virtual device:

- publishes announce to `edge/{greenhouse_id}/zone/{device_id}/registry/announce`
- publishes raw telemetry to `edge/{greenhouse_id}/zone/{device_id}/telemetry/raw`
- receives commands on `edge/{greenhouse_id}/zone/{device_id}/command/output`
- receives config on `edge/{greenhouse_id}/zone/{device_id}/config`

Published telemetry includes:

- sensor metrics (`air_*`, `soil_*`)
- digital inputs (`din_00..din_03`)
- digital outputs (`dout_00..dout_07`)

## UI Features

- add/remove/select virtual instances
- edit sensors and digital IO of the active instance
- connect/disconnect all instances
- broker + greenhouse configuration
- save/load/delete/clear state profiles

## Run Locally (Bun)

```bash
bun install
bun run dev
```

URL: `http://localhost:5173`

## Run with Gateway Docker Stack

From `firmware/src/gateway`:

```bash
./scripts/up.sh simulator_hw
```

URL: `http://localhost:4173`

## Optional `.env` Configuration

Create `src/gateway/simulator-hw/.env` if needed:

```env
MQTT_HOST=127.0.0.1
MQTT_PORT=1883
GREENHOUSE_ID=greenhouse-demo
PUBLISH_INTERVAL_MS=10000
MQTT_USERNAME=
MQTT_PASSWORD=
AUTO_CONNECT=true
SIM_DATA_DIR=./sim-data
```

Runtime behavior note:

- values edited in UI are persisted and override startup defaults after restart.

## Persisted Data

Default folder: `sim-data/`

- `connection.json`
- `instances.json`
- `profiles/*.json`

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
