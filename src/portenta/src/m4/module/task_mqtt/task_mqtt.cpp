#include "task_mqtt.h"

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
    while (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");
}

// RPC Callback from M7
int publish_telemetry(float airHum, float airTemp, float soilMoist, float soilTemp, float soilCond, float soilPh, float soilN, float soilP, float soilK, float soilSal, float soilTds) {
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
    jsonPayload += "\"soil_tds\":" + String(soilTds);
    jsonPayload += "}";

    mqttClient.beginMessage(MQTT_TOPIC_TELEMETRY);
    mqttClient.print(jsonPayload);
    mqttClient.endMessage();

    Serial.println("Published telemetry: " + jsonPayload);
    return 1;
}

void init() {
    // Register the RPC function to be callable from M7
    RPC.bind("publish_telemetry", publish_telemetry);

    connectNetwork();
    connectMqtt();
}

void process() {
    mqttClient.poll();
}

} // namespace task_mqtt
} // namespace module
} // namespace gms_edge