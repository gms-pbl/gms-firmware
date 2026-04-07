#include "task_sensor.h"
#include <Arduino_PortentaMachineControl.h>

namespace gms_edge {
namespace module {
namespace task_sensor {

using namespace rtos;

ModbusRtuManager modbus_manager;
AirSensorSth30 air_sensor(&modbus_manager, AIR_SENSOR_ADDRESS);
SoilSensorSn3002 soil_sensor(&modbus_manager, SOIL_SENSOR_ADDRESS);

Thread sensorThread(SENSOR_TASK_PRIORITY, SENSOR_TASK_STACK_SIZE);

void execute() {
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

        // Read Digital Inputs 00-03 from DIN Rail
        telemetry.din_00 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_00);
        telemetry.din_01 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_01);
        telemetry.din_02 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_02);
        telemetry.din_03 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_03);

        // Send all 15 values over IPC to M4
        RPC.call("publish_telemetry", 
            telemetry.air_humidity, 
            telemetry.air_temperature, 
            telemetry.soil_moisture, 
            telemetry.soil_temperature,
            telemetry.soil_conductivity,
            telemetry.soil_ph,
            telemetry.soil_nitrogen,
            telemetry.soil_phosphorus,
            telemetry.soil_potassium,
            telemetry.soil_salinity,
            telemetry.soil_tds,
            telemetry.din_00,
            telemetry.din_01,
            telemetry.din_02,
            telemetry.din_03
        ).as<int>();

        ThisThread::sleep_for(std::chrono::milliseconds(SENSOR_POLL_INTERVAL_MS));
    }
}

void init() {
    MachineControl_DigitalInputs.begin();
    modbus_manager.initialize();
    air_sensor.initialize();
    soil_sensor.initialize();

    sensorThread.start(mbed::callback(execute));
}

} // namespace task_sensor
} // namespace module
} // namespace gms_edge