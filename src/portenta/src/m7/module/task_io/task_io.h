#pragma once

#ifndef TASK_IO_H
#define TASK_IO_H

#include "../../main.h"

namespace gms_edge {
namespace module {
namespace task_io {

void init();
int set_output(int channel, int state);

} // namespace task_io
} // namespace module
} // namespace gms_edge

#endif // TASK_IO_H