#include "task_mqtt.h"
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <cstdio>
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

void onMqttMessage(int messageSize);

namespace {

struct TelemetryPacket {
    float air_hum;
    float air_temp;
    float soil_moist;
    float soil_temp;
    float soil_cond;
    float soil_ph;
    float soil_n;
    float soil_p;
    float soil_k;
    float soil_sal;
    float soil_tds;
    int din_00;
    int din_01;
    int din_02;
    int din_03;
};

constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 5000;
constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 3000;
constexpr uint8_t TELEMETRY_QUEUE_DEPTH = 16;

rtos::MemoryPool<TelemetryPacket, TELEMETRY_QUEUE_DEPTH> telemetry_pool;
rtos::Queue<TelemetryPacket, TELEMETRY_QUEUE_DEPTH> telemetry_queue;

unsigned long last_wifi_attempt_ms = 0;
unsigned long last_mqtt_attempt_ms = 0;
bool mqtt_subscribed = false;

void configureSubscriptions() {
    if (mqtt_subscribed || !mqttClient.connected()) {
        return;
    }

    mqttClient.onMessage(onMqttMessage);
    if (mqttClient.subscribe(MQTT_TOPIC_COMMAND_OUTPUT)) {
        mqtt_subscribed = true;
        Serial.print("Subscribed to: ");
        Serial.println(MQTT_TOPIC_COMMAND_OUTPUT);
    }
}

bool attemptNetworkConnection() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    unsigned long now = millis();
    if (now - last_wifi_attempt_ms < WIFI_RETRY_INTERVAL_MS) {
        return false;
    }
    last_wifi_attempt_ms = now;

    Serial.print("Connecting to network...");
    int status = WiFi.begin(WIFI_SSID, WIFI_PASS);
    if (status == WL_CONNECTED) {
        Serial.println(" connected!");
        return true;
    }

    Serial.println(" not connected yet.");
    return false;
}

bool attemptMqttConnection() {
    if (mqttClient.connected()) {
        return true;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    unsigned long now = millis();
    if (now - last_mqtt_attempt_ms < MQTT_RETRY_INTERVAL_MS) {
        return false;
    }
    last_mqtt_attempt_ms = now;

    Serial.print("Connecting to MQTT broker...");
    IPAddress brokerIP;
    bool broker_is_ip = brokerIP.fromString(MQTT_BROKER);

    bool connected = false;
    if (broker_is_ip) {
        connected = mqttClient.connect(brokerIP, MQTT_PORT);
    } else {
        connected = mqttClient.connect(MQTT_BROKER, MQTT_PORT);
    }

    if (!connected) {
        Serial.println(" failed.");
        return false;
    }

    Serial.println(" connected!");
    mqtt_subscribed = false;
    configureSubscriptions();
    return true;
}

bool publishPacket(const TelemetryPacket& packet) {
    char payload[512];
    int written = snprintf(
        payload,
        sizeof(payload),
        "{\"air_temp\":%.2f,\"air_hum\":%.2f,\"soil_moist\":%.2f,\"soil_temp\":%.2f,\"soil_cond\":%.2f,\"soil_ph\":%.2f,\"soil_n\":%.2f,\"soil_p\":%.2f,\"soil_k\":%.2f,\"soil_salinity\":%.2f,\"soil_tds\":%.2f,\"din_00\":%d,\"din_01\":%d,\"din_02\":%d,\"din_03\":%d}",
        packet.air_temp,
        packet.air_hum,
        packet.soil_moist,
        packet.soil_temp,
        packet.soil_cond,
        packet.soil_ph,
        packet.soil_n,
        packet.soil_p,
        packet.soil_k,
        packet.soil_sal,
        packet.soil_tds,
        packet.din_00,
        packet.din_01,
        packet.din_02,
        packet.din_03
    );

    if (written <= 0 || static_cast<size_t>(written) >= sizeof(payload)) {
        Serial.println("Telemetry payload serialization failed.");
        return false;
    }

    if (mqttClient.beginMessage(MQTT_TOPIC_TELEMETRY) == 0) {
        return false;
    }

    mqttClient.print(payload);
    if (mqttClient.endMessage() == 0) {
        return false;
    }

    Serial.print("Published telemetry: ");
    Serial.println(payload);
    return true;
}

} // namespace

int publish_telemetry(float airHum, float airTemp, float soilMoist, float soilTemp, float soilCond, float soilPh, float soilN, float soilP, float soilK, float soilSal, float soilTds, int din0, int din1, int din2, int din3) {
    TelemetryPacket* packet = telemetry_pool.try_alloc();
    if (packet == nullptr) {
        osEvent event = telemetry_queue.get(0);
        if (event.status == osEventMessage) {
            TelemetryPacket* old_packet = static_cast<TelemetryPacket*>(event.value.p);
            telemetry_pool.free(old_packet);
            packet = telemetry_pool.try_alloc();
        }
    }

    if (packet == nullptr) {
        Serial.println("Telemetry queue full, dropping sample.");
        return 0;
    }

    packet->air_hum = airHum;
    packet->air_temp = airTemp;
    packet->soil_moist = soilMoist;
    packet->soil_temp = soilTemp;
    packet->soil_cond = soilCond;
    packet->soil_ph = soilPh;
    packet->soil_n = soilN;
    packet->soil_p = soilP;
    packet->soil_k = soilK;
    packet->soil_sal = soilSal;
    packet->soil_tds = soilTds;
    packet->din_00 = din0;
    packet->din_01 = din1;
    packet->din_02 = din2;
    packet->din_03 = din3;

    if (telemetry_queue.put(packet, 0) != osOK) {
        telemetry_pool.free(packet);
        Serial.println("Telemetry enqueue failed.");
        return 0;
    }

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
    mqttClient.setConnectionTimeout(5000);
    attemptNetworkConnection();
    attemptMqttConnection();
}

void process() {
    attemptNetworkConnection();
    attemptMqttConnection();

    if (mqttClient.connected()) {
        configureSubscriptions();
        mqttClient.poll();

        for (uint8_t i = 0; i < 4; ++i) {
            osEvent event = telemetry_queue.get(0);
            if (event.status != osEventMessage) {
                break;
            }

            TelemetryPacket* packet = static_cast<TelemetryPacket*>(event.value.p);
            if (!publishPacket(*packet)) {
                telemetry_pool.free(packet);
                break;
            }
            telemetry_pool.free(packet);
        }
    }
}

} // namespace task_mqtt
} // namespace module
} // namespace gms_edge
