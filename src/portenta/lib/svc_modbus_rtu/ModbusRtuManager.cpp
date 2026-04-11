#include "ModbusRtuManager.h"

namespace gms_edge {
    
ModbusRtuManager::ModbusRtuManager() {
    _modbus_client = &ModbusRTUClient;
    _rs485_port = &MachineControl_RS485Comm;
}

ModbusRtuManager::~ModbusRtuManager() {}

bool ModbusRtuManager::initialize() {
    _rs485_port->begin(4800, SERIAL_8N1, 0, 500);
    _rs485_port->setYZTerm(false);
    _rs485_port->setABTerm(false);
    _rs485_port->setFullDuplex(false);
    _rs485_port->receive();
    _rs485_port->setDelays(1000, 5000);
    _rs485_port->setModeRS232(false);

    if (!_modbus_client->begin(*_rs485_port, 4800, SERIAL_8N1)) {
        Serial.println("Failed to start Modbus RTU Client!");
        return false;
    }
  
    Serial.println("Modbus RTU initialized successfully.");
    return true;
}

bool ModbusRtuManager::read_multiple_registers(int id, int type, int address, int nb, int16_t* data) {
    if (!_modbus_client->requestFrom(id, type, address, nb)) {
        Serial.print("Failed to read registers: ");
        Serial.println(_modbus_client->lastError());
        return false;
    } 

    int index = 0;
    while (_modbus_client->available() && index < nb) {
        data[index++] = _modbus_client->read();
    }

    if (index != nb) {
        Serial.print("Incomplete Modbus response. Expected ");
        Serial.print(nb);
        Serial.print(" registers, got ");
        Serial.println(index);
        return false;
    }

    return true;
}

bool ModbusRtuManager::write_single_register(int id, int address, uint16_t value) {
    if (!_modbus_client->holdingRegisterWrite(id, address, value)) {
        Serial.print("Failed to write register: ");
        Serial.println(_modbus_client->lastError());
        return false;
    }

    return true;
}

} // namespace gms_edge
