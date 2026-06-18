#include "logger.h"

// RP2350/2040のマルチコア機能を使うためのインクルード
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
#include <pico/multicore.h>
component::Logger* global_logger_ptr = nullptr;
#endif

namespace component {

Logger::Logger(SPIClass& spi, pin_t SD_cs, pin_t SD_inserted, float clock_freq)
  : process::Component("Logger", component_id),
    clock_(*this, clock_freq), spi_(spi), SD_inserted_(SD_inserted), SD_cs_(SD_cs) {
}

void Logger::setup() {
  listen(all_packets_, WOBC_LOGGER_PACKET_QUEUE_SIZE);
  if (SD_inserted_ >= 0){
    pinMode(SD_inserted_, INPUT_PULLUP);
  }
  while (!openFile()) {
    error("cOR", "Retrying to open SD card...");
    delay(100);
  }
  start(clock_);

  

  // ====== タスクの分離（コア1 or 0への割り当て） ======
#if defined(ARDUINO_ARCH_ESP32)
  xTaskCreatePinnedToCore(
      sdWriteTaskWrapper, "SD_Task", 8192, this, 1, &sdTaskHandle_, 0
  );
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
  global_logger_ptr = this;
  multicore_launch_core1(sdWriteTaskWrapper);
#endif
}

// フレームワーク側のloopは空にする（ブロックさせない）
void Logger::loop() {
}

// ====== 裏で動き続けるSD書き込み専用タスク ======
void Logger::sdWriteTask() {
  while (true) {
    bool wrote_something = false;
    int burst_count = 0; // バースト処理の回数をカウント

    while (file_ && all_packets_.available() > 0) {
      const wcpp::Packet packet = all_packets_.pop();

      bool ok = true;
      ok &= file_.write(packet.encode(), packet.size()) == packet.size();
      ok &= file_.write((uint8_t)packet.checksum()) == 1;
      ok &= file_.write((uint8_t)'\0') == 1;

      if (!ok) {
        error("cWE", "SD log write error");
        file_.close();
        break; 
      } else {
        packets_wrote_++;
        bytes_wrote_ += packet.size() + 2;
        wrote_something = true;
      }

      // ★ 最強のWDT対策：50個（0.5秒分）処理するごとに、OSに一瞬だけ呼吸させる
      burst_count++;
      if (burst_count % 50 == 0) {
#if defined(ARDUINO_ARCH_ESP32)
        taskYIELD(); // FreeRTOSのコンテキストスイッチ（1msも待たず、数μsで戻る）
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
        yield(); 
#endif
      }
    }

    // ----- 2. 20分（1,200,000 ms）経過のチェックと分割 -----
    if (file_ && request_file_split.load()) {
      
      // ② センサータスクが今のI2C通信を終えてZOHモードに入るのを待つ
      delay(10); 

      // ③ 今のファイルを閉じる（ここで数百ms〜数秒かかることがある）
      file_.flush();
      file_.close();

      // ④ 新しいファイルを開く（既存のopenFile関数などを利用）
      // ※ openFile() の中身で連番のインクリメント等が行われる想定
      while (!openFile()) {
        delay(10); // 開けなかったら少し待ってリトライ
      }

      // ⑤ タイマーリセット & センサータスクに「I2C再開してヨシ！」と通達
      request_file_split.store(false);
    }

    // 暇なタイミングでFlush
    if (file_ && wrote_something) {
      static uint32_t last_flush_time = 0;
      if (millis() - last_flush_time > 1000) {
        file_.flush();
        last_flush_time = millis();
      }
    }

    // キューが空になったら、今まで通り1ms休んで次のデータ群を待つ
#if defined(ARDUINO_ARCH_ESP32)
    vTaskDelay(pdMS_TO_TICKS(1)); 
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    delay(1); 
#endif
  }
}

// ====== OS向けラッパ関数 ======
#if defined(ARDUINO_ARCH_ESP32)
void Logger::sdWriteTaskWrapper(void* parameter) {
  Logger* logger = static_cast<Logger*>(parameter);
  logger->sdWriteTask();
}
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
void Logger::sdWriteTaskWrapper() {
  if(global_logger_ptr) {
    global_logger_ptr->sdWriteTask();
  }
}
#endif

bool Logger::openFile() {
  if (file_) {
    return true;
  }

  if (SD_inserted_ >= 0 && digitalRead(SD_inserted_)) {
      error("cNI", "SD card is not inserted");
      return false;
  }
  if (!SD.begin(SD_cs_)) { 
    error("cBF", "failed to initialize sd card");
    return false;
  }

  unsigned file_number = 0;
  wcpp::Packet card_info = loadPacket('c'); 
  if (card_info) {
    auto e = card_info.find("Fn");
    if (e) file_number = (*e).getUInt();
  }

  char file_name[16];
  snprintf(file_name, sizeof(file_name), "/log_%04d.bin", file_number);

  #if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    file_ = SD.open(file_name, "a");
  #elif defined(ARDUINO_ARCH_ESP32)
    file_ = SD.open(file_name, FILE_APPEND);
  #endif

  if (!file_) {
    error("cOF", "failed to open file: %s", file_name);
    SD.end();
    return false;
  }

  file_number++;
  wcpp::Packet card_info_updated = newPacket(32);
  card_info_updated.telemetry('c', component_id);
  card_info_updated.append("Fn").setInt(file_number);
  storePacket(card_info_updated);

  return true;
}

void Logger::flushFile() {
  if (file_) file_.flush();
}

/*
void Logger::sendLog() {
  wcpp::Packet log = newPacket(64);
  log.telemetry(log_telemetry_id, component_id);
  log.append("Fo").setBool(!!file_);
  log.append("Bw").setInt(bytes_wrote_);
  log.append("Pw").setInt(packets_wrote_);
  log.append("Qz").setInt(all_packets_.available());
  log.append("Qm").setInt(WOBC_LOGGER_PACKET_QUEUE_SIZE);
  sendPacket(log);
}*/

Logger::Clock::Clock(Logger& logger, float freq)
  : process::Timer("LoggerClock", 1000 / freq),
    logger_(logger) {
}

void Logger::Clock::callback() { 
  if (!logger_.file_) {
    // 1秒に1回だけ再オープンを試みる（100Hzの連打を防ぐ）
    static uint32_t last_retry_time = 0;
    if (millis() - last_retry_time > 1000) {
      logger_.openFile();
      last_retry_time = millis();
  //logger_.sendLog();
}
}
}

}