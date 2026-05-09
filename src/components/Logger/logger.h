#pragma once
#include <library/wobc.h>
#include <SPI.h>
#include <SD.h>

namespace component {

#ifndef WOBC_LOGGER_PACKET_QUEUE_SIZE
#define WOBC_LOGGER_PACKET_QUEUE_SIZE 256
#endif

#ifndef WOBC_LOGGER_FLUSH_INTERVAL
#define WOBC_LOGGER_FLUSH_INTERVAL 50
#endif

class Logger: public process::Component {
public:
  static const uint8_t component_id = 20;
  static const uint8_t log_telemetry_id = 'L';

  Logger(SPIClass& spi, pin_t SD_cs, pin_t SD_inserted = no_pin, float clock_freq = 1.0);

protected:
  class Clock: public process::Timer {
  public:
    Clock(Logger& logger, float freq);
  protected:
    Logger& logger_;
    void callback() override;
  } clock_;

  SPIClass& spi_;
  pin_t SD_inserted_;
  pin_t SD_cs_;
  File file_;
  Listener all_packets_;

  unsigned packets_wrote_;
  unsigned bytes_wrote_;

  void setup() override;
  void loop() override;

  bool openFile();
  void sendLog();
  void flushFile();
  void sdWriteTask(); // 実際に裏で回し続ける処理

#if defined(ARDUINO_ARCH_ESP32)
  TaskHandle_t sdTaskHandle_;
  static void sdWriteTaskWrapper(void* parameter);
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
  static void sdWriteTaskWrapper();
#endif

  friend Clock;
};

}