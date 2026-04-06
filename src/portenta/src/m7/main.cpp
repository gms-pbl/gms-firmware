#include "main.h"
#include "module/task_sensor/task_sensor.h"

using namespace gms_edge;

void setup() {
    Serial.begin(115200);
    RPC.begin();

    // Initialize tasks
    module::task_sensor::init();
}

void loop() {
    // RTOS threads handle all logic
    rtos::ThisThread::yield();
}