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

namespace wobc {

  // 環境の違いを吸収する共通のSPI初期化関数 (テンプレート版)
  template <typename TSPI>
  inline void beginSPI(TSPI& spi, int sck, int miso, int mosi, int ss) {
#if defined(ARDUINO_ARCH_RP2350) || defined(ARDUINO_ARCH_RP2040)
    spi.setSCK(sck);
    spi.setRX(miso);
    spi.setTX(mosi);
    spi.setCS(ss);
    spi.begin();
#else
    spi.begin(sck, miso, mosi, ss);
#endif
  }

  // 環境の違いを吸収する共通のI2C初期化関数 (テンプレート版)
  template <typename TWire>
  inline void beginI2C(TWire& wire, int sda, int scl, uint32_t freq = 400000) {
#if defined(ARDUINO_ARCH_RP2350) || defined(ARDUINO_ARCH_RP2040)
    wire.setSDA(sda);
    wire.setSCL(scl);
    wire.begin();
    wire.setClock(freq);
#else
    wire.begin(sda, scl, freq);
#endif
  }

}