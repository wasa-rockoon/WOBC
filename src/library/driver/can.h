#pragma once

#ifdef ARDUINO_ARCH_ESP32
#include "esp32/can.h"
#endif

#ifdef ARDUINO_ARCH_RP2040
#include "rp2040/can.h"
#endif