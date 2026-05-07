# GMS Portenta Firmware

Firmware project for real zone controllers (Portenta H7).

Current focus is M7-based end-to-end integration.

## What It Does

- Reads sensors (air + soil + digital inputs).
- Publishes raw telemetry to gateway-local MQTT topics.
- Announces device identity and metadata.
- Receives low-level output commands (`channel`, `state`).
- Receives zone config (`zone_id`, `zone_name`) and caches it in memory.

Telemetry payload includes:

- sensor metrics (`air_*`, `soil_*`)
- digital inputs (`din_00..din_03`)
- digital outputs (`dout_00..dout_07`)

## MQTT Topic Contract (Device Side)

Device computes topics dynamically:

- `edge/{greenhouse_id}/zone/{device_id}/registry/announce`
- `edge/{greenhouse_id}/zone/{device_id}/telemetry/raw`
- `edge/{greenhouse_id}/zone/{device_id}/command/output`
- `edge/{greenhouse_id}/zone/{device_id}/config`

## Device Identity

- `DEVICE_ID` optional in `.env`.
- If `DEVICE_ID` is empty, firmware auto-derives stable ID from WiFi MAC (`portenta-...`).

Recommendation:

- keep `DEVICE_ID` empty unless you need a fixed manual identity for lab testing.
- never reuse one `DEVICE_ID` across two devices.

## Required `.env`

File: `firmware/src/portenta/.env`

```env
WIFI_SSID="your-wifi"
WIFI_PASS="your-pass"
MQTT_BROKER="192.168.1.32" # machine running the gateway stack
MQTT_PORT="18831"          # single-gateway local broker port
GREENHOUSE_ID="home"       # must match gateway GREENHOUSE_ID

# Optional:
# DEVICE_ID="portenta-dev-01"
# DEVICE_LABEL="Portenta Zone Bench"
```

## Build and Upload

From `firmware/src/portenta`:

```bash
# M7 firmware
./scripts/build_m7.sh
./scripts/upload_m7.sh

# Optional M4 image
./scripts/build_m4.sh
./scripts/upload_m4.sh

# Sensor utility
./scripts/build_sensor_util.sh
./scripts/upload_sensor_util.sh
```

## Expected Runtime Sequence

1. Device connects WiFi.
2. Device connects MQTT broker (`MQTT_BROKER:MQTT_PORT`).
3. Device publishes announce.
4. Gateway may push cached config (`zone_id`, `zone_name`).
5. Device applies config and publishes telemetry continuously.
6. Commands from backend route through gateway to `command/output`.

## Connecting Hardware to Single-Gateway Mode

When using the no-simulator hardware stack (`firmware/src/gateway/scripts/up-prod.sh`):

1. Set Portenta `.env` to the host machine LAN IP and the local broker port.
2. Keep `GREENHOUSE_ID` aligned with `firmware/src/gateway/.env`.

```env
MQTT_BROKER="192.168.1.32"
MQTT_PORT="18831"
GREENHOUSE_ID="home"
```

The backend uses the separate cloud/global broker on host `1883`; Portenta must continue to use only the local broker on `18831`.

## Connecting Hardware to Cluster Gateway

When using simulator cluster manager (`http://localhost:4173`):

1. Pick the gateway node (for example `test1`).
2. Read its local endpoint from the card: `Local MQTT: internal:<port>`.
3. Set Portenta `.env` to the host machine LAN IP + that port:

```env
MQTT_BROKER="192.168.1.32"
MQTT_PORT="18833"
GREENHOUSE_ID="test1"
```

4. Build and upload M7 firmware.
5. Login in frontend (`/g`), open the matching greenhouse, and verify discovery in zone registry.

Notes:

- `GREENHOUSE_ID` must match the selected gateway greenhouse id.
- If two tenants both use `test1`, isolation is still preserved by tenant-scoped backend auth and gateway tenant config.
- The node port is the decisive hardware routing selector in cluster mode.

## Local-Only Broker Safety

- Firmware now enforces local-only MQTT broker targets.
- Allowed broker hosts:
  - private IPv4 ranges (`10.x`, `172.16-31.x`, `192.168.x`)
  - link-local (`169.254.x`)
  - loopback (`127.x`)
  - hostnames ending with `.local`
  - `localhost`
- Public broker hostnames/IPs are rejected at runtime to prevent accidental direct cloud connections from devices.

## If Device Does Not Show in Frontend

- Verify Portenta serial log shows successful MQTT connect and telemetry publish.
- Verify `MQTT_BROKER` + `MQTT_PORT` point to the intended gateway local broker endpoint.
- Verify frontend user is logged into the tenant that owns that greenhouse.
- Verify backend and gateway edge are running and connected.

## Before Plugging Real Hardware

- Ensure gateway stack is running (`firmware/src/gateway/scripts/up.sh` or `firmware/src/gateway/scripts/up-cluster.sh`).
- For no-simulator hardware tests, use `firmware/src/gateway/scripts/up-prod.sh` and keep Portenta on `MQTT_PORT=18831`.
- Ensure backend is running (`backend/infra/scripts/up.sh`, or `backend/infra/scripts/up.sh -v` to follow logs).
- Ensure frontend greenhouse/zone page uses the same greenhouse id.

## Useful Files

- main config/macros: `src/m7/main.h`
- MQTT behavior: `src/m7/module/task_mqtt/task_mqtt.cpp`
- sensor task: `src/m7/module/task_sensor/task_sensor.cpp`
- output task: `src/m7/module/task_io/task_io.cpp`
