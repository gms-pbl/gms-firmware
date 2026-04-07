#include "main.h"
#include "module/task_sensor/task_sensor.h"
#include "module/task_io/task_io.h"

using namespace gms_edge;

void setup() {
    Serial.begin(115200);
    RPC.begin();

    // Initialize tasks
    module::task_io::init();
    module::task_sensor::init();
}

void loop() {
    // RTOS threads handle all logic
    rtos::ThisThread::yield();
}