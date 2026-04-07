#!/bin/bash
# Helper script to flash Portenta H7 without PlatformIO's serial port bug

echo "Compiling firmware..."
pio run

echo ""
echo "!!! PLEASE DOUBLE-TAP THE RESET BUTTON NOW !!!"
echo "Waiting for Portenta Bootloader (ID 2341:035b)..."

# Wait for device to appear
while ! ~/.platformio/packages/tool-dfuutil-arduino/dfu-util -l 2>/dev/null | grep -q "2341:035b"; do
    sleep 1
done

echo "Device found! Flashing M7 Core..."
~/.platformio/packages/tool-dfuutil-arduino/dfu-util --device 2341:035b -D .pio/build/portenta_h7_m7/firmware.bin -a 0 --dfuse-address=0x08040000:leave

echo ""
echo "M7 Flashed. The board has rebooted."
echo "!!! PLEASE DOUBLE-TAP THE RESET BUTTON AGAIN !!!"
echo "Waiting for Bootloader..."

while ! ~/.platformio/packages/tool-dfuutil-arduino/dfu-util -l 2>/dev/null | grep -q "2341:035b"; do
    sleep 1
done

echo "Device found! Flashing M4 Core..."
~/.platformio/packages/tool-dfuutil-arduino/dfu-util --device 2341:035b -D .pio/build/portenta_h7_m4/firmware.bin -a 0 --dfuse-address=0x08100000:leave

echo "All done!"
