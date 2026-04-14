#pragma once

#ifndef M7_MAIN_H
#define M7_MAIN_H

#include <Arduino.h>
#include <mbed.h>
#include <rtos.h>

#include "ModbusRtuManager.h"
#include "AirSensorSth30.h"
#include "SoilSensorSn3002.h"

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

#ifndef GREENHOUSE_ID
  #define GREENHOUSE_ID "greenhouse-demo"
#endif

#ifndef DEVICE_ID
  #define DEVICE_ID ""
#endif

#ifndef DEVICE_LABEL
  #define DEVICE_LABEL "Portenta H7"
#endif

// MQTT Settings
#define MQTT_PORT 1883
#define MQTT_ANNOUNCE_INTERVAL_MS 30000

// Modbus Settings
#define AIR_SENSOR_ADDRESS 10
#define SOIL_SENSOR_ADDRESS 1

// Polling Settings
#define SENSOR_POLL_INTERVAL_MS 10000

// Thread Priorities
#define SENSOR_TASK_PRIORITY osPriorityNormal
#define SENSOR_TASK_STACK_SIZE 2048
#define COM_TASK_PRIORITY osPriorityHigh
#define COM_TASK_STACK_SIZE 4096

#endif // M7_MAIN_H
