#include "AirSensorSth30.h"

namespace gms_edge {

AirSensorSth30::AirSensorSth30(ModbusRtuManager* modbus_manager, uint8_t device_address)
    : _device_address(device_address), _modbus_manager(modbus_manager), _data{0}, _factory_data{0}, _correction_data{0} {}

AirSensorSth30::~AirSensorSth30() {}

bool AirSensorSth30::initialize() {
    return true;
}

bool AirSensorSth30::read_all_registers() {
    int16_t raw_data[2] = {0};

    if (!_modbus_manager->read_multiple_registers(_device_address, HOLDING_REGISTERS, REG_HUMIDITY, 2, raw_data)) {
        return false;
    }

    _data.humidity = raw_data[0] / 100.0f;
    _data.temperature = raw_data[1] / 100.0f;
    
    delay(100);

    if (_modbus_manager->read_multiple_registers(_device_address, HOLDING_REGISTERS, REG_DEVICE_ADDRESS, 2, raw_data)) {
        _factory_data.device_addr = static_cast<uint16_t>(raw_data[0]);
        _factory_data.baud = static_cast<uint16_t>(raw_data[1]);
    }

    delay(100);

    if (_modbus_manager->read_multiple_registers(_device_address, HOLDING_REGISTERS, REG_CORRECTION, 1, raw_data)) {
        _correction_data.correction = raw_data[0];
    }

    return true;
}

float AirSensorSth30::get_temperature() const { return _data.temperature; }
float AirSensorSth30::get_humidity() const { return _data.humidity; }
uint16_t AirSensorSth30::get_device_address() const { return _factory_data.device_addr; }
uint16_t AirSensorSth30::get_device_baud() const { return _factory_data.baud; }
uint16_t AirSensorSth30::get_correction() const { return _correction_data.correction; }

} // namespace gms_edge
