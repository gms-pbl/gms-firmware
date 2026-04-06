#include <Arduino.h>
#include <RPC.h>
#include <ArduinoMqttClient.h>
#include <WiFi.h> // Using WiFi abstraction

#include "TelemetryIpc.h"

using namespace gms_edge;

#define USE_WIFI 1

const char ssid[] = "your_network_ssid";
const char pass[] = "your_network_password";

const char broker[] = "192.168.1.50";
int        port     = 1883;
const char topic[]  = "telemetry/zone1";

#if USE_WIFI
  WiFiClient netClient;
#else
  // EthernetClient netClient; 
#endif

MqttClient mqttClient(netClient);

void connectNetwork() {
    Serial.print("Connecting to network...");
    while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");
}

void connectMqtt() {
    Serial.print("Connecting to MQTT broker...");
    while (!mqttClient.connect(broker, port)) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");
}

// RPC Callback from M7
void publish_telemetry(uint8_t* payload, size_t size) {
    if (size != sizeof(TelemetryIpc)) return;

    TelemetryIpc* telemetry = (TelemetryIpc*)payload;

    if (!mqttClient.connected()) {
        connectMqtt();
    }

    // Serialize to JSON (Simplified for now)
    String jsonPayload = "{";
    jsonPayload += "\"air_temp\":" + String(telemetry->air_temperature) + ",";
    jsonPayload += "\"air_hum\":" + String(telemetry->air_humidity) + ",";
    jsonPayload += "\"soil_moist\":" + String(telemetry->soil_moisture) + ",";
    jsonPayload += "\"soil_temp\":" + String(telemetry->soil_temperature);
    jsonPayload += "}";

    mqttClient.beginMessage(topic);
    mqttClient.print(jsonPayload);
    mqttClient.endMessage();

    Serial.println("Published telemetry: " + jsonPayload);
}

void setup() {
    Serial.begin(115200);
    RPC.begin();
    
    // Register the RPC function to be callable from M7
    RPC.bind("publish_telemetry", publish_telemetry);

    connectNetwork();
    connectMqtt();
}

void loop() {
    mqttClient.poll();
    delay(10);
}
