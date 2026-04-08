#!/bin/bash
cd "$(dirname "$0")/.."
~/.platformio/packages/tool-dfuutil-arduino/dfu-util --device 2341:035b -a 0 --dfuse-address=0x08040000:mass-erase:force:leave
