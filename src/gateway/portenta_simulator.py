import os
import json
import time
import math
import random
import paho.mqtt.client as mqtt

LOCAL_HOST = os.environ.get("LOCAL_BROKER_HOST", "127.0.0.1")
LOCAL_PORT = int(os.environ.get("LOCAL_BROKER_PORT", 1883))
TOPIC_TELEMETRY = "telemetry/zone1"
TOPIC_COMMANDS = "commands/zone1/#"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[SIMULATOR] Connected to Mosquitto at {LOCAL_HOST}:{LOCAL_PORT}")
        client.subscribe(TOPIC_COMMANDS)
        print(f"[SIMULATOR] Subscribed to {TOPIC_COMMANDS}")
    else:
        print(f"[SIMULATOR] Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    print(f"\n>>> [SIMULATOR] ACTUATOR COMMAND RECEIVED! <<<")
    print(f"    Topic: {msg.topic}")
    print(f"    Payload: {payload}")
    try:
        data = json.loads(payload)
        channel = data.get("channel")
        state = data.get("state")
        action = "ON" if state == 1 else "OFF"
        print(f"    ACTION: Physical Relay {channel} clicked {action}!\n")
    except:
        pass

if __name__ == "__main__":
    print("Starting Portenta Hardware Simulator...")
    
    client = mqtt.Client("Portenta_Simulator_Zone1")
    client.on_connect = on_connect
    client.on_message = on_message

    print("Connecting to broker...")
    client.connect(LOCAL_HOST, LOCAL_PORT, 60)
    client.loop_start()

    tick = 0
    while True:
        # Simulate realistic data that slowly fluctuates
        soil_moisture = 35.0 + math.sin(tick / 10.0) * 20.0  # Fluctuates between 15% and 55%
        
        telemetry = {
            "air_temp": round(24.5 + random.uniform(-0.5, 0.5), 2),
            "air_hum": round(55.2 + random.uniform(-1.0, 1.0), 2),
            "soil_moist": round(soil_moisture, 2),
            "soil_temp": round(18.4 + random.uniform(-0.2, 0.2), 2),
            "soil_cond": 1.20,
            "soil_ph": 6.80,
            "soil_n": 45.0,
            "soil_p": 20.0,
            "soil_k": 35.0,
            "soil_salinity": 0.5,
            "soil_tds": 250.0,
            "din_00": random.choice([0, 1]) if tick % 10 == 0 else 0, # Occasional blips
            "din_01": 0,
            "din_02": 1,
            "din_03": 0
        }
        
        payload = json.dumps(telemetry)
        print(f"[SIMULATOR] Publishing telemetry: Soil Moisture = {telemetry['soil_moist']}%")
        client.publish(TOPIC_TELEMETRY, payload)
        
        tick += 1
        time.sleep(10) # 10 second polling interval just like the real M7 code