#include "task_io.h"
#include <Arduino_PortentaMachineControl.h>

namespace gms_edge {
namespace module {
namespace task_io {

// RPC Callback: Set Digital Output (0-7)
// Pins 0-3 are connected to relays, 4-7 to the rail
int set_output(int channel, int state) {
    if (channel < 0 || channel > 7) {
        Serial.println("Error: Output channel out of bounds (0-7)");
        return -1;
    }

    if (state) {
        MachineControl_DigitalOutputs.write(channel, HIGH);
        Serial.print("Output "); Serial.print(channel); Serial.println(" set to HIGH");
    } else {
        MachineControl_DigitalOutputs.write(channel, LOW);
        Serial.print("Output "); Serial.print(channel); Serial.println(" set to LOW");
    }
    
    return 1;
}

void init() {
    Serial.println("Initializing Machine Control I/O...");

    // Initialize the I/O expanders and outputs
    MachineControl_DigitalOutputs.begin(true);
    MachineControl_DigitalOutputs.writeAll(0);
    
    // Bind the RPC function so the M4 core can call it when an MQTT message arrives
    RPC.bind("set_output", set_output);
}

} // namespace task_io
} // namespace module
} // namespace gms_edge