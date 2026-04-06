#pragma once

#include "ModbusRtuManager.h"

namespace gms_edge {

class AirSensorSth30 {
public:
    struct SensorData {
        float humidity;
        float temperature;
    };

    struct SensorFactoryData {
        uint16_t device_addr;
        uint16_t baud;
    };
    
    struct SensorCorrectionData {
        uint16_t correction;
    };
    
    enum RegisterAddress {
        REG_HUMIDITY = 0x0000,
        REG_TEMPERATURE = 0x0001,
        REG_DEVICE_ADDRESS = 0x0066,
        REG_BAUDRATE = 0x0067,
        REG_CORRECTION = 0x006B,
    };

    explicit AirSensorSth30(ModbusRtuManager* modbus_manager, uint8_t device_address);
    ~AirSensorSth30();

    bool initialize();
    bool read_all_registers();

    float get_temperature() const;
    float get_humidity() const;
    uint16_t get_device_address() const;
    uint16_t get_device_baud() const;
    uint16_t get_correction() const;

private:
    uint8_t _device_address;
    ModbusRtuManager* _modbus_manager;

    SensorData _data;
    SensorFactoryData _factory_data;
    SensorCorrectionData _correction_data;
};

} // namespace gms_edge
