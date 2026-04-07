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

#ifndef WIFI_SSID
  #error "WIFI_SSID is not defined. Check your .env file."
#endif
#ifndef WIFI_PASS
  #error "WIFI_PASS is not defined. Check your .env file."
#endif
#ifndef MQTT_BROKER
  #error "MQTT_BROKER is not defined. Check your .env file."
#endif

// MQTT Settings
#define MQTT_PORT 1883
#define MQTT_TOPIC_TELEMETRY "telemetry/zone1"
#define MQTT_TOPIC_COMMAND_OUTPUT "commands/zone1/output"

#endif // M4_MAIN_H