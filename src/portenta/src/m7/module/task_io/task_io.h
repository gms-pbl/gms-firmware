#pragma once

#ifndef TASK_IO_H
#define TASK_IO_H

#include "../../main.h"

namespace gms_edge {
namespace module {
namespace task_io {

void init();
int set_output(int channel, int state);
void get_output_states(
    int& dout0,
    int& dout1,
    int& dout2,
    int& dout3,
    int& dout4,
    int& dout5,
    int& dout6,
    int& dout7);

} // namespace task_io
} // namespace module
} // namespace gms_edge

#endif // TASK_IO_H
