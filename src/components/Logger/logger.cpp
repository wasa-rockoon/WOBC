#include "logger.h"

namespace component {

Logger::Logger(SPIClass& spi, pin_t SD_cs, pin_t SD_inserted, float clock_freq)
  : process::Component("Logger", component_id),
    clock_(*this, clock_freq),
    spi_(spi), SD_cs_(SD_cs), SD_inserted_(SD_inserted) {
}

void Logger::setup() {
  listen(all_packets_, WOBC_LOGGER_PACKET_QUEUE_SIZE);

  pinMode(SD_inserted_, INPUT_PULLUP);
  openFile();
}

void Logger::loop() {
  while (file_ && all_packets_) {
    // パケット書き込み

    const wcpp::Packet packet = all_packets_.pop();

    uint8_t buf[wcpp::size_max];
    wcpp::Packet packet_log = wcpp::Packet::empty(buf, wcpp::size_max);

    if (packet.isLocal()) { // Add unit id
      if (packet.isCommand()) {
        packet_log.command(
          packet.packet_id(), packet.component_id(), kernel::unit_id(), kernel::unit_id());
      }
      else {
        packet_log.telemetry(
          packet.packet_id(), packet.component_id(), kernel::unit_id(), kernel::unit_id());
      }
      packet_log.copyPayload(packet);
    }
    else {
      packet_log.copy(packet);
    }

    packet_log.append("Ts").setInt(millis()); // Add timestamp in ms

    bool ok = true;
    ok |= file_.write(packet.encode(), packet.size()) == packet.size();
    ok |= file_.write((uint8_t)packet.checksum()) == 1;
    ok |= file_.write((uint8_t)'\0') == 1;

    if (!ok) {
      error("cWE", "SD log write error");
      file_.close();
    }
    else {
      packets_wrote_++;
      bytes_wrote_ += packet_log.size() + 2;
    }
  }
}


bool Logger::openFile() {
  if (file_) {
    return true;
  }

  if (digitalRead(SD_inserted_)) {
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
  snprintf(file_name, sizeof(file_name), "/log_%4d.bin", file_number);

  file_ = SD.open(file_name, FILE_APPEND);

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

void Logger::sendLog() {
  // Log
  wcpp::Packet log = newPacket(32);
  log.telemetry(log_telemetry_id, component_id);
  log.append("Fo").setBool(!!file_);
  log.append("Bw").setInt(bytes_wrote_);
  log.append("Pw").setInt(packets_wrote_);
  sendPacket(log);
}

Logger::Clock::Clock(Logger& logger, float freq)
  : process::Timer("LoggerClock", 1000 / freq),
    logger_(logger) {
}

void Logger::Clock::callback() { 
  if (!logger_.file_) {
    logger_.openFile();
  }

  logger_.sendLog();
  logger_.flushFile();
}

}
