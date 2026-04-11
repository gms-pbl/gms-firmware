# simulator-hw

Virtual hardware simulator for the GMS Portenta flow.

This app simulates the current MQTT behavior of your firmware:
- publishes telemetry to `telemetry/zone1`
- listens for commands on `commands/zone1/output`
- applies output changes to virtual `dout_00..dout_07`

It also provides a Svelte UI to:
- edit sensor values
- toggle digital inputs/outputs
- save/load/delete/clear state profiles
- edit connection settings and connect/disconnect from the broker

## Local development

```bash
bun install
bun run dev
```

UI default URL: `http://localhost:5173`

## Docker development (hot reload)

From `src/gateway`:

```bash
docker compose up -d --build
```

The `simulator_hw` service runs `vite dev` inside Docker with bind mounts, so edits under `src/gateway/simulator-hw` hot-reload without restarting the container.

UI URL in Docker mode: `http://localhost:4173`

### Production-style local run

```bash
bun run build
bun run start
```

UI default URL: `http://localhost:4173`

## Configuration

You can provide startup defaults through env vars (`.env` supported by SvelteKit):

- `MQTT_HOST`
- `MQTT_PORT`
- `TELEMETRY_TOPIC`
- `COMMAND_TOPIC`
- `PUBLISH_INTERVAL_MS`
- `MQTT_CLIENT_ID`
- `MQTT_USERNAME` (optional)
- `MQTT_PASSWORD` (optional)
- `AUTO_CONNECT` (`true`/`false`)
- `SIM_DATA_DIR`

Important: settings changed in the UI are persisted and override env defaults on next restart.

## Saved state and profiles

Persisted files are stored under `sim-data/` by default:
- `current-state.json`
- `connection.json`
- `profiles/*.json`

This directory is ignored by git.

## MQTT command payload format

The simulator expects commands matching your firmware contract:

```json
{ "channel": 2, "state": 1 }
```

- `channel`: integer `0..7`
- `state`: `0` (LOW) or `1` (HIGH)
