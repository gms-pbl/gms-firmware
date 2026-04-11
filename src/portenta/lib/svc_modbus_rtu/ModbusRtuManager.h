#pragma once

#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include <Arduino_PortentaMachineControl.h>

namespace gms_edge {

class ModbusRtuManager {
public:
    ModbusRtuManager();
    ~ModbusRtuManager();

    bool initialize();
    bool read_multiple_registers(int id, int type, int address, int nb, int16_t* data);
    bool write_single_register(int id, int address, uint16_t value);

private:
    ModbusRTUClientClass* _modbus_client;
    RS485CommClass* _rs485_port;
};

} // namespace gms_edge
