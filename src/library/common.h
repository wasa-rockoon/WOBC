#pragma once

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
#include <Arduino.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <task.h>
#include <atomic>
#else 
#error
#endif

// メインからLoggerへの分割要求フラグ
inline std::atomic<bool> request_file_split{false};

using pin_t = byte;

constexpr pin_t no_pin = 255;

constexpr uint8_t packet_id_error = '!';
constexpr uint8_t packet_id_heartbeat = '"';
constexpr uint8_t packet_id_log = '#';

constexpr uint8_t unit_id_local = 0x00;

namespace wobc {

  //SPI初期化用ラッパ関数
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

  //I2C初期化用ラッパ関数
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