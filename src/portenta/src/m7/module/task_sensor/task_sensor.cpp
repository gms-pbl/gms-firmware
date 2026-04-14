#include "task_sensor.h"
#include "../task_mqtt/task_mqtt.h"
#include "../task_io/task_io.h"
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
        float air_hum = 0, air_temp = 0;
        float soil_moist = 0, soil_temp = 0, soil_cond = 0, soil_ph = 0;
        float soil_n = 0, soil_p = 0, soil_k = 0, soil_sal = 0, soil_tds = 0;
        int din_00 = 0, din_01 = 0, din_02 = 0, din_03 = 0;
        int dout_00 = 0, dout_01 = 0, dout_02 = 0, dout_03 = 0;
        int dout_04 = 0, dout_05 = 0, dout_06 = 0, dout_07 = 0;

        if (air_sensor.read_all_registers()) {
            air_hum = air_sensor.get_humidity();
            air_temp = air_sensor.get_temperature();
        }

        delay(100);

        if (soil_sensor.read_all_registers()) {
            soil_moist = soil_sensor.get_moisture();
            soil_temp = soil_sensor.get_temperature();
            soil_cond = soil_sensor.get_conductivity();
            soil_ph = soil_sensor.get_ph();
            soil_n = soil_sensor.get_nitrogen();
            soil_p = soil_sensor.get_phosphorus();
            soil_k = soil_sensor.get_potassium();
            soil_sal = soil_sensor.get_salinity();
            soil_tds = soil_sensor.get_total_dissolved_solids();
        }

        // Read Digital Inputs 00-03 from DIN Rail
        din_00 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_00);
        din_01 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_01);
        din_02 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_02);
        din_03 = MachineControl_DigitalInputs.read(DIN_READ_CH_PIN_03);

        module::task_io::get_output_states(
            dout_00,
            dout_01,
            dout_02,
            dout_03,
            dout_04,
            dout_05,
            dout_06,
            dout_07);

        // Send sensor + input + output states to MQTT publisher
        module::task_mqtt::publish_telemetry(
            air_hum, 
            air_temp, 
            soil_moist, 
            soil_temp,
            soil_cond,
            soil_ph,
            soil_n,
            soil_p,
            soil_k,
            soil_sal,
            soil_tds,
            din_00,
            din_01,
            din_02,
            din_03,
            dout_00,
            dout_01,
            dout_02,
            dout_03,
            dout_04,
            dout_05,
            dout_06,
            dout_07
        );

        ThisThread::sleep_for(std::chrono::milliseconds(SENSOR_POLL_INTERVAL_MS));
    }
}

void init() {
    Serial.println("  [Sensor Task] Initializing Modbus Manager (RS485)...");
    modbus_manager.initialize();

    Serial.println("  [Sensor Task] Initializing Wire (I2C)...");
    Wire.begin();
    Serial.println("  [Sensor Task] Wire (I2C) initialized.");

    Serial.println("  [Sensor Task] Initializing Digital Inputs...");
    MachineControl_DigitalInputs.begin();
    Serial.println("  [Sensor Task] Digital Inputs initialized.");
    
    Serial.println("  [Sensor Task] Initializing Air Sensor...");
    air_sensor.initialize();
    
    Serial.println("  [Sensor Task] Initializing Soil Sensor...");
    soil_sensor.initialize();

    Serial.println("  [Sensor Task] Starting thread...");
    sensorThread.start(mbed::callback(execute));
}

} // namespace task_sensor
} // namespace module
} // namespace gms_edge
