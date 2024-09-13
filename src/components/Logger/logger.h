#pragma once
#include <library/wobc.h>
#include <SPI.h>
#include <SD.h>

namespace component {

#ifndef WOBC_LOGGER_PACKET_QUEUE_SIZE
#define WOBC_LOGGER_PACKET_QUEUE_SIZE 32
#endif

class Logger: public process::Component {
public:
  static const uint8_t component_id = 20; // TODO
  static const uint8_t log_telemetry_id = 'L';

  Logger(SPIClass& spi, pin_t SD_cs, pin_t SD_inserted = no_pin, float clock_freq = 1.0);

protected:

  class Clock: public process::Timer { // ログ出力等のタイマー
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

  friend Clock;
};

}