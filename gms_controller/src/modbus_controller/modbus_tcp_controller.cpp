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

#include "modbus_tcp_controller.hpp"

#include <Wire.h>

namespace gms_controller
{

IPAddress ip(192, 168, 99, 111);

ModbusTcpController::ModbusTcpController(arduino::EthernetClass * ethernet)
: ethernet_(ethernet)
{

}
    
ModbusTcpController::~ModbusTcpController()
{
    ethernet_ = nullptr;
}

bool ModbusTcpController::initilize()
{
    ethernet_->begin(NULL, ip);

    if (ethernet_->hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet interface not found!");
        return false;
    }

    if (ethernet_->linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable not connected!");
    }

    eth_server_.begin(502);
    
    if (!modbus_tcp_server_.begin(1)) {
        Serial.println("Failed to start Modbus TCP Server!");
        return false;
    }

    Wire.begin(); 

    if (!MachineControl_DigitalInputs.begin()) {
        Serial.println("Digital input GPIO expander initialization fail!!");
    }

    MachineControl_DigitalOutputs.begin(true);
    MachineControl_DigitalOutputs.writeAll(0);
    
    if(!modbus_tcp_server_.configureDiscreteInputs(0x00, 8)) {
        Serial.println("Failed to configure discrete inputs");
        return false;
    }

    if(!modbus_tcp_server_.configureCoils(0x00, 8)) {
        Serial.println("Failed to configure coils");
        return false;
    }
    
    if(!modbus_tcp_server_.configureHoldingRegisters(0x00, 22)) {
        Serial.println("Failed to configure holding registers");
        return false;
    }

    return true;
}

void ModbusTcpController::loop() 
{
    if (!client_.connected()) {
        client_.stop();
        client_ = eth_server_.accept();
        modbus_tcp_server_.accept(client_);
    }

    if (client_.connected()) {
        modbus_tcp_server_.poll();
        update_coils();
        update_discrete_inputs();
        update_holding_registers();
    }
}

bool ModbusTcpController::write_holding_registers(float * data, uint8_t len)
{
    Register_u32 temp;

    temp.f32 = data[0];
    holding_registers_[0] = temp.u16_arr[0];
    holding_registers_[1] = temp.u16_arr[1];
    
    temp.f32 = data[1];
    holding_registers_[2] = temp.u16_arr[0];
    holding_registers_[3] = temp.u16_arr[1];
    
    temp.f32 = data[2];
    holding_registers_[4] = temp.u16_arr[0];
    holding_registers_[5] = temp.u16_arr[1];
    
    temp.f32 = data[3];
    holding_registers_[6] = temp.u16_arr[0];
    holding_registers_[7] = temp.u16_arr[1];
    
    temp.f32 = data[4];
    holding_registers_[8] = temp.u16_arr[0];
    holding_registers_[9] = temp.u16_arr[1];

    temp.f32 = data[5];
    holding_registers_[10] = temp.u16_arr[0];
    holding_registers_[11] = temp.u16_arr[1];

    temp.f32 = data[6];
    holding_registers_[12] = temp.u16_arr[0];
    holding_registers_[13] = temp.u16_arr[1];

    temp.f32 = data[7];
    holding_registers_[14] = temp.u16_arr[0];
    holding_registers_[15] = temp.u16_arr[1];

    temp.f32 = data[8];
    holding_registers_[16] = temp.u16_arr[0];
    holding_registers_[17] = temp.u16_arr[1];

    temp.f32 = data[9];
    holding_registers_[18] = temp.u16_arr[0];
    holding_registers_[19] = temp.u16_arr[1];

    temp.f32 = data[10];
    holding_registers_[20] = temp.u16_arr[0];
    holding_registers_[21] = temp.u16_arr[1];

    return true;
}

} // namespace gms_controller