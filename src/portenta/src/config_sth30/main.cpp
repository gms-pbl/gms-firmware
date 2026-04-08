#include <Arduino.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include <Arduino_PortentaMachineControl.h>

#define CURRENT_ADDRESS 1
#define NEW_ADDRESS 10

void setup() {
    Serial.begin(115200);
    delay(3000); // Wait for Serial to open

    Serial.println("\n\n--- Air Sensor Reprogramming Tool ---");
    Serial.println("WARNING: Make sure ONLY the Air Sensor is connected to the RS485 bus!");
    
    Serial.println("Initializing RS485 Port...");
    MachineControl_RS485Comm.begin(4800, SERIAL_8N1, 0, 500);
    MachineControl_RS485Comm.setYZTerm(false);
    MachineControl_RS485Comm.setABTerm(false);
    MachineControl_RS485Comm.setFullDuplex(false);
    MachineControl_RS485Comm.receive();
    MachineControl_RS485Comm.setDelays(1000, 5000);
    MachineControl_RS485Comm.setModeRS232(false);

    Serial.println("Starting Modbus RTU Client...");
    if (!ModbusRTUClient.begin(MachineControl_RS485Comm, 4800, SERIAL_8N1)) {
        Serial.println("FAILED to start Modbus RTU Client!");
        while (1) delay(100);
    }
    Serial.println("Modbus Client Started successfully.\n");
    
    Serial.print("Attempting to change sensor address from ");
    Serial.print(CURRENT_ADDRESS);
    Serial.print(" to ");
    Serial.println(NEW_ADDRESS);

    // Register 0x0066 (102) is the device address register for STH30
    Serial.println("Sending Modbus write command...");
    if (!ModbusRTUClient.holdingRegisterWrite(CURRENT_ADDRESS, 0x0066, NEW_ADDRESS)) {
        Serial.print("Failed to write new address. Modbus Error: ");
        Serial.println(ModbusRTUClient.lastError());
        Serial.println("Is the sensor connected and currently at Address 1?");
    } else {
        Serial.println("Write successful! The sensor should now be at Address 10.");
        Serial.println("Please wait 2 seconds for the sensor to reboot...\n");
        delay(2000);

        Serial.println("Verifying new address by reading Temperature/Humidity at Address 10...");
        if (!ModbusRTUClient.requestFrom(NEW_ADDRESS, HOLDING_REGISTERS, 0x0000, 2)) {
            Serial.print("Verification failed. Modbus Error: ");
            Serial.println(ModbusRTUClient.lastError());
        } else {
            Serial.println("!!! VERIFICATION SUCCESS !!!");
            int16_t humidity_raw = ModbusRTUClient.read();
            int16_t temperature_raw = ModbusRTUClient.read();
            
            Serial.print("   -> Air Temp: ");
            Serial.print(temperature_raw / 100.0f);
            Serial.println(" C");
            
            Serial.print("   -> Air Hum: ");
            Serial.print(humidity_raw / 100.0f);
            Serial.println(" %");
            
            Serial.println("\nYou can now plug the Soil Sensor back in and flash the main GMS firmware!");
        }
    }
}

void loop() {
    delay(1000);
}
