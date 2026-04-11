#include <Arduino.h>

#include "ModbusRtuManager.h"
#include "AirSensorSth30.h"

namespace {

constexpr uint8_t kCurrentAirAddress = 2;
constexpr uint8_t kTargetAirAddress = 10;
constexpr int kScanStartAddress = 1;
constexpr int kScanEndAddress = 20;

bool probeAddress(gms_edge::ModbusRtuManager& modbus_manager, int address) {
    int16_t value = 0;
    return modbus_manager.read_multiple_registers(address, HOLDING_REGISTERS, 0x0000, 1, &value);
}

void scanBus(gms_edge::ModbusRtuManager& modbus_manager) {
    Serial.println("Scanning RS485 bus...");
    for (int address = kScanStartAddress; address <= kScanEndAddress; ++address) {
        if (probeAddress(modbus_manager, address)) {
            Serial.print("  Found device at address ");
            Serial.println(address);
        }
        delay(50);
    }
}

void haltForever() {
    while (true) {
        delay(1000);
    }
}

} // namespace

void setup() {
    Serial.begin(115200);

    uint32_t start = millis();
    while (!Serial && millis() - start < 5000) {
        delay(10);
    }

    Serial.println("\n\n--- STH30 Address Configuration Tool ---");
    Serial.println("For reliable address programming, connect ONLY the air sensor to RS485.");

    gms_edge::ModbusRtuManager modbus_manager;
    if (!modbus_manager.initialize()) {
        Serial.println("Modbus initialization failed.");
        haltForever();
    }

    scanBus(modbus_manager);

    gms_edge::AirSensorSth30 air_sensor(&modbus_manager, kCurrentAirAddress);
    if (!air_sensor.read_all_registers()) {
        Serial.print("Could not read STH30 at address ");
        Serial.println(kCurrentAirAddress);
        Serial.println("If soil sensor is also connected or addresses conflict, disconnect other nodes and retry.");
        haltForever();
    }

    Serial.print("Current STH30 address: ");
    Serial.println(air_sensor.get_device_address());

    if (!air_sensor.set_device_address(kTargetAirAddress)) {
        Serial.println("Failed to write new STH30 address.");
        haltForever();
    }

    delay(500);

    if (!probeAddress(modbus_manager, kTargetAirAddress)) {
        Serial.println("Address write sent, but target address did not respond.");
        Serial.println("Power-cycle the sensor and rerun this tool to verify.");
        haltForever();
    }

    gms_edge::AirSensorSth30 readdressed_sensor(&modbus_manager, kTargetAirAddress);
    if (!readdressed_sensor.read_all_registers()) {
        Serial.println("Target address responds, but full register read failed.");
        haltForever();
    }

    Serial.print("Success. Air sensor reprogrammed to address ");
    Serial.println(kTargetAirAddress);
    Serial.println("You can now reconnect the soil sensor at address 1 on the same RS485 bus.");
}

void loop() {
    delay(1000);
}
