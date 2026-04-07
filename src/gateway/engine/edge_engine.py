import os
import time
import sqlite3
import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to Local Broker successfully.")
        # Subscribe to all telemetry from all zones
        client.subscribe("telemetry/#")
    else:
        print("Failed to connect, return code %d\n", rc)

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    topic = msg.topic
    print(f"Received from {topic}: {payload}")
    
    # Store in Offline Buffer
    try:
        c = userdata['db'].cursor()
        c.execute("INSERT INTO telemetry (topic, payload) VALUES (?, ?)", (topic, payload))
        userdata['db'].commit()
    except Exception as e:
        print(f"Failed to insert into DB: {e}")

if __name__ == "__main__":
    print("Starting GMS Edge Engine...")
    
    # Wait for Mosquitto to start
    time.sleep(2)

    # Initialize Local SQLite Database for Offline Buffer
    os.makedirs("/app/data", exist_ok=True)
    db_path = "/app/data/offline_buffer.db"
    conn = sqlite3.connect(db_path, check_same_thread=False)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS telemetry
                 (id INTEGER PRIMARY KEY AUTOINCREMENT, topic TEXT, payload TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, synced BOOLEAN DEFAULT 0)''')
    conn.commit()
    print(f"Offline buffer DB initialized at {db_path}.")

    # Connect to Local MQTT Broker
    broker_host = os.environ.get("LOCAL_BROKER_HOST", "mosquitto")
    broker_port = int(os.environ.get("LOCAL_BROKER_PORT", 1883))
    
    client = mqtt.Client("EdgeEngine_Gateway", userdata={'db': conn})
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"Connecting to MQTT Broker at {broker_host}:{broker_port}...")
    while True:
        try:
            client.connect(broker_host, broker_port, 60)
            break
        except Exception as e:
            print(f"Broker not ready, retrying in 5 seconds... ({e})")
            time.sleep(5)

    client.loop_forever()