/*
 * MIT License
 * 
 * Copyright (c) 2026 Potlog Radu
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <chrono>
#include <Arduino.h>

#include <mbed.h>
#include <rtos.h>

#include "sth30.hpp"
#include "sn_3002.hpp"

#include "modbus_tcp_controller.hpp"

using namespace rtos;

gms_controller::ModbusRtuController rtu_contorller;
gms_controller::Sth30 air_sensor(&rtu_contorller, 10);
gms_controller::Sn3002 soil_sensor(&rtu_contorller, 1);
gms_controller::ModbusTcpController modbus_tcp_controller(&Ethernet);

using namespace rtos;

rtos::Thread sensorThread(osPriorityNormal, 2048);
rtos::Thread comThread(osPriorityHigh, 2048);

void sensorTask() {

    for(;;) {
        air_sensor.read_all_registers();
        Serial.println("--------------- Air sensor -------------------------");
        Serial.print("Humidity: ");
        Serial.print(air_sensor.get_humidity());
        Serial.print("\n");
        Serial.print("Temperature: ");
        Serial.print(air_sensor.get_temp());
        Serial.print("\n");
        Serial.print("Correction: ");
        Serial.print(air_sensor.get_correction());
        Serial.print("\n");
        Serial.print("Device address: ");
        Serial.print(air_sensor.get_device_address());
        Serial.print("\n");
        Serial.print("Device baud: ");
        Serial.println(air_sensor.get_device_baud());
        
        delay(100);
        
        soil_sensor.read_all_registers();
        Serial.println("--------------- Soil sensor -----------------------");
        Serial.print("Humidity: ");
        Serial.print(soil_sensor.get_moisture());
        Serial.print("\n");
        Serial.print("Temperature: ");
        Serial.print(soil_sensor.get_temp());
        Serial.print("\n");
        Serial.print("Conductivity: ");
        Serial.print(soil_sensor.get_conductivity());
        Serial.print("\n");
        Serial.print("PH: ");
        Serial.print(soil_sensor.get_ph());
        Serial.print("\n");
        Serial.print("Nitrogen: ");
        Serial.print(soil_sensor.get_nitrogen());
        Serial.print("\n");
        Serial.print("Phosphorus: ");
        Serial.print(soil_sensor.get_phosphorus());
        Serial.print("\n");
        Serial.print("Potasium: ");
        Serial.print(soil_sensor.get_potasium());
        Serial.print("\n");
        Serial.print("Salinity: ");
        Serial.print(soil_sensor.get_salinity());
        Serial.print("\n");
        Serial.print("Total disolved solid: ");
        Serial.println(soil_sensor.get_total_disolved_solid());
        Serial.print("\n");

        float buff[11];
        
        buff[0]  = air_sensor.get_humidity();
        buff[1]  = air_sensor.get_temp();
        buff[2]  = soil_sensor.get_moisture();
        buff[3]  = soil_sensor.get_temp();
        buff[4]  = soil_sensor.get_conductivity();
        buff[5]  = soil_sensor.get_ph();
        buff[6]  = soil_sensor.get_nitrogen();
        buff[7]  = soil_sensor.get_phosphorus();
        buff[8]  = soil_sensor.get_potasium();
        buff[9]  = soil_sensor.get_salinity();
        buff[10] = soil_sensor.get_total_disolved_solid();
        
        modbus_tcp_controller.write_holding_registers(buff, 11);

        ThisThread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void comTask() {

    for(;;) {
        modbus_tcp_controller.loop();
        ThisThread::sleep_for(std::chrono::milliseconds(50));
    }
}

void setup() 
{
    rtu_contorller.initilize();
    modbus_tcp_controller.initilize();
    air_sensor.initialize();
    soil_sensor.initialize();

    comThread.start(mbed::callback(comTask));
    sensorThread.start(mbed::callback(sensorTask));
}

void loop() 
{
    delay(1000);
}