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
int publish_telemetry(float airHum, float airTemp, float soilMoist, float soilTemp) {
    if (!mqttClient.connected()) {
        connectMqtt();
    }

    // Serialize to JSON
    String jsonPayload = "{";
    jsonPayload += "\"air_temp\":" + String(airTemp) + ",";
    jsonPayload += "\"air_hum\":" + String(airHum) + ",";
    jsonPayload += "\"soil_moist\":" + String(soilMoist) + ",";
    jsonPayload += "\"soil_temp\":" + String(soilTemp);
    jsonPayload += "}";

    mqttClient.beginMessage(topic);
    mqttClient.print(jsonPayload);
    mqttClient.endMessage();

    Serial.println("Published telemetry: " + jsonPayload);
    return 1;
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
