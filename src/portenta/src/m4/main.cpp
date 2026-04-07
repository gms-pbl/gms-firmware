#include "main.h"
#include "module/task_mqtt/task_mqtt.h"

using namespace gms_edge;

void setup() {
    // M4 doesn't have a USB serial port, so Serial.begin connects to the hardware UART.
    // Instead, RPC will route prints automatically if configured.
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