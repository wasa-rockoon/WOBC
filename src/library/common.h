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

constexpr pin_t no_pin = 255;

constexpr uint8_t packet_id_error = '!';
constexpr uint8_t packet_id_heartbeat = '"';
constexpr uint8_t packet_id_log = '#';

constexpr uint8_t unit_id_local = 0x00;