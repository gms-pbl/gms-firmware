#include "main.h"
#include "module/task_sensor/task_sensor.h"
#include "module/task_io/task_io.h"

using namespace gms_edge;

void setup() {
    bootM4();
    Serial.begin(115200);
    // Give Serial a moment to initialize
    delay(2000);
    
    RPC.begin();

    // Initialize tasks
    module::task_io::init();
    module::task_sensor::init();
}

void loop() {
    // RTOS threads handle all logic
    rtos::ThisThread::yield();
}