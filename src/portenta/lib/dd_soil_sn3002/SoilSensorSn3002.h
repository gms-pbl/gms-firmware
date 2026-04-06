#pragma once

#include "ModbusRtuManager.h"

namespace gms_edge {

class SoilSensorSn3002 {
public:
    struct SensorData {
        float moisture;
        float temperature;
        float conductivity;
        float ph;
        float nitrogen;
        float phosphorus;
        float potassium;
        float salinity;
        float total_dissolved_solids;
    };

    enum RegisterAddress {
        REG_MOISTURE = 0x0000,
        REG_TEMPERATURE = 0x0001,
        REG_CONDUCTIVITY = 0x0002,
        REG_PH = 0x0003,
        REG_NITROGEN = 0x0004,
        REG_PHOSPHORUS = 0x0005,
        REG_POTASSIUM = 0x0006,
        REG_SALINITY = 0x0007,
        REG_TDS = 0x0008,
    };

    explicit SoilSensorSn3002(ModbusRtuManager* modbus_manager, uint8_t device_address);
    ~SoilSensorSn3002();

    bool initialize();
    bool read_all_registers();

    float get_moisture() const;
    float get_temperature() const;
    float get_conductivity() const;
    float get_ph() const;
    float get_nitrogen() const;
    float get_phosphorus() const;
    float get_potassium() const;
    float get_salinity() const;
    float get_total_dissolved_solids() const;

private:
    uint8_t _device_address;
    ModbusRtuManager* _modbus_manager;
    SensorData _data;
};

} // namespace gms_edge
