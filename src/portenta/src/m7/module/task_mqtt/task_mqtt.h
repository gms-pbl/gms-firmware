#pragma once

#ifndef TASK_MQTT_H
#define TASK_MQTT_H

#include "../../main.h"

namespace gms_edge {
namespace module {
namespace task_mqtt {

void init();
void process();
int publish_telemetry(float airHum, float airTemp, float soilMoist, float soilTemp, float soilCond, float soilPh, float soilN, float soilP, float soilK, float soilSal, float soilTds, int din0, int din1, int din2, int din3);

} // namespace task_mqtt
} // namespace module
} // namespace gms_edge

#endif // TASK_MQTT_H