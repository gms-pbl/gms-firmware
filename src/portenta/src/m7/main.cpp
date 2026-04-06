#include <Arduino.h>
#include <RPC.h>
#include <mbed.h>
#include <rtos.h>

#include "ModbusRtuManager.h"
#include "AirSensorSth30.h"
#include "SoilSensorSn3002.h"
#include "TelemetryIpc.h"

using namespace rtos;
using namespace gms_edge;

ModbusRtuManager modbus_manager;
AirSensorSth30 air_sensor(&modbus_manager, 10);
SoilSensorSn3002 soil_sensor(&modbus_manager, 1);

Thread sensorThread(osPriorityNormal, 2048);

void sensorTask() {
    for (;;) {
        TelemetryIpc telemetry = {0};

        if (air_sensor.read_all_registers()) {
            telemetry.air_humidity = air_sensor.get_humidity();
            telemetry.air_temperature = air_sensor.get_temperature();
        }

        delay(100);

        if (soil_sensor.read_all_registers()) {
            telemetry.soil_moisture = soil_sensor.get_moisture();
            telemetry.soil_temperature = soil_sensor.get_temperature();
            telemetry.soil_conductivity = soil_sensor.get_conductivity();
            telemetry.soil_ph = soil_sensor.get_ph();
            telemetry.soil_nitrogen = soil_sensor.get_nitrogen();
            telemetry.soil_phosphorus = soil_sensor.get_phosphorus();
            telemetry.soil_potassium = soil_sensor.get_potassium();
            telemetry.soil_salinity = soil_sensor.get_salinity();
            telemetry.soil_tds = soil_sensor.get_total_dissolved_solids();
        }

        // Send struct over IPC to M4
        RPC.call("publish_telemetry", (uint8_t*)&telemetry, sizeof(TelemetryIpc));

        ThisThread::sleep_for(std::chrono::milliseconds(10000)); // Poll every 10 seconds
    }
}

void setup() {
    Serial.begin(115200);
    RPC.begin();

    // Initialize sensors
    modbus_manager.initialize();
    air_sensor.initialize();
    soil_sensor.initialize();

    sensorThread.start(mbed::callback(sensorTask));
}

void loop() {
    // M7 runs RTOS threads, loop can just wait
    delay(1000);
}
