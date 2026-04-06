#pragma once

#ifndef M7_MAIN_H
#define M7_MAIN_H

#include <Arduino.h>
#include <RPC.h>
#include <mbed.h>
#include <rtos.h>

#include "ModbusRtuManager.h"
#include "AirSensorSth30.h"
#include "SoilSensorSn3002.h"
#include "TelemetryIpc.h"

// Modbus Settings
#define AIR_SENSOR_ADDRESS 10
#define SOIL_SENSOR_ADDRESS 1

// Polling Settings
#define SENSOR_POLL_INTERVAL_MS 10000

// Thread Priorities
#define SENSOR_TASK_PRIORITY osPriorityNormal
#define SENSOR_TASK_STACK_SIZE 2048

#endif // M7_MAIN_H