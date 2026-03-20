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

#ifndef MODBUS_CONTROLLER_SOIL_SENSOR_HPP_
#define MODBUS_CONTROLLER_SOIL_SENSOR_HPP_

#include <stdio.h>

#include "modbus_rtu_controller.hpp"

namespace gms_controller 
{

class Sn3002
{

public:

    typedef struct SensorData_ {
        float moisture;
        float temperature;
        float conductivity;
        float ph;
        float nitrogen;
        float phosphorus;
        float potasium;
        float salinity;
        float total_disolved_solid;
    } SensorData;

    enum Sn3002_ReadRegisterAddress {
        MOISTURE_CONTENT_REG_ADDR               = 0x0000U,
        TEMPERATURE_CONTENT_REG_ADDR            = 0x0001U,
        CONDUCTIVITY_CONTENT_REG_ADDR           = 0x0002U,
        PH_CONTENT_REG_ADDR                     = 0x0003U,
        NITROGEN_CONTENT_REG_ADDR               = 0x0004U,
        PHOSPHORUS_CONTENT_REG_ADDR             = 0x0005U,
        POTASSIUM_CONTENT_REG_ADDR              = 0x0006U,
        SALINITY_CONTENT_REG_ADDR               = 0x0007U,
        TOTAL_DISSOLVED_SOLIDS_CONTENT_REG_ADDR = 0x0008U,
    };

    explicit Sn3002(ModbusRtuController * modbus_rtu_controller, uint8_t device_address);

    ~Sn3002();

    bool initialize();

    bool read_all_registers();

    float get_moisture();

    float get_temp();

    float get_conductivity();

    float get_ph();

    float get_nitrogen();

    float get_phosphorus();

    float get_potasium();
    
    float get_salinity();
    
    float get_total_disolved_solid();
    
private:

    uint8_t device_address_;

    ModbusRtuController * modbus_rtu_controller_;

    SensorData sensor_data_{0.0f, 0.0f};
};

} // namespace gms_controller 

#endif // MODBUS_CONTROLLER_SOIL_SENSOR_HPP_