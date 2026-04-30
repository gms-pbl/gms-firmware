import json
import os
import sqlite3
import threading
import time
import uuid

import paho.mqtt.client as mqtt


LOCAL_HOST = os.environ.get("LOCAL_BROKER_HOST", "mosquitto")
LOCAL_PORT = int(os.environ.get("LOCAL_BROKER_PORT", 1883))

CLOUD_HOST = os.environ.get("CLOUD_BROKER_HOST", "mqtt.cloud.example.com")
CLOUD_PORT = int(os.environ.get("CLOUD_BROKER_PORT", 1883))
CLOUD_USER = os.environ.get("CLOUD_USERNAME", "edge_gateway_1")
CLOUD_PASS = os.environ.get("CLOUD_PASSWORD", "secret")

TENANT_ID = os.environ.get("TENANT_ID", "tenant-demo")
GREENHOUSE_ID = os.environ.get("GREENHOUSE_ID", "greenhouse-demo")
GATEWAY_ID = os.environ.get("GATEWAY_ID", GREENHOUSE_ID)

SNAPSHOT_INTERVAL_SEC = int(os.environ.get("SNAPSHOT_INTERVAL_SEC", 60))
BUFFER_FLUSH_BATCH = int(os.environ.get("BUFFER_FLUSH_BATCH", 100))
BUFFER_FLUSH_INTERVAL_SEC = int(os.environ.get("BUFFER_FLUSH_INTERVAL_SEC", 5))
STATUS_HEARTBEAT_SEC = int(os.environ.get("STATUS_HEARTBEAT_SEC", 30))
DEFAULT_DELTA_THRESHOLD = float(os.environ.get("DELTA_THRESHOLD", 0.1))
CONFIG_REHYDRATE_COOLDOWN_SEC = int(os.environ.get("CONFIG_REHYDRATE_COOLDOWN_SEC", 45))

DB_PATH = os.environ.get("EDGE_DB_PATH", "/app/data/offline_buffer.db")


cloud_connected = False
db_conn = None

zone_registry = {}
threshold_configs = {}
threshold_alert_states = {}
last_metrics = {}
last_snapshot_at = {}
last_irrigation_state = {}
last_rehydrate_at = {}

lock = threading.Lock()

UNSET = object()

SENSOR_KEYS = {
    "air_temp",
    "air_hum",
    "soil_moist",
    "soil_temp",
    "soil_cond",
    "soil_ph",
    "soil_n",
    "soil_p",
    "soil_k",
    "soil_salinity",
    "soil_tds",
}


def now_iso():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def uuid_str():
    return str(uuid.uuid4())


def uplink_topic(stream):
    return f"gms/{TENANT_ID}/{GREENHOUSE_ID}/uplink/{stream}"


def downlink_topic(stream):
    return f"gms/{TENANT_ID}/{GREENHOUSE_ID}/downlink/{stream}"


def local_zone_topic(device_id, stream):
    return f"edge/{GREENHOUSE_ID}/zone/{device_id}/{stream}"


def parse_local_topic(topic):
    parts = topic.split("/")
    if len(parts) < 6:
        return None
    if parts[0] != "edge" or parts[2] != "zone":
        return None

    greenhouse_id = parts[1]
    device_id = parts[3]
    stream = "/".join(parts[4:])
    return greenhouse_id, device_id, stream


