#!/bin/bash
cd "$(dirname "$0")/.."
pio run -e sensor_util
