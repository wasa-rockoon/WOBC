#include "logger.h"

namespace component {

Logger::Logger(SPIClass& spi)
  : process::Component("Logger", component_id),
    spi_(spi) {
}

void Logger::setup() {
  listen(all_packets_, 8);

  // TODO SDカード初期化
}

void Logger::loop() {
  while (all_packets_) {
    const wcpp::Packet& packet = all_packets_.pop();
    // TODO パケット書き込み
  }
}

}
