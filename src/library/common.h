#pragma once

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)
#include <Arduino.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <task.h>
#else 
#error
#endif

using pin_t = byte;

constexpr uint8_t packet_id_error = '!';
constexpr uint8_t packet_id_log = '#';