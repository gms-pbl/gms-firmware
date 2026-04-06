#pragma once

#ifndef M4_MAIN_H
#define M4_MAIN_H

#include <Arduino.h>
#include <RPC.h>
#include <ArduinoMqttClient.h>
#include <WiFi.h>

#include "TelemetryIpc.h"

// Networking Settings
#define USE_WIFI 1
#define WIFI_SSID "your_network_ssid"
#define WIFI_PASS "your_network_password"

// MQTT Settings
#define MQTT_BROKER "192.168.1.50"
#define MQTT_PORT 1883
#define MQTT_TOPIC_TELEMETRY "telemetry/zone1"

#endif // M4_MAIN_H