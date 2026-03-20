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

#include "sth30.hpp"

namespace gms_controller
{

Sth30::Sth30(ModbusRtuController * modbus_rtu_controller, uint8_t device_address)
: device_address_(device_address) 
, modbus_rtu_controller_(modbus_rtu_controller)
{

}

Sth30::~Sth30()
{

}

bool Sth30::initialize()
{
    return true;
}

bool Sth30::read_all_registers()
{
    int16_t data[2] = {0};

    modbus_rtu_controller_->read_multiple_registers(device_address_, HOLDING_REGISTERS, HUMIDITY_CONTENT_REG_ADDR, 2, data);

    sensor_data_.humidity = data[0] / 100.0f;
    sensor_data_.temperature = data[1] / 100.0f;
    
    delay(100);

    modbus_rtu_controller_->read_multiple_registers(device_address_, HOLDING_REGISTERS, DEVICE_ADDRESS_CONTENT_REG_ADDR, 2, data);
    sensor_factory_data_.device_addr = static_cast<uint16_t>(data[0]);
    sensor_factory_data_.baud = static_cast<uint16_t>(data[1]);

    delay(100);

    modbus_rtu_controller_->read_multiple_registers(device_address_, HOLDING_REGISTERS, CORRECTION_CONTENT_REG_ADDR, 1, data);
    sensor_correction_data_.correction = data[0];

    return true;
}


float Sth30::get_temp()
{
    return sensor_data_.temperature;
}

float Sth30::get_humidity()
{
    return sensor_data_.humidity;
}

uint16_t Sth30::get_device_address()
{
    return sensor_factory_data_.device_addr;
}

uint16_t Sth30::get_device_baud()
{
    return sensor_factory_data_.baud;
}

uint16_t Sth30::get_correction()
{
    return sensor_correction_data_.correction;
}

} // namespace gms_controller