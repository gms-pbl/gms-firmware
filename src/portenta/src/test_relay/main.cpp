#include <Arduino.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include <Arduino_PortentaMachineControl.h>

void setup() {
    Serial.begin(115200);
    delay(3000); // Wait for Serial to open

    Serial.println("\n\n--- Air Sensor Address Scanner ---");
    
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
}

void loop() {
    Serial.println("--- Scanning Addresses 1 to 15 ---");
    
    for (int address = 1; address <= 15; address++) {
        Serial.print("Testing Address ");
        Serial.print(address);
        Serial.print("... ");
        
        // Request 2 registers (Humidity and Temp) at address 0x0000
        if (!ModbusRTUClient.requestFrom(address, HOLDING_REGISTERS, 0x0000, 2)) {
            Serial.println("Timeout / No response.");
        } else {
            Serial.println("\n!!! SUCCESS !!! SENSOR FOUND AT THIS ADDRESS!");
            
            int16_t humidity_raw = ModbusRTUClient.read();
            int16_t temperature_raw = ModbusRTUClient.read();
            
            float humidity = humidity_raw / 10.0f;
            float temperature = temperature_raw / 10.0f;
            
            Serial.print("   -> Air Temp: ");
            Serial.print(temperature);
            Serial.println(" C");
            
            Serial.print("   -> Air Hum: ");
            Serial.print(humidity);
            Serial.println(" %");
        }
        
        delay(100); // Small delay between scans
    }

    Serial.println("Scan complete. Waiting 5 seconds...\n");
    delay(5000);
}
