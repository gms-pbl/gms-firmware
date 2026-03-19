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

#include "sn_3002.hpp"

namespace gms_controller
{

Sn3002::Sn3002(ModbusRtuController * modbus_rtu_controller)
: modbus_rtu_controller_(modbus_rtu_controller)
{

}

Sn3002::~Sn3002()
{

}

bool Sn3002::initialize()
{
    return true;
}

bool Sn3002::read_all_registers()
{
    int16_t data[9] = {0};

    modbus_rtu_controller_->read_multiple_registers(sensor_address_, HOLDING_REGISTERS, MOISTURE_CONTENT_REG_ADDR, 8, data);

    sensor_data_.moisture               = static_cast<float>(data[0]) / 10.0f;
    sensor_data_.temperature            = static_cast<float>(data[1]) / 10.0f;
    sensor_data_.conductivity           = static_cast<float>(data[2]);
    sensor_data_.ph                     = static_cast<float>(data[3]) / 10.0f;
    sensor_data_.nitrogen               = static_cast<float>(data[4]);
    sensor_data_.phosphorus             = static_cast<float>(data[5]);
    sensor_data_.potasium               = static_cast<float>(data[6]);
    sensor_data_.salinity               = static_cast<float>(data[7]);
    sensor_data_.total_disolved_solid   = static_cast<float>(data[8]);

    return false;
}

float Sn3002::get_moisture()
{
    return sensor_data_.moisture;
}

float Sn3002::get_temp()
{
    return sensor_data_.temperature;
}

float Sn3002::get_conductivity()
{
    return sensor_data_.conductivity;
}

float Sn3002::get_ph()
{
    return sensor_data_.ph;
}

float Sn3002::get_nitrogen()
{
    return sensor_data_.nitrogen;
}

float Sn3002::get_phosphorus()
{
    return sensor_data_.phosphorus;
}

float Sn3002::get_potasium()
{
    return sensor_data_.potasium;
}

float Sn3002::get_salinity()
{
    return sensor_data_.salinity;
}

float Sn3002::get_total_disolved_solid()
{
    return sensor_data_.total_disolved_solid;
}

} // gms_controller