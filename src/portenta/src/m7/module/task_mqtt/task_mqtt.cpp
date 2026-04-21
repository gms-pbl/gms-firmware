#include "task_mqtt.h"
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
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
    int dout_00;
    int dout_01;
    int dout_02;
    int dout_03;
    int dout_04;
    int dout_05;
    int dout_06;
    int dout_07;
};

constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 5000;
constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 3000;
constexpr uint32_t ANNOUNCE_INTERVAL_MS = MQTT_ANNOUNCE_INTERVAL_MS;
constexpr uint8_t TELEMETRY_QUEUE_DEPTH = 16;

rtos::MemoryPool<TelemetryPacket, TELEMETRY_QUEUE_DEPTH> telemetry_pool;
rtos::Queue<TelemetryPacket, TELEMETRY_QUEUE_DEPTH> telemetry_queue;

unsigned long last_wifi_attempt_ms = 0;
unsigned long last_mqtt_attempt_ms = 0;
unsigned long last_announce_ms = 0;
bool mqtt_subscribed = false;

String device_id;
String zone_id_cache;
String zone_name_cache;

String topic_announce;
String topic_telemetry;
String topic_command;
String topic_config;

bool localBrokerPolicyWarned = false;

bool isPrivateOrLocalIp(const IPAddress& ip) {
    return
        ip[0] == 10 ||
        (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||
        (ip[0] == 192 && ip[1] == 168) ||
        (ip[0] == 169 && ip[1] == 254) ||
        ip[0] == 127;
}

bool isAllowedLocalBrokerHost(const char* host) {
    IPAddress parsed;
    if (parsed.fromString(host)) {
        return isPrivateOrLocalIp(parsed);
    }

    String normalized(host);
    normalized.trim();
    normalized.toLowerCase();

    if (normalized == "localhost") {
        return true;
    }

    return normalized.endsWith(".local");
}

uint16_t resolveMqttPort() {
    unsigned long parsed = strtoul(MQTT_PORT, nullptr, 10);
    if (parsed == 0 || parsed > 65535) {
        return 1883;
    }
    return static_cast<uint16_t>(parsed);
}

String jsonEscape(const String& input) {
    String escaped;
    escaped.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input.charAt(i);
        if (c == '\\' || c == '"') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

bool extractJsonStringField(const String& payload, const char* key, String& out, bool& is_null) {
    String key_token = String("\"") + key + "\"";
    int key_index = payload.indexOf(key_token);
    if (key_index < 0) {
        return false;
    }

    int colon_index = payload.indexOf(':', key_index + key_token.length());
    if (colon_index < 0) {
        return false;
    }

    int payload_length = payload.length();
    int value_index = colon_index + 1;
    while (value_index < payload_length &&
           (payload.charAt(value_index) == ' ' || payload.charAt(value_index) == '\t' ||
            payload.charAt(value_index) == '\r' || payload.charAt(value_index) == '\n')) {
        ++value_index;
    }

    if (value_index + 3 < payload_length && payload.substring(value_index, value_index + 4) == "null") {
        out = "";
        is_null = true;
        return true;
    }

    if (value_index >= payload_length || payload.charAt(value_index) != '"') {
        return false;
    }

    int value_end = payload.indexOf('"', value_index + 1);
    if (value_end < 0) {
        return false;
    }

    out = payload.substring(value_index + 1, value_end);
    is_null = false;
    return true;
}

void buildTopics() {
    String base = String("edge/") + GREENHOUSE_ID + "/zone/" + device_id;
    topic_announce = base + "/registry/announce";
    topic_telemetry = base + "/telemetry/raw";
    topic_command = base + "/command/output";
    topic_config = base + "/config";
}

String deviceIdFromMac() {
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);

    bool all_zero = true;
    for (uint8_t i = 0; i < 6; ++i) {
        if (mac[i] != 0) {
            all_zero = false;
            break;
        }
    }

    if (all_zero) {
        return String("");
    }

    char buffer[32];
    snprintf(
        buffer,
        sizeof(buffer),
        "portenta-%02x%02x%02x%02x%02x%02x",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
    return String(buffer);
}

bool ensureDeviceIdentity() {
    if (device_id.length() > 0) {
        return true;
    }

    if (std::strlen(DEVICE_ID) > 0) {
        device_id = String(DEVICE_ID);
    } else {
        device_id = deviceIdFromMac();
    }

    if (device_id.length() == 0) {
        Serial.println("Unable to derive device_id yet (waiting for valid MAC).");
        return false;
    }

    buildTopics();
    Serial.print("Resolved device_id: ");
    Serial.println(device_id);
    Serial.print("Greenhouse: ");
    Serial.println(GREENHOUSE_ID);
    return true;
}

bool publishAnnounce(bool force) {
    if (!mqttClient.connected() || topic_announce.length() == 0) {
        return false;
    }

    unsigned long now = millis();
    if (!force && (now - last_announce_ms) < ANNOUNCE_INTERVAL_MS) {
        return true;
    }

    String payload = String("{\"device_id\":\"") + jsonEscape(device_id) +
                     "\",\"firmware_version\":\"portenta-m7-v2\",\"metadata\":{\"label\":\"" +
                     jsonEscape(String(DEVICE_LABEL)) + "\"";

    if (zone_id_cache.length() > 0) {
        payload += String(",\"zone_id\":\"") + jsonEscape(zone_id_cache) + "\"";
    }
    if (zone_name_cache.length() > 0) {
        payload += String(",\"zone_name\":\"") + jsonEscape(zone_name_cache) + "\"";
    }

    payload += "}}";

    unsigned long payload_size = static_cast<unsigned long>(payload.length());
    if (mqttClient.beginMessage(topic_announce.c_str(), payload_size) == 0) {
        return false;
    }

    mqttClient.print(payload);
    if (mqttClient.endMessage() == 0) {
        return false;
    }

    last_announce_ms = now;
    Serial.print("Published announce: ");
    Serial.println(payload);
    return true;
}

bool parseCommandPayload(const String& payload, int& channel, int& state) {
    int chan_index = payload.indexOf("\"channel\":");
    int state_index = payload.indexOf("\"state\":");

    if (chan_index < 0 || state_index < 0) {
        return false;
    }

    int chan_end = payload.indexOf(',', chan_index);
    if (chan_end < 0) {
        chan_end = payload.indexOf('}', chan_index);
    }
    int state_end = payload.indexOf(',', state_index);
    if (state_end < 0) {
        state_end = payload.indexOf('}', state_index);
    }

    if (chan_end < 0 || state_end < 0) {
        return false;
    }

    channel = payload.substring(chan_index + 10, chan_end).toInt();
    state = payload.substring(state_index + 8, state_end).toInt();
    return true;
}

bool applyConfigPayload(const String& payload) {
    String parsed_zone_id;
    String parsed_zone_name;
    bool zone_id_null = false;
    bool zone_name_null = false;

    bool has_zone_id = extractJsonStringField(payload, "zone_id", parsed_zone_id, zone_id_null);
    bool has_zone_name = extractJsonStringField(payload, "zone_name", parsed_zone_name, zone_name_null);

    if (!has_zone_id && !has_zone_name) {
        Serial.println("Config payload missing zone fields.");
        return false;
    }

    String next_zone_id = zone_id_cache;
    String next_zone_name = zone_name_cache;

    if (has_zone_id) {
        next_zone_id = zone_id_null ? String("") : parsed_zone_id;
    }

    if (has_zone_name) {
        next_zone_name = zone_name_null ? String("") : parsed_zone_name;
    }

    if (next_zone_id == zone_id_cache && next_zone_name == zone_name_cache) {
        Serial.println("Config unchanged; skipping announce.");
        return false;
    }

    zone_id_cache = next_zone_id;
    zone_name_cache = next_zone_name;

    Serial.print("Applied config zone_id=");
    Serial.print(zone_id_cache.length() > 0 ? zone_id_cache : "<unassigned>");
    Serial.print(" zone_name=");
    Serial.println(zone_name_cache.length() > 0 ? zone_name_cache : "<unassigned>");
    return true;
}

void configureSubscriptions() {
    if (mqtt_subscribed || !mqttClient.connected()) {
        return;
    }

    mqttClient.onMessage(onMqttMessage);
    bool command_ok = mqttClient.subscribe(topic_command.c_str());
    bool config_ok = mqttClient.subscribe(topic_config.c_str());
    if (command_ok && config_ok) {
        mqtt_subscribed = true;
        Serial.print("Subscribed to: ");
        Serial.println(topic_command);
        Serial.print("Subscribed to: ");
        Serial.println(topic_config);
    } else {
        Serial.println("Failed to subscribe to command/config topics.");
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
    if (!ensureDeviceIdentity()) {
        return false;
    }

    if (!isAllowedLocalBrokerHost(MQTT_BROKER)) {
        if (!localBrokerPolicyWarned) {
            Serial.println("MQTT broker rejected by local-only policy.");
            Serial.println("Set MQTT_BROKER to local LAN IP (RFC1918) or .local hostname.");
            Serial.print("Configured MQTT_BROKER: ");
            Serial.println(MQTT_BROKER);
            localBrokerPolicyWarned = true;
        }
        return false;
    }
    localBrokerPolicyWarned = false;

    unsigned long now = millis();
    if (now - last_mqtt_attempt_ms < MQTT_RETRY_INTERVAL_MS) {
        return false;
    }
    last_mqtt_attempt_ms = now;

    Serial.print("Connecting to MQTT broker...");
    uint16_t brokerPort = resolveMqttPort();
    IPAddress brokerIP;
    bool broker_is_ip = brokerIP.fromString(MQTT_BROKER);

    bool connected = false;
    if (broker_is_ip) {
        connected = mqttClient.connect(brokerIP, brokerPort);
    } else {
        connected = mqttClient.connect(MQTT_BROKER, brokerPort);
    }

    if (!connected) {
        Serial.println(" failed.");
        return false;
    }

    Serial.println(" connected!");
    last_announce_ms = 0;
    mqtt_subscribed = false;
    configureSubscriptions();
    publishAnnounce(true);
    return true;
}

bool publishPacket(const TelemetryPacket& packet) {
    char payload[640];
    int written = snprintf(
        payload,
        sizeof(payload),
        "{\"air_temp\":%.2f,\"air_hum\":%.2f,\"soil_moist\":%.2f,\"soil_temp\":%.2f,\"soil_cond\":%.2f,\"soil_ph\":%.2f,\"soil_n\":%.2f,\"soil_p\":%.2f,\"soil_k\":%.2f,\"soil_salinity\":%.2f,\"soil_tds\":%.2f,\"din_00\":%d,\"din_01\":%d,\"din_02\":%d,\"din_03\":%d,\"dout_00\":%d,\"dout_01\":%d,\"dout_02\":%d,\"dout_03\":%d,\"dout_04\":%d,\"dout_05\":%d,\"dout_06\":%d,\"dout_07\":%d}",
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
        packet.din_03,
        packet.dout_00,
        packet.dout_01,
        packet.dout_02,
        packet.dout_03,
        packet.dout_04,
        packet.dout_05,
        packet.dout_06,
        packet.dout_07
    );

    if (written <= 0 || static_cast<size_t>(written) >= sizeof(payload)) {
        Serial.println("Telemetry payload serialization failed.");
        return false;
    }

    if (mqttClient.beginMessage(topic_telemetry.c_str(), static_cast<unsigned long>(written)) == 0) {
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

int publish_telemetry(
    float airHum,
    float airTemp,
    float soilMoist,
    float soilTemp,
    float soilCond,
    float soilPh,
    float soilN,
    float soilP,
    float soilK,
    float soilSal,
    float soilTds,
    int din0,
    int din1,
    int din2,
    int din3,
    int dout0,
    int dout1,
    int dout2,
    int dout3,
    int dout4,
    int dout5,
    int dout6,
    int dout7) {
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
    packet->dout_00 = dout0;
    packet->dout_01 = dout1;
    packet->dout_02 = dout2;
    packet->dout_03 = dout3;
    packet->dout_04 = dout4;
    packet->dout_05 = dout5;
    packet->dout_06 = dout6;
    packet->dout_07 = dout7;

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

    if (topic == topic_command) {
        int channel = -1;
        int state = 0;
        if (!parseCommandPayload(payload, channel, state)) {
            Serial.println("Invalid payload format for command.");
            return;
        }

        Serial.print("Executing task_io::set_output(");
        Serial.print(channel);
        Serial.print(", ");
        Serial.print(state);
        Serial.println(")");

        module::task_io::set_output(channel, state);
        return;
    }

    if (topic == topic_config) {
        if (applyConfigPayload(payload)) {
            publishAnnounce(true);
        }
        return;
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
        publishAnnounce(false);

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
