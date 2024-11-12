#pragma once

#include "generic_serial.h"

#ifdef ARDUINO_ARCH_ESP32
#include "esp32/can.h"
#include "esp32/kvs.h"
#endif

#ifdef ARDUINO_ARCH_RP2040
#include "rp2040/can.h"
#include "rp2040/kvs.h"
#endif