def init_db():
    os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS outbound_buffer (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            topic TEXT NOT NULL,
            payload TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS zone_registry (
            device_id TEXT PRIMARY KEY,
            zone_id TEXT,
            zone_name TEXT,
            metadata_json TEXT,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS threshold_config (
            tenant_id TEXT NOT NULL,
            greenhouse_id TEXT NOT NULL,
            zone_id TEXT NOT NULL,
            config_version INTEGER NOT NULL,
            thresholds_json TEXT NOT NULL,
            applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (tenant_id, greenhouse_id, zone_id, config_version)
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS threshold_alert_state (
            tenant_id TEXT NOT NULL,
            greenhouse_id TEXT NOT NULL,
            zone_id TEXT NOT NULL,
            device_id TEXT NOT NULL,
            sensor_key TEXT NOT NULL,
            severity TEXT NOT NULL,
            threshold_version INTEGER,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (tenant_id, greenhouse_id, zone_id, device_id, sensor_key)
        )
        """
    )
    conn.commit()
    print(f"[DB] Initialized at {DB_PATH}", flush=True)
    return conn


def load_zone_registry_cache():
    with lock:
        zone_registry.clear()
        rows = db_conn.execute(
            "SELECT device_id, zone_id, zone_name, metadata_json FROM zone_registry"
        ).fetchall()

    for device_id, zone_id, zone_name, metadata_json in rows:
        metadata = {}
        if metadata_json:
            try:
                metadata = json.loads(metadata_json)
            except Exception:
                metadata = {}

        zone_registry[device_id] = {
            "zone_id": zone_id,
            "zone_name": zone_name,
            "metadata": metadata,
        }

    print(f"[REGISTRY] Loaded {len(zone_registry)} cached zone assignments.", flush=True)


def load_threshold_config_cache():
    with lock:
        rows = db_conn.execute(
            """
            SELECT zone_id, config_version, thresholds_json
            FROM threshold_config tc
            WHERE tenant_id = ?
              AND greenhouse_id = ?
              AND config_version = (
                  SELECT MAX(config_version)
                  FROM threshold_config
                  WHERE tenant_id = tc.tenant_id
                    AND greenhouse_id = tc.greenhouse_id
                    AND zone_id = tc.zone_id
              )
            """,
            (TENANT_ID, GREENHOUSE_ID),
        ).fetchall()

    threshold_configs.clear()
    for zone_id, config_version, thresholds_json in rows:
        try:
            thresholds = json.loads(thresholds_json)
        except Exception:
            thresholds = {}
        threshold_configs[zone_id] = {
            "config_version": int(config_version),
            "thresholds": thresholds,
        }

    print(f"[THRESHOLD] Loaded {len(threshold_configs)} cached zone configs.", flush=True)


def load_threshold_alert_state_cache():
    with lock:
        rows = db_conn.execute(
            """
            SELECT zone_id, device_id, sensor_key, severity, threshold_version
            FROM threshold_alert_state
            WHERE tenant_id = ? AND greenhouse_id = ?
            """,
            (TENANT_ID, GREENHOUSE_ID),
        ).fetchall()

    threshold_alert_states.clear()
    for zone_id, device_id, sensor_key, severity, threshold_version in rows:
        threshold_alert_states[(zone_id, device_id, sensor_key)] = {
            "severity": severity or "OK",
            "threshold_version": threshold_version,
        }

    print(f"[THRESHOLD] Loaded {len(threshold_alert_states)} cached alert states.", flush=True)


def save_zone_registry(device_id, zone_id, zone_name, metadata):
    metadata_json = json.dumps(metadata or {})
    with lock:
        db_conn.execute(
            """
            INSERT INTO zone_registry(device_id, zone_id, zone_name, metadata_json, updated_at)
            VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)
            ON CONFLICT(device_id) DO UPDATE SET
                zone_id = excluded.zone_id,
                zone_name = excluded.zone_name,
                metadata_json = excluded.metadata_json,
                updated_at = CURRENT_TIMESTAMP
            """,
            (device_id, zone_id, zone_name, metadata_json),
        )
        db_conn.commit()

    zone_registry[device_id] = {
        "zone_id": zone_id,
        "zone_name": zone_name,
        "metadata": metadata or {},
    }


def remove_zone_registry(device_id):
    with lock:
        db_conn.execute("DELETE FROM zone_registry WHERE device_id = ?", (device_id,))
        db_conn.commit()

    zone_registry.pop(device_id, None)
    last_rehydrate_at.pop(device_id, None)


def resolve_zone(device_id):
    config = zone_registry.get(device_id)
    if not config:
        return device_id, None

    zone_id = config.get("zone_id") or device_id
    zone_name = config.get("zone_name")
    return zone_id, zone_name


def normalize_zone_field(value):
    if not isinstance(value, str):
        return None

    normalized = value.strip()
    if not normalized:
        return None

    return normalized


def normalize_metrics(payload_dict):
    metrics = {}
    for key, value in payload_dict.items():
        if isinstance(value, bool):
            metrics[key] = 1.0 if value else 0.0
            continue

        if isinstance(value, (int, float)):
            metrics[key] = float(value)
            continue

        try:
            casted = float(value)
            metrics[key] = casted
        except Exception:
            continue
    return metrics


def metric_threshold(metric_name):
    if metric_name.startswith("din_") or metric_name.startswith("dout_"):
        return 1.0
    return DEFAULT_DELTA_THRESHOLD


def compute_delta(device_id, metrics):
    previous = last_metrics.get(device_id, {})
    changed = {}

    for key, value in metrics.items():
        if key not in previous:
            changed[key] = value
            continue

        if abs(value - previous[key]) >= metric_threshold(key):
            changed[key] = value

    last_metrics[device_id] = metrics
    return changed


def publish_or_buffer(topic, payload_dict):
    payload_text = json.dumps(payload_dict)

    if cloud_connected:
        cloud_client.publish(topic, payload_text)
        return

    with lock:
        db_conn.execute(
            "INSERT INTO outbound_buffer(topic, payload) VALUES (?, ?)",
            (topic, payload_text),
        )
        db_conn.commit()


def flush_offline_buffer():
    if not cloud_connected:
        return

    with lock:
        rows = db_conn.execute(
            "SELECT id, topic, payload FROM outbound_buffer ORDER BY id ASC LIMIT ?",
            (BUFFER_FLUSH_BATCH,),
        ).fetchall()

    if not rows:
        return

    for row_id, topic, payload in rows:
        cloud_client.publish(topic, payload)
        with lock:
            db_conn.execute("DELETE FROM outbound_buffer WHERE id = ?", (row_id,))

    with lock:
        db_conn.commit()

    print(f"[CLOUD] Flushed {len(rows)} buffered uplink messages.", flush=True)


def publish_registry_event(
    event_type,
    device_id,
    firmware_version=None,
    metadata=None,
    command_id=None,
    zone_id=UNSET,
    zone_name=UNSET,
):
    resolved_zone_id, resolved_zone_name = resolve_zone(device_id)
    effective_zone_id = resolved_zone_id if zone_id is UNSET else zone_id
    effective_zone_name = resolved_zone_name if zone_name is UNSET else zone_name

    payload = {
        "event_id": uuid_str(),
        "type": event_type,
        "tenant_id": TENANT_ID,
        "greenhouse_id": GREENHOUSE_ID,
        "gateway_id": GATEWAY_ID,
        "device_id": device_id,
        "zone_id": effective_zone_id,
        "zone_name": effective_zone_name,
        "firmware_version": firmware_version,
        "timestamp": now_iso(),
        "metadata": metadata or {},
    }
    if command_id:
        payload["command_id"] = command_id
    publish_or_buffer(uplink_topic("registry"), payload)


def publish_gateway_status(state):
    payload = {
        "event_id": uuid_str(),
        "type": "GATEWAY_STATUS",
        "tenant_id": TENANT_ID,
        "greenhouse_id": GREENHOUSE_ID,
        "gateway_id": GATEWAY_ID,
        "status": state,
        "timestamp": now_iso(),
    }
    publish_or_buffer(uplink_topic("status"), payload)


def publish_command_ack(status, command_id, device_id=None, zone_id=None, reason=None, config_version=None, ack_type="COMMAND_ACK"):
    payload = {
        "event_id": uuid_str(),
        "type": ack_type,
        "tenant_id": TENANT_ID,
        "greenhouse_id": GREENHOUSE_ID,
        "gateway_id": GATEWAY_ID,
        "command_id": command_id,
        "device_id": device_id,
        "zone_id": zone_id,
        "config_version": config_version,
        "status": status,
        "reason": reason,
        "timestamp": now_iso(),
    }
    publish_or_buffer(uplink_topic("command_ack"), payload)


def sanitize_threshold_payload(thresholds):
    if not isinstance(thresholds, dict) or not thresholds:
        raise ValueError("thresholds must be a non-empty object")

    sanitized = {}
    for sensor_key, levels in thresholds.items():
        if sensor_key not in SENSOR_KEYS:
            raise ValueError(f"unsupported sensor key: {sensor_key}")
        if not isinstance(levels, dict):
            raise ValueError(f"{sensor_key} levels must be an object")

        sensor = {}
        for level in ("normal", "warn", "critical"):
            bounds = levels.get(level) or {}
            if not isinstance(bounds, dict):
                raise ValueError(f"{sensor_key}.{level} must be an object")
            sensor[level] = sanitize_bounds(sensor_key, level, bounds)

        validate_threshold_envelope(sensor_key, sensor)
        sanitized[sensor_key] = sensor

    return sanitized


def sanitize_bounds(sensor_key, level, bounds):
    min_value = sanitize_number(bounds.get("min"), f"{sensor_key}.{level}.min")
    max_value = sanitize_number(bounds.get("max"), f"{sensor_key}.{level}.max")
    if min_value is not None and max_value is not None and min_value > max_value:
        raise ValueError(f"{sensor_key}.{level}.min must be <= max")
    return {"min": min_value, "max": max_value}


def sanitize_number(value, label):
    if value is None or value == "":
        return None
    if isinstance(value, bool):
        raise ValueError(f"{label} must be numeric")
    try:
        number = float(value)
    except Exception as exc:
        raise ValueError(f"{label} must be numeric") from exc
    if not (number == number and abs(number) != float("inf")):
        raise ValueError(f"{label} must be finite")
    return number


def validate_threshold_envelope(sensor_key, sensor):
    normal = sensor["normal"]
    warn = sensor["warn"]
    critical = sensor["critical"]
    require_min_at_or_below(sensor_key, "warn.min", warn.get("min"), "normal.min", normal.get("min"))
    require_max_at_or_above(sensor_key, "warn.max", warn.get("max"), "normal.max", normal.get("max"))
    require_min_at_or_below(sensor_key, "critical.min", critical.get("min"), "warn.min", warn.get("min"))
    require_max_at_or_above(sensor_key, "critical.max", critical.get("max"), "warn.max", warn.get("max"))


def require_min_at_or_below(sensor_key, left_label, left, right_label, right):
    if left is not None and right is not None and left > right:
        raise ValueError(f"{sensor_key}: {left_label} must be <= {right_label}")


def require_max_at_or_above(sensor_key, left_label, left, right_label, right):
    if left is not None and right is not None and left < right:
        raise ValueError(f"{sensor_key}: {left_label} must be >= {right_label}")


def save_threshold_config(zone_id, config_version, thresholds):
    thresholds_json = json.dumps(thresholds, sort_keys=True)
    with lock:
        db_conn.execute(
            """
            INSERT INTO threshold_config(
                tenant_id,
                greenhouse_id,
                zone_id,
                config_version,
                thresholds_json,
                applied_at
            ) VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
            ON CONFLICT(tenant_id, greenhouse_id, zone_id, config_version)
            DO UPDATE SET
                thresholds_json = excluded.thresholds_json,
                applied_at = CURRENT_TIMESTAMP
            """,
            (TENANT_ID, GREENHOUSE_ID, zone_id, int(config_version), thresholds_json),
        )
        db_conn.commit()

    threshold_configs[zone_id] = {
        "config_version": int(config_version),
        "thresholds": thresholds,
    }


def publish_threshold_alert(device_id, zone_id, sensor_key, severity, value, threshold, config_version):
    level = "critical" if severity == "CRITICAL" else "warn" if severity == "WARNING" else "normal"
    bounds = threshold.get(level, {}) if isinstance(threshold, dict) else {}
    message = build_threshold_alert_message(sensor_key, severity, value, bounds)
    payload = {
        "alert_id": uuid_str(),
        "gateway_id": GATEWAY_ID,
        "zone_id": zone_id,
        "device_id": device_id,
        "sensor_key": sensor_key,
        "severity": severity,
        "message": message,
        "source": "edge",
        "threshold_version": config_version,
        "current_value": value,
        "threshold_min": bounds.get("min"),
        "threshold_max": bounds.get("max"),
        "timestamp": now_iso(),
    }
    publish_or_buffer(uplink_topic("alert"), payload)
    print(f"[THRESHOLD] alert zone={zone_id} device={device_id} sensor={sensor_key} severity={severity} value={value}", flush=True)


def build_threshold_alert_message(sensor_key, severity, value, bounds):
    if severity == "INFO":
        return f"{sensor_key} recovered to {value:.2f}."
    min_value = bounds.get("min")
    max_value = bounds.get("max")
    if min_value is not None and value < min_value:
        return f"{sensor_key} {severity.lower()} low: {value:.2f} < {min_value:.2f}."
    if max_value is not None and value > max_value:
        return f"{sensor_key} {severity.lower()} high: {value:.2f} > {max_value:.2f}."
    return f"{sensor_key} {severity.lower()} threshold transition: {value:.2f}."


def classify_threshold_value(value, threshold):
    critical = threshold.get("critical") or {}
    warn = threshold.get("warn") or {}

    if bound_breached(value, critical):
        return "CRITICAL"
    if bound_breached(value, warn):
        return "WARNING"
    return "OK"


def bound_breached(value, bounds):
    min_value = bounds.get("min")
    max_value = bounds.get("max")
    if min_value is not None and value < min_value:
        return True
    if max_value is not None and value > max_value:
        return True
    return False


def save_threshold_alert_state(zone_id, device_id, sensor_key, severity, config_version):
    with lock:
        db_conn.execute(
            """
            INSERT INTO threshold_alert_state(
                tenant_id,
                greenhouse_id,
                zone_id,
                device_id,
                sensor_key,
                severity,
                threshold_version,
                updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
            ON CONFLICT(tenant_id, greenhouse_id, zone_id, device_id, sensor_key)
            DO UPDATE SET
                severity = excluded.severity,
                threshold_version = excluded.threshold_version,
                updated_at = CURRENT_TIMESTAMP
            """,
            (TENANT_ID, GREENHOUSE_ID, zone_id, device_id, sensor_key, severity, config_version),
        )
        db_conn.commit()

    threshold_alert_states[(zone_id, device_id, sensor_key)] = {
        "severity": severity,
        "threshold_version": config_version,
    }


def evaluate_thresholds(device_id, zone_id, metrics):
    config = threshold_configs.get(zone_id)
    if not config:
        return

    thresholds = config.get("thresholds") or {}
    config_version = config.get("config_version")
    for sensor_key, value in metrics.items():
        threshold = thresholds.get(sensor_key)
        if not threshold:
            continue

        severity = classify_threshold_value(value, threshold)
        state_key = (zone_id, device_id, sensor_key)
        previous = threshold_alert_states.get(state_key, {}).get("severity", "OK")
        if severity == previous:
            continue

        save_threshold_alert_state(zone_id, device_id, sensor_key, severity, config_version)
        if severity == "OK":
            if previous != "OK":
                publish_threshold_alert(device_id, zone_id, sensor_key, "INFO", value, threshold, config_version)
            continue

        publish_threshold_alert(device_id, zone_id, sensor_key, severity, value, threshold, config_version)


def publish_zone_config(device_id, zone_id=None, zone_name=None):
    payload = {
        "zone_id": zone_id,
        "zone_name": zone_name,
        "ts": now_iso(),
    }
    local_client.publish(local_zone_topic(device_id, "config"), json.dumps(payload))


def evaluate_local_safety_rules(device_id, metrics):
    soil_moisture = metrics.get("soil_moist")
    if soil_moisture is None:
        return

    previous_state = last_irrigation_state.get(device_id)
    desired_state = None
    if soil_moisture < 20.0:
        desired_state = 1
    elif soil_moisture > 40.0:
        desired_state = 0

    if desired_state is None or desired_state == previous_state:
        return

    command = {
        "channel": 1,
        "state": desired_state,
        "source": "edge_safety_rule",
        "ts": now_iso(),
    }
    local_client.publish(local_zone_topic(device_id, "command/output"), json.dumps(command))
    last_irrigation_state[device_id] = desired_state
    print(
        f"[SAFETY] device={device_id} soil_moist={soil_moisture:.2f} -> irrigation={desired_state}",
        flush=True,
    )


def handle_local_announce(device_id, payload_text):
    metadata = {}
    firmware_version = None
    event_type = "DEVICE_DISCOVERED"
    announced_zone_id = None
    announced_zone_name = None

    try:
        body = json.loads(payload_text)
        if isinstance(body, dict):
            firmware_version = body.get("firmware_version")
            announced_zone_id = normalize_zone_field(body.get("zone_id"))
            announced_zone_name = normalize_zone_field(body.get("zone_name"))
            metadata = body.get("metadata", {})
            if not isinstance(metadata, dict):
                metadata = {}
            if announced_zone_id is None:
                announced_zone_id = normalize_zone_field(metadata.get("zone_id"))
            if announced_zone_name is None:
                announced_zone_name = normalize_zone_field(metadata.get("zone_name"))
            incoming_type = body.get("type")
            if isinstance(incoming_type, str) and incoming_type.strip():
                event_type = incoming_type.strip().upper()
    except Exception:
        metadata = {}

    if event_type in {"DEVICE_REMOVED", "DEVICE_DECOMMISSIONED"}:
        remove_zone_registry(device_id)
        last_metrics.pop(device_id, None)
        last_snapshot_at.pop(device_id, None)
        last_irrigation_state.pop(device_id, None)
        publish_registry_event(
            "DEVICE_DECOMMISSIONED",
            device_id,
            firmware_version=firmware_version,
            metadata=metadata,
            zone_id=None,
            zone_name=None,
        )
        return

    publish_registry_event("DEVICE_DISCOVERED", device_id, firmware_version, metadata)

    cached_assignment = zone_registry.get(device_id)
    if cached_assignment:
        cached_zone_id = normalize_zone_field(cached_assignment.get("zone_id"))
        cached_zone_name = normalize_zone_field(cached_assignment.get("zone_name"))

        if announced_zone_id == cached_zone_id and announced_zone_name == cached_zone_name:
            return

        now_ts = time.time()
        previous_rehydrate = last_rehydrate_at.get(device_id, 0.0)
        if now_ts - previous_rehydrate < CONFIG_REHYDRATE_COOLDOWN_SEC:
            return

        publish_zone_config(device_id, cached_zone_id, cached_zone_name)
        last_rehydrate_at[device_id] = now_ts
        print(
            f"[REGISTRY] Rehydrated config for device={device_id} zone_id={cached_zone_id} zone_name={cached_zone_name}",
            flush=True,
        )


def handle_local_telemetry(device_id, payload_text):
    try:
        raw = json.loads(payload_text)
    except Exception:
        print(f"[LOCAL] Invalid telemetry JSON from {device_id}: {payload_text}", flush=True)
        return

    if not isinstance(raw, dict):
        return

    metrics = normalize_metrics(raw)
    if not metrics:
        return

    zone_id, zone_name = resolve_zone(device_id)
    evaluate_local_safety_rules(device_id, metrics)
    evaluate_thresholds(device_id, zone_id, metrics)
    changed = compute_delta(device_id, metrics)

    base_payload = {
        "tenant_id": TENANT_ID,
        "greenhouse_id": GREENHOUSE_ID,
        "gateway_id": GATEWAY_ID,
        "device_id": device_id,
        "zone_id": zone_id,
        "zone_name": zone_name,
        "timestamp": now_iso(),
    }

    if changed:
        payload = dict(base_payload)
        payload.update(
            {
                "event_id": uuid_str(),
                "kind": "delta",
                "metrics": changed,
            }
        )
        publish_or_buffer(uplink_topic("telemetry"), payload)

    now_ts = time.time()
    if now_ts - last_snapshot_at.get(device_id, 0.0) >= SNAPSHOT_INTERVAL_SEC:
        payload = dict(base_payload)
        payload.update(
            {
                "event_id": uuid_str(),
                "kind": "snapshot",
                "metrics": metrics,
            }
        )
        publish_or_buffer(uplink_topic("telemetry"), payload)
        last_snapshot_at[device_id] = now_ts


def resolve_target_device(command):
    device_id = command.get("device_id")
    if device_id:
        return device_id

    zone_id = command.get("zone_id")
    if not zone_id:
        return None

    for current_device_id, config in zone_registry.items():
        if config.get("zone_id") == zone_id:
            return current_device_id
    return None


def translate_command(command):
    payload = command.get("payload", {})
    if not isinstance(payload, dict):
        payload = {}

    if "channel" in payload and "state" in payload:
        try:
            return {
                "channel": int(payload.get("channel")),
                "state": 1 if int(payload.get("state")) else 0,
                "command_id": command.get("command_id"),
            }
        except Exception:
            return None

    command_type = (command.get("type") or "").upper()
    if command_type == "IRRIGATION_ON":
        return {"channel": 1, "state": 1, "command_id": command.get("command_id")}
    if command_type == "IRRIGATION_OFF":
        return {"channel": 1, "state": 0, "command_id": command.get("command_id")}

    return None


def handle_downlink_registry(payload):
    command_id = payload.get("command_id") or uuid_str()
    command_type = (payload.get("type") or "").upper()
    device_id = payload.get("device_id")

    if command_type == "ZONE_REGISTRY_SYNC":
        zones = payload.get("zones")
        if not isinstance(zones, list):
            publish_command_ack("FAILED", command_id, reason="Invalid or missing zones list")
            return

        assigned_devices = set()

        for zone in zones:
            if not isinstance(zone, dict):
                continue

            synced_device_id = zone.get("device_id")
            if not synced_device_id:
                continue

            zone_id = zone.get("zone_id") or uuid_str()
            zone_name = zone.get("zone_name") or f"zone-{zone_id[:8]}"
            metadata = zone.get("metadata") if isinstance(zone.get("metadata"), dict) else {}

            save_zone_registry(synced_device_id, zone_id, zone_name, metadata)
            publish_zone_config(synced_device_id, zone_id, zone_name)
            publish_registry_event(
                "ZONE_ASSIGNMENT_APPLIED",
                synced_device_id,
                metadata=metadata,
                command_id=command_id,
                zone_id=zone_id,
                zone_name=zone_name,
            )
            assigned_devices.add(synced_device_id)

        for existing_device_id in list(zone_registry.keys()):
            if existing_device_id in assigned_devices:
                continue

            remove_zone_registry(existing_device_id)
            publish_zone_config(existing_device_id)
            publish_registry_event(
                "ZONE_UNASSIGNED_APPLIED",
                existing_device_id,
                command_id=command_id,
                zone_id=None,
                zone_name=None,
            )

        publish_command_ack(
            "APPLIED",
            command_id,
            reason=f"Applied registry sync for {len(assigned_devices)} zones",
        )
        return

    if not device_id:
        publish_command_ack("FAILED", command_id, reason="Missing device_id for registry command")
        return

    if command_type == "ASSIGN_ZONE":
        zone_id = payload.get("zone_id") or uuid_str()
        zone_name = payload.get("zone_name") or f"zone-{zone_id[:8]}"
        metadata = payload.get("metadata") if isinstance(payload.get("metadata"), dict) else {}

        save_zone_registry(device_id, zone_id, zone_name, metadata)
        publish_zone_config(device_id, zone_id, zone_name)

        publish_registry_event(
            "ZONE_ASSIGNMENT_APPLIED",
            device_id,
            metadata=metadata,
            command_id=command_id,
            zone_id=zone_id,
            zone_name=zone_name,
        )
        publish_command_ack("APPLIED", command_id, device_id=device_id, zone_id=zone_id)
        return

    if command_type == "UNASSIGN_ZONE":
        remove_zone_registry(device_id)
        publish_zone_config(device_id)
        publish_registry_event(
            "ZONE_UNASSIGNED_APPLIED",
            device_id,
            command_id=command_id,
            zone_id=None,
            zone_name=None,
        )
        publish_command_ack("APPLIED", command_id, device_id=device_id)
        return

    publish_command_ack("FAILED", command_id, device_id=device_id, reason=f"Unknown registry command type: {command_type}")


def handle_downlink_command(payload):
    command_id = payload.get("command_id") or uuid_str()
    device_id = resolve_target_device(payload)

    if not device_id:
        publish_command_ack("FAILED", command_id, reason="Unable to resolve target device")
        return

    translated = translate_command(payload)
    if translated is None:
        publish_command_ack("FAILED", command_id, device_id=device_id, reason="Unsupported command payload")
        return

    local_client.publish(local_zone_topic(device_id, "command/output"), json.dumps(translated))
    zone_id, _ = resolve_zone(device_id)
    publish_command_ack("FORWARDED", command_id, device_id=device_id, zone_id=zone_id)


def handle_downlink_threshold(payload):
    command_id = payload.get("command_id") or uuid_str()
    zone_id = payload.get("zone_id")
    config_version = payload.get("config_version")

    print(
        f"[THRESHOLD] Downlink received command_id={command_id} zone={zone_id or '-'} version={config_version or '-'}",
        flush=True,
    )

    def fail(reason):
        print(
            f"[THRESHOLD] Rejected command_id={command_id} zone={zone_id or '-'} reason={reason}",
            flush=True,
        )
        publish_command_ack(
            "FAILED",
            command_id,
            zone_id=zone_id,
            reason=reason,
            config_version=config_version,
            ack_type="THRESHOLD_CONFIG",
        )

    if payload.get("tenant_id") != TENANT_ID or payload.get("greenhouse_id") != GREENHOUSE_ID:
        fail("Threshold payload scope does not match gateway tenant/greenhouse")
        return

    payload_gateway_id = payload.get("gateway_id")
    if payload_gateway_id and payload_gateway_id != GATEWAY_ID:
        fail("Threshold payload gateway_id does not match this gateway")
        return

    if not zone_id:
        fail("Missing zone_id")
        return

    try:
        config_version = int(config_version)
    except Exception:
        fail("Invalid or missing config_version")
        return

    try:
        thresholds = sanitize_threshold_payload(payload.get("thresholds"))
        save_threshold_config(zone_id, config_version, thresholds)
    except Exception as exc:
        fail(str(exc))
        return

    publish_command_ack(
        "APPLIED",
        command_id,
        zone_id=zone_id,
        reason=f"Applied threshold config v{config_version} for zone {zone_id}",
        config_version=config_version,
        ack_type="THRESHOLD_CONFIG",
    )
    print(f"[THRESHOLD] Applied zone={zone_id} version={config_version}", flush=True)


def on_local_connect(client, userdata, flags, rc):
    if rc != 0:
        print(f"[LOCAL] Connect failed (RC {rc})", flush=True)
        return

    print("[LOCAL] Connected successfully.", flush=True)
    client.subscribe(local_zone_topic("+", "registry/announce"))
    client.subscribe(local_zone_topic("+", "telemetry/raw"))


def on_local_message(client, userdata, msg):
    parsed = parse_local_topic(msg.topic)
    if not parsed:
        return

    greenhouse_id, device_id, stream = parsed
    if greenhouse_id != GREENHOUSE_ID:
        return

    payload = msg.payload.decode()

    if stream == "registry/announce":
        handle_local_announce(device_id, payload)
        return

    if stream == "telemetry/raw":
        handle_local_telemetry(device_id, payload)
        return


def on_cloud_connect(client, userdata, flags, rc):
    global cloud_connected

    if rc != 0:
        print(f"[CLOUD] Connect failed (RC {rc})", flush=True)
        cloud_connected = False
        return

    cloud_connected = True
    client.subscribe(downlink_topic("#"))
    print("[CLOUD] Connected successfully.", flush=True)
    publish_gateway_status("ONLINE")
    flush_offline_buffer()


def on_cloud_disconnect(client, userdata, rc):
    global cloud_connected
    cloud_connected = False
    print("[CLOUD] Disconnected, switching to offline buffering.", flush=True)


def on_cloud_message(client, userdata, msg):
    topic = msg.topic
    payload_text = msg.payload.decode()

    try:
        payload = json.loads(payload_text)
    except Exception:
        print(f"[CLOUD] Invalid JSON on {topic}: {payload_text}", flush=True)
        return

    if topic == downlink_topic("registry"):
        handle_downlink_registry(payload)
        return

    if topic == downlink_topic("command"):
        handle_downlink_command(payload)
        return

    if topic == downlink_topic("threshold"):
        handle_downlink_threshold(payload)
        return


def buffer_flush_loop():
    while True:
        if cloud_connected:
            flush_offline_buffer()
        time.sleep(BUFFER_FLUSH_INTERVAL_SEC)


def heartbeat_loop():
    while True:
        publish_gateway_status("ONLINE" if cloud_connected else "OFFLINE")
        time.sleep(STATUS_HEARTBEAT_SEC)


if __name__ == "__main__":
    print("Starting GMS Edge Engine...", flush=True)
    print(
        f"[CFG] tenant={TENANT_ID} greenhouse={GREENHOUSE_ID} gateway={GATEWAY_ID}",
        flush=True,
    )

    db_conn = init_db()
    load_zone_registry_cache()
    load_threshold_config_cache()
    load_threshold_alert_state_cache()

    local_client = mqtt.Client(client_id=f"edge-local-{GATEWAY_ID}")
    local_client.on_connect = on_local_connect
    local_client.on_message = on_local_message

    cloud_client = mqtt.Client(client_id=f"edge-cloud-{GATEWAY_ID}")
    cloud_client.username_pw_set(CLOUD_USER, CLOUD_PASS)
    cloud_client.on_connect = on_cloud_connect
    cloud_client.on_disconnect = on_cloud_disconnect
    cloud_client.on_message = on_cloud_message

    cloud_client.connect_async(CLOUD_HOST, CLOUD_PORT, 60)
    cloud_client.loop_start()

    flush_thread = threading.Thread(target=buffer_flush_loop, daemon=True)
    flush_thread.start()

    heartbeat_thread = threading.Thread(target=heartbeat_loop, daemon=True)
    heartbeat_thread.start()

    print(f"[LOCAL] Connecting to {LOCAL_HOST}:{LOCAL_PORT}...", flush=True)
    while True:
        try:
            local_client.connect(LOCAL_HOST, LOCAL_PORT, 60)
            break
        except Exception as exc:
            print(f"[LOCAL] Broker not ready, retrying in 5 seconds... ({exc})", flush=True)
            time.sleep(5)

    local_client.loop_forever()
