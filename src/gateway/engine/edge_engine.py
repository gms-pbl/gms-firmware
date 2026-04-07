import os
import time
import json
import sqlite3
import threading
import paho.mqtt.client as mqtt

# Local Mosquitto Broker
LOCAL_HOST = os.environ.get("LOCAL_BROKER_HOST", "mosquitto")
LOCAL_PORT = int(os.environ.get("LOCAL_BROKER_PORT", 1883))

# Cloud EMQX Broker
CLOUD_HOST = os.environ.get("CLOUD_BROKER_HOST", "emqx.cloud.example.com")
CLOUD_PORT = int(os.environ.get("CLOUD_BROKER_PORT", 1883))
CLOUD_USER = os.environ.get("CLOUD_USERNAME", "edge_gateway_1")
CLOUD_PASS = os.environ.get("CLOUD_PASSWORD", "secret")

# State
cloud_connected = False
db_conn = None

def init_db():
    os.makedirs("/app/data", exist_ok=True)
    db_path = "/app/data/offline_buffer.db"
    conn = sqlite3.connect(db_path, check_same_thread=False)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS telemetry
                 (id INTEGER PRIMARY KEY AUTOINCREMENT, topic TEXT, payload TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)''')
    conn.commit()
    print(f"[DB] Initialized at {db_path}", flush=True)
    return conn

def evaluate_safety_rules(payload_str):
    try:
        data = json.loads(payload_str)
        # Hysteresis safety logic:
        # If soil moisture < 20%, turn on irrigation (Channel 1)
        # If > 40%, turn off irrigation
        soil_moisture = data.get("soil_moist")
        
        if soil_moisture is not None:
            if soil_moisture < 20.0:
                print("[SAFETY] Soil moisture critically low. Triggering Irrigation (ON).", flush=True)
                cmd = json.dumps({"channel": 1, "state": 1})
                local_client.publish("commands/zone1/output", cmd)
            elif soil_moisture > 40.0:
                print("[SAFETY] Soil moisture recovered. Triggering Irrigation (OFF).", flush=True)
                cmd = json.dumps({"channel": 1, "state": 0})
                local_client.publish("commands/zone1/output", cmd)
                
    except Exception as e:
        pass

def flush_offline_buffer():
    global cloud_connected
    if not cloud_connected:
        return
        
    try:
        c = db_conn.cursor()
        c.execute("SELECT id, topic, payload FROM telemetry ORDER BY id ASC LIMIT 50")
        rows = c.fetchall()
        
        if not rows:
            return

        print(f"[CLOUD] Flushing {len(rows)} buffered records...", flush=True)
        for row_id, topic, payload in rows:
            # Forward to cloud
            cloud_client.publish(f"cloud/{topic}", payload)
            # Delete from buffer
            c.execute("DELETE FROM telemetry WHERE id = ?", (row_id,))
            
        db_conn.commit()
    except Exception as e:
        print(f"[CLOUD] Flush error: {e}", flush=True)

# --- LOCAL CLIENT CALLBACKS ---
def on_local_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[LOCAL] Connected successfully.", flush=True)
        client.subscribe("telemetry/#")
    else:
        print(f"[LOCAL] Connect failed (RC {rc})", flush=True)

def on_local_message(client, userdata, msg):
    payload = msg.payload.decode()
    topic = msg.topic
    print(f"[LOCAL] Received: {topic} -> {payload}", flush=True)
    
    # 1. Evaluate Local Safety Rules (Hysteresis)
    evaluate_safety_rules(payload)
    
    # 2. Forward or Buffer Data
    if cloud_connected:
        cloud_client.publish(f"cloud/{topic}", payload)
        print(f"[CLOUD] Forwarded: cloud/{topic}", flush=True)
    else:
        try:
            c = db_conn.cursor()
            c.execute("INSERT INTO telemetry (topic, payload) VALUES (?, ?)", (topic, payload))
            db_conn.commit()
            print(f"[DB] Buffered: {topic}", flush=True)
        except Exception as e:
            print(f"[DB] Insert failed: {e}", flush=True)

# --- CLOUD CLIENT CALLBACKS ---
def on_cloud_connect(client, userdata, flags, rc):
    global cloud_connected
    if rc == 0:
        print("[CLOUD] Connected successfully.", flush=True)
        cloud_connected = True
        # Subscribe to cloud commands destined for this edge
        client.subscribe("cloud/commands/#")
        # Trigger an immediate buffer flush
        flush_offline_buffer()
    else:
        print(f"[CLOUD] Connect failed (RC {rc})", flush=True)

def on_cloud_disconnect(client, userdata, rc):
    global cloud_connected
    cloud_connected = False
    print("[CLOUD] Disconnected. Switching to offline buffer mode.", flush=True)

def on_cloud_message(client, userdata, msg):
    payload = msg.payload.decode()
    topic = msg.topic
    print(f"[CLOUD] Received Command: {topic} -> {payload}", flush=True)
    
    # Map cloud/commands/zone1/output -> commands/zone1/output
    if topic.startswith("cloud/commands/"):
        local_topic = topic.replace("cloud/commands/", "commands/")
        local_client.publish(local_topic, payload)
        print(f"[LOCAL] Forwarded Command: {local_topic}", flush=True)

def background_flusher():
    while True:
        if cloud_connected:
            flush_offline_buffer()
        time.sleep(5)

if __name__ == "__main__":
    print("Starting GMS Edge Engine...", flush=True)
    time.sleep(2)
    
    db_conn = init_db()

    # Setup Local Client
    local_client = mqtt.Client("EdgeEngine_Local")
    local_client.on_connect = on_local_connect
    local_client.on_message = on_local_message
    
    # Setup Cloud Client
    cloud_client = mqtt.Client("EdgeEngine_Cloud")
    cloud_client.username_pw_set(CLOUD_USER, CLOUD_PASS)
    cloud_client.on_connect = on_cloud_connect
    cloud_client.on_disconnect = on_cloud_disconnect
    cloud_client.on_message = on_cloud_message

    # We will try to connect to cloud, but we don't block if it fails (Offline first!)
    print(f"[CLOUD] Connecting to {CLOUD_HOST}:{CLOUD_PORT}...", flush=True)
    try:
        # Just use connect_async so it retries in background
        cloud_client.connect_async(CLOUD_HOST, CLOUD_PORT, 60)
        cloud_client.loop_start()
    except Exception as e:
        print(f"[CLOUD] Warning, couldn't initiate cloud connection: {e}", flush=True)

    # Start Flusher Thread
    flusher = threading.Thread(target=background_flusher, daemon=True)
    flusher.start()

    # Start Local Client Loop (Blocks main thread)
    print(f"[LOCAL] Connecting to {LOCAL_HOST}:{LOCAL_PORT}...", flush=True)
    while True:
        try:
            local_client.connect(LOCAL_HOST, LOCAL_PORT, 60)
            break
        except Exception as e:
            print(f"[LOCAL] Broker not ready, retrying in 5 seconds... ({e})", flush=True)
            time.sleep(5)

    local_client.loop_forever()