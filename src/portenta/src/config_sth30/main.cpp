#include <Arduino.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include <Arduino_PortentaMachineControl.h>

#define DEFAULT_ADDRESS 1
#define NEW_ADDRESS 10

#define DEFAULT_BAUD 9600
#define NEW_BAUD_CODE 3 // 3 = 4800 baud according to STH30 datasheet

void setup() {
    Serial.begin(115200);
    delay(3000); // Wait for Serial to open

    Serial.println("\n\n--- Air Sensor STH30 Reprogramming Tool ---");
    Serial.println("WARNING: Make sure ONLY the STH30 Air Sensor is connected to the RS485 bus!");
    
    // ---------------------------------------------------------
    // STEP 1: Connect at Factory Default Settings (9600 Baud)
    // ---------------------------------------------------------
    Serial.println("\n[STEP 1] Initializing RS485 Port at 9600 Baud (Factory Default)...");
    MachineControl_RS485Comm.begin(DEFAULT_BAUD, SERIAL_8N1, 0, 500);
    MachineControl_RS485Comm.setYZTerm(false);
    MachineControl_RS485Comm.setABTerm(false);
    MachineControl_RS485Comm.setFullDuplex(false);
    MachineControl_RS485Comm.receive();
    MachineControl_RS485Comm.setDelays(1000, 5000);
    MachineControl_RS485Comm.setModeRS232(false);

    if (!ModbusRTUClient.begin(MachineControl_RS485Comm, DEFAULT_BAUD, SERIAL_8N1)) {
        Serial.println("FAILED to start Modbus RTU Client!");
        while (1) delay(100);
    }
    Serial.println("Connected to Modbus at 9600 baud.");

    // ---------------------------------------------------------
    // STEP 2: Change Device Address to 10
    // ---------------------------------------------------------
    Serial.println("\n[STEP 2] Changing Device Address from 1 to 10...");
    // Register 0x0100 is the Device Address register for STH30
    if (!ModbusRTUClient.holdingRegisterWrite(DEFAULT_ADDRESS, 0x0100, NEW_ADDRESS)) {
        Serial.print("Failed to write new address. Modbus Error: ");
        Serial.println(ModbusRTUClient.lastError());
        Serial.println("Is the sensor already at Address 10, or wired incorrectly?");
    } else {
        Serial.println("SUCCESS! Address changed to 10.");
    }
    delay(500);

    // ---------------------------------------------------------
    // STEP 3: Change Baud Rate to 4800
    // ---------------------------------------------------------
    Serial.println("\n[STEP 3] Changing Baud Rate from 9600 to 4800...");
    // Register 0x0101 is the Baud Rate register. Value 3 = 4800 baud.
    // Note: We must send this command to the NEW address (10) because we just changed it!
    if (!ModbusRTUClient.holdingRegisterWrite(NEW_ADDRESS, 0x0101, NEW_BAUD_CODE)) {
        Serial.print("Failed to write new baud rate. Modbus Error: ");
        Serial.println(ModbusRTUClient.lastError());
    } else {
        Serial.println("SUCCESS! Baud rate changed to 4800.");
    }

    Serial.println("\n[ACTION REQUIRED] Please POWER CYCLE the sensor to apply changes.");
    Serial.println("Waiting 10 seconds for you to unplug and re-plug the sensor's 24V power...");
    delay(10000);

    // ---------------------------------------------------------
    // STEP 4: Reconnect at New Settings (4800 Baud)
    // ---------------------------------------------------------
    Serial.println("\n[STEP 4] Re-initializing RS485 Port at 4800 Baud...");
    MachineControl_RS485Comm.begin(4800, SERIAL_8N1, 0, 500);
    MachineControl_RS485Comm.receive();
    ModbusRTUClient.begin(MachineControl_RS485Comm, 4800, SERIAL_8N1);

    Serial.println("Verifying new settings by reading Temperature/Humidity at Address 10...");
    if (!ModbusRTUClient.requestFrom(NEW_ADDRESS, HOLDING_REGISTERS, 0x0000, 2)) {
        Serial.print("Verification failed. Modbus Error: ");
        Serial.println(ModbusRTUClient.lastError());
        Serial.println("Did you power cycle it? If so, the wiring or sensor might be faulty.");
    } else {
        Serial.println("\n!!! VERIFICATION SUCCESS !!!");
        int16_t humidity_raw = ModbusRTUClient.read();
        int16_t temperature_raw = ModbusRTUClient.read();
        
        Serial.print("   -> Air Temp: ");
        Serial.print(temperature_raw / 100.0f);
        Serial.println(" C");
        
        Serial.print("   -> Air Hum: ");
        Serial.print(humidity_raw / 100.0f);
        Serial.println(" %");
        
        Serial.println("\nPERFECT! The STH30 is now permanently set to Address 10 @ 4800 baud.");
        Serial.println("You can now plug the Soil Sensor (Address 1) back in and flash the main GMS firmware!");
    }
}

void loop() {
    delay(1000);
}
