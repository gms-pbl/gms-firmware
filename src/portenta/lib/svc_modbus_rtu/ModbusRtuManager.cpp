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

    delay(10); // Give the sensor time to push data onto the RS485 bus

    int index = 0;
    while (_modbus_client->available() && index < nb) {
        data[index++] = _modbus_client->read();
    }
    return true;
}

} // namespace gms_edge
