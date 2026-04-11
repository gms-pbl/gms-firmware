#include "SoilSensorSn3002.h"

namespace gms_edge {

SoilSensorSn3002::SoilSensorSn3002(ModbusRtuManager* modbus_manager, uint8_t device_address)
    : _device_address(device_address), _modbus_manager(modbus_manager), _data{0} {}

SoilSensorSn3002::~SoilSensorSn3002() {}

bool SoilSensorSn3002::initialize() {
    return true;
}

bool SoilSensorSn3002::read_all_registers() {
    int16_t raw_data[9] = {0};

    bool success = _modbus_manager->read_multiple_registers(_device_address, HOLDING_REGISTERS, REG_MOISTURE, 9, raw_data);

    _data.moisture               = static_cast<float>(raw_data[0]) / 10.0f;
    _data.temperature            = static_cast<float>(raw_data[1]) / 10.0f;
    _data.conductivity           = static_cast<float>(raw_data[2]);
    _data.ph                     = static_cast<float>(raw_data[3]) / 10.0f;
    _data.nitrogen               = static_cast<float>(raw_data[4]);
    _data.phosphorus             = static_cast<float>(raw_data[5]);
    _data.potassium              = static_cast<float>(raw_data[6]);
    _data.salinity               = static_cast<float>(raw_data[7]);
    _data.total_dissolved_solids = static_cast<float>(raw_data[8]);

    return success;
}

bool SoilSensorSn3002::set_device_address(uint8_t new_address) {
    if (!_modbus_manager->write_single_register(_device_address, REG_DEVICE_ADDRESS, new_address)) {
        return false;
    }

    _device_address = new_address;
    return true;
}

bool SoilSensorSn3002::set_device_baud_rate_code(uint16_t baud_rate_code) {
    return _modbus_manager->write_single_register(_device_address, REG_DEVICE_BAUD_RATE, baud_rate_code);
}

float SoilSensorSn3002::get_moisture() const { return _data.moisture; }
float SoilSensorSn3002::get_temperature() const { return _data.temperature; }
float SoilSensorSn3002::get_conductivity() const { return _data.conductivity; }
float SoilSensorSn3002::get_ph() const { return _data.ph; }
float SoilSensorSn3002::get_nitrogen() const { return _data.nitrogen; }
float SoilSensorSn3002::get_phosphorus() const { return _data.phosphorus; }
float SoilSensorSn3002::get_potassium() const { return _data.potassium; }
float SoilSensorSn3002::get_salinity() const { return _data.salinity; }
float SoilSensorSn3002::get_total_dissolved_solids() const { return _data.total_dissolved_solids; }

} // namespace gms_edge
