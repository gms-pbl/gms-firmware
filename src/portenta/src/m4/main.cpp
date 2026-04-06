#include "main.h"
#include "module/task_mqtt/task_mqtt.h"

using namespace gms_edge;

void setup() {
    Serial.begin(115200);
    RPC.begin();

    // Initialize tasks
    module::task_mqtt::init();
}

void loop() {
    // Process MQTT keep-alives and incoming messages
    module::task_mqtt::process();
    rtos::ThisThread::yield();
}