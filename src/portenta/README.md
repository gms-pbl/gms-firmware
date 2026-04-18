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
MQTT_BROKER="192.168.1.32"      # gateway mini PC IP
GREENHOUSE_ID="greenhouse-demo" # must match gateway GREENHOUSE_ID

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
2. Device connects MQTT broker (`MQTT_BROKER:1883`).
3. Device publishes announce.
4. Gateway may push cached config (`zone_id`, `zone_name`).
5. Device applies config and publishes telemetry continuously.
6. Commands from backend route through gateway to `command/output`.

## Before Plugging Real Hardware

- Ensure gateway stack is running (`firmware/src/gateway/scripts/up.sh`).
- Ensure backend is running (`backend/infra/scripts/up.sh`, or `backend/infra/scripts/up.sh -v` to follow logs).
- Ensure frontend zones page uses same greenhouse id.

## Useful Files

- main config/macros: `src/m7/main.h`
- MQTT behavior: `src/m7/module/task_mqtt/task_mqtt.cpp`
- sensor task: `src/m7/module/task_sensor/task_sensor.cpp`
- output task: `src/m7/module/task_io/task_io.cpp`
