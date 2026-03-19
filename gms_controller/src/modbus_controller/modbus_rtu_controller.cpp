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

#include "modbus_rtu_controller.hpp"

namespace gms_controller
{
    
ModbusRtuController::ModbusRtuController()
{
    modbus_rtu_client_ = &ModbusRTUClient;
    rs_485_port_ = &MachineControl_RS485Comm;
}

ModbusRtuController::~ModbusRtuController()
{

}

bool ModbusRtuController::initilize()
{
    rs_485_port_->begin(4800, SERIAL_8N1, 0, 500);
    rs_485_port_->setYZTerm(false);
    rs_485_port_->setABTerm(false);
    rs_485_port_->setFullDuplex(false);
    rs_485_port_->receive();
    rs_485_port_->setDelays(1000, 5000);
    rs_485_port_->setModeRS232(false);

    if (!modbus_rtu_client_->begin(*rs_485_port_, 4800, SERIAL_8N1)) {
        Serial.println("Failed to start Modbus RTU Client!");
        return false;
    }
  
    Serial.println("Initialization done!");

    return true;
}

bool ModbusRtuController::read_single_register(int id, int type, int address)
{
    return false;
}

bool ModbusRtuController::read_multiple_registers(int id, int type, int address, int nb, int16_t * data)
{
    int index = 0UL;

    if (!modbus_rtu_client_->requestFrom(id, type, address, nb)) {
        Serial.print("Failed to read registers! ");
        Serial.println(modbus_rtu_client_->lastError());
        return false;
    } else {
        while (modbus_rtu_client_->available()) {
            data[index] = modbus_rtu_client_->read();
            index++;
            if (index >= nb) break;
        }
    }

    return true;
}

} // namespace gms_cotroller    