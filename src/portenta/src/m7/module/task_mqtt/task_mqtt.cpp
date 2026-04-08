#include "task_mqtt.h"
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include "../task_io/task_io.h"

namespace gms_edge {
namespace module {
namespace task_mqtt {

#if USE_WIFI
  WiFiClient netClient;
#else
  // EthernetClient netClient; 
#endif

MqttClient mqttClient(netClient);

void connectNetwork() {
    Serial.print("Connecting to network...");
    while (WiFi.begin(WIFI_SSID, WIFI_PASS) != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");
}

void connectMqtt() {
    Serial.print("Connecting to MQTT broker...");
    mqttClient.setConnectionTimeout(5000); // 5 second timeout instead of default 30
    
    // Fix: Convert the string IP from .env to an actual IPAddress object
    IPAddress brokerIP;
    brokerIP.fromString(MQTT_BROKER);

    while (!mqttClient.connect(brokerIP, MQTT_PORT)) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");
}

int publish_telemetry(float airHum, float airTemp, float soilMoist, float soilTemp, float soilCond, float soilPh, float soilN, float soilP, float soilK, float soilSal, float soilTds, int din0, int din1, int din2, int din3) {
    if (!mqttClient.connected()) {
        connectMqtt();
    }

    // Serialize full payload to JSON
    String jsonPayload = "{";
    jsonPayload += "\"air_temp\":" + String(airTemp) + ",";
    jsonPayload += "\"air_hum\":" + String(airHum) + ",";
    jsonPayload += "\"soil_moist\":" + String(soilMoist) + ",";
    jsonPayload += "\"soil_temp\":" + String(soilTemp) + ",";
    jsonPayload += "\"soil_cond\":" + String(soilCond) + ",";
    jsonPayload += "\"soil_ph\":" + String(soilPh) + ",";
    jsonPayload += "\"soil_n\":" + String(soilN) + ",";
    jsonPayload += "\"soil_p\":" + String(soilP) + ",";
    jsonPayload += "\"soil_k\":" + String(soilK) + ",";
    jsonPayload += "\"soil_salinity\":" + String(soilSal) + ",";
    jsonPayload += "\"soil_tds\":" + String(soilTds) + ",";
    jsonPayload += "\"din_00\":" + String(din0) + ",";
    jsonPayload += "\"din_01\":" + String(din1) + ",";
    jsonPayload += "\"din_02\":" + String(din2) + ",";
    jsonPayload += "\"din_03\":" + String(din3);
    jsonPayload += "}";

    mqttClient.beginMessage(MQTT_TOPIC_TELEMETRY);
    mqttClient.print(jsonPayload);
    mqttClient.endMessage();

    Serial.println("Published telemetry: " + jsonPayload);
    return 1;
}

// Callback for incoming MQTT messages
void onMqttMessage(int messageSize) {
    String topic = mqttClient.messageTopic();
    String payload = "";

    while (mqttClient.available()) {
        payload += (char)mqttClient.read();
    }

    Serial.print("Received message on topic: ");
    Serial.println(topic);
    Serial.print("Payload: ");
    Serial.println(payload);

    if (topic == MQTT_TOPIC_COMMAND_OUTPUT) {
        // Very basic JSON parsing for {"channel": 2, "state": 1}
        int chanIndex = payload.indexOf("\"channel\":");
        int stateIndex = payload.indexOf("\"state\":");

        if (chanIndex != -1 && stateIndex != -1) {
            // Extract the channel and state values
            int channel = payload.substring(chanIndex + 10, payload.indexOf(',', chanIndex)).toInt();
            int state = payload.substring(stateIndex + 8, payload.indexOf('}', stateIndex)).toInt();

            // Set output directly instead of RPC
            Serial.print("Executing task_io::set_output(");
            Serial.print(channel); Serial.print(", "); Serial.print(state); Serial.println(")");
            
            module::task_io::set_output(channel, state);
        } else {
            Serial.println("Invalid payload format for command.");
        }
    }
}

void init() {
    connectNetwork();
    connectMqtt();

    // Subscribe to the command topic and register callback
    mqttClient.onMessage(onMqttMessage);
    mqttClient.subscribe(MQTT_TOPIC_COMMAND_OUTPUT);
}

void process() {
    if (WiFi.status() != WL_CONNECTED) {
        connectNetwork();
        connectMqtt();
        mqttClient.onMessage(onMqttMessage);
        mqttClient.subscribe(MQTT_TOPIC_COMMAND_OUTPUT);
    } else if (!mqttClient.connected()) {
        connectMqtt();
        mqttClient.onMessage(onMqttMessage);
        mqttClient.subscribe(MQTT_TOPIC_COMMAND_OUTPUT);
    }
    mqttClient.poll();
}

} // namespace task_mqtt
} // namespace module
} // namespace gms_edge
