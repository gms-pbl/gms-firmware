#include "main.h"
#include <Wire.h>
#include "module/task_sensor/task_sensor.h"
#include "module/task_io/task_io.h"

using namespace gms_edge;

void setup() {
    bootM4(); // Boot the M4 core
    Serial.begin(115200);
    // Wait up to 5 seconds for the Serial monitor to connect
    uint32_t start = millis();
    while (!Serial && millis() - start < 5000) {
        delay(10);
    }
    
    Serial.println("\n\n--- M7 Core Booting ---");
    Serial.println("Initializing RPC...");
    RPC.begin();
    Serial.println("RPC Initialized.");

    // Match mentor's initialization order: Modbus RS485 -> Wire I2C -> Digital Inputs -> Digital Outputs
    Serial.println("Initializing Sensor Task...");
    module::task_sensor::init();
    Serial.println("Sensor Task Initialized.");

    Serial.println("Initializing IO Task...");
    module::task_io::init();
    Serial.println("IO Task Initialized.");
    
    Serial.println("Setup Complete. Entering Loop.");
}

void loop() {
    Serial.println("[M7] Heartbeat - Polling sensors...");
    // RTOS threads handle all logic
    rtos::ThisThread::sleep_for(std::chrono::seconds(5));
}