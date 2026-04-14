#include "task_io.h"
#include <Arduino_PortentaMachineControl.h>

namespace gms_edge {
namespace module {
namespace task_io {

namespace {

int output_states[8] = {0, 0, 0, 0, 0, 0, 0, 0};

} // namespace

// RPC Callback: Set Digital Output (0-7)
// Pins 0-3 are connected to relays, 4-7 to the rail
int set_output(int channel, int state) {
    if (channel < 0 || channel > 7) {
        Serial.println("Error: Output channel out of bounds (0-7)");
        return -1;
    }

    int normalized_state = state ? 1 : 0;

    if (normalized_state) {
        MachineControl_DigitalOutputs.write(channel, HIGH);
        Serial.print("Output "); Serial.print(channel); Serial.println(" set to HIGH");
    } else {
        MachineControl_DigitalOutputs.write(channel, LOW);
        Serial.print("Output "); Serial.print(channel); Serial.println(" set to LOW");
    }

    output_states[channel] = normalized_state;
    
    return 1;
}

void get_output_states(
    int& dout0,
    int& dout1,
    int& dout2,
    int& dout3,
    int& dout4,
    int& dout5,
    int& dout6,
    int& dout7) {
    dout0 = output_states[0];
    dout1 = output_states[1];
    dout2 = output_states[2];
    dout3 = output_states[3];
    dout4 = output_states[4];
    dout5 = output_states[5];
    dout6 = output_states[6];
    dout7 = output_states[7];
}

void init() {
    Serial.println("Initializing Machine Control I/O...");

    // Initialize the I/O expanders and outputs
    MachineControl_DigitalOutputs.begin(true);
    MachineControl_DigitalOutputs.writeAll(0);
    for (int channel = 0; channel < 8; ++channel) {
        output_states[channel] = 0;
    }
}

} // namespace task_io
} // namespace module
} // namespace gms_edge
