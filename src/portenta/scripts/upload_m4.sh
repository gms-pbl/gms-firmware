#!/bin/bash
cd "$(dirname "$0")/.."
~/.platformio/packages/tool-dfuutil-arduino/dfu-util --device 2341:035b -D .pio/build/portenta_h7_m4/firmware.bin -a 0 --dfuse-address=0x08100000:leave
