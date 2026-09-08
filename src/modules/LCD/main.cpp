// GS2026: LoRa基板からCANで届くGPSを20文字x4行のLCDに表示する。
#include <Arduino.h>
#include <LiquidCrystal.h>
#include <library/wobc.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint8_t module_id = 0x47;
constexpr uint8_t unit_id = 0x64;
constexpr uint8_t gps_component_id = 0x15;
constexpr uint8_t gps_packet_id = 'M';

// hardware/modules/GS2026 の2026-08-28基板データ、LCD501。
constexpr uint8_t lcd_rs = 8;
constexpr uint8_t lcd_enable = 18;
constexpr uint8_t lcd_db4 = 6;
constexpr uint8_t lcd_db5 = 7;
constexpr uint8_t lcd_db6 = 5;
constexpr uint8_t lcd_db7 = 4;
constexpr uint8_t lcd_columns = 20;
constexpr uint8_t lcd_rows = 4;
constexpr uint32_t page_interval_ms = 2000;
constexpr uint32_t refresh_interval_ms = 100;
constexpr uint32_t gps_timeout_ms = 5000;

core::CANBus can_bus(44, 43); // RX, TX。既存WOBCと同じ125kbps。
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

class Main : public process::Component {
public:
  Main() : process::Component("LCD", 0x00),
           lcd_(lcd_rs, lcd_enable, lcd_db4, lcd_db5, lcd_db6, lcd_db7) {
    priority_ = 1;
  }

protected:
  void setup() override {
    gps_listener_.telemetry().packet(gps_packet_id).component(gps_component_id);
    listen(gps_listener_, 8);
    lcd_.begin(lcd_columns, lcd_rows);
    lcd_.noCursor();
    lcd_.noBlink();
    page_started_ms_ = millis();
    last_refresh_ms_ = page_started_ms_;
    render(page_started_ms_);
  }

  void loop() override {
    // 表示切替中も受信を続ける。2秒のdelayは使わない。
    // 連続受信時も表示処理に戻れるよう、1回に最大8パケットを読む。
    for (unsigned i = 0; i < 8 && gps_listener_; ++i) {
      const wcpp::Packet packet = gps_listener_.pop();
      if (packet) receiveGps(packet);
    }

    const uint32_t now = millis();
    const uint32_t elapsed_pages = (now - page_started_ms_) / page_interval_ms;
    if (elapsed_pages != 0) {
      if (elapsed_pages % 2 != 0) altitude_page_ = !altitude_page_;
      page_started_ms_ += elapsed_pages * page_interval_ms;
    }
    if (now - last_refresh_ms_ >= refresh_interval_ms) {
      render(now);
      last_refresh_ms_ = now;
    }
    delay(10);
  }

private:
  kernel::Listener gps_listener_;
  LiquidCrystal lcd_;
  double latitude_ = 0.0;
  double longitude_ = 0.0;
  double altitude_ = 0.0;
  uint8_t origin_unit_id_ = 0;
  bool received_gps_ = false;
  bool altitude_page_ = false;
  uint32_t last_gps_ms_ = 0;
  uint32_t page_started_ms_ = 0;
  uint32_t last_refresh_ms_ = 0;
  char displayed_[lcd_rows][lcd_columns + 1] = {};

  static bool readNumber(const wcpp::Packet& packet, const char* name, double& value) {
    const auto entry = packet.find(name);
    if (!entry || (!(*entry).isFloat() && !(*entry).isInt())) return false;
    value = (*entry).getFloat64();
    return std::isfinite(value);
  }

  void receiveGps(const wcpp::Packet& packet) {
    double latitude, longitude, altitude;
    // LA/LO: 度（float32/64）、AL: GPS高度[m]（現行送信側は整数）。
    // 3項目が揃った同一パケットだけを採用し、別時刻・別送信元の値を混ぜない。
    if (!readNumber(packet, "LA", latitude) ||
        !readNumber(packet, "LO", longitude) ||
        !readNumber(packet, "AL", altitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) return;

    latitude_ = latitude;
    longitude_ = longitude;
    altitude_ = altitude;
    origin_unit_id_ = packet.origin_unit_id();
    last_gps_ms_ = millis();
    received_gps_ = true;
  }

  void writeLine(uint8_t row, const char* text) {
    char line[lcd_columns + 1];
    std::memset(line, ' ', lcd_columns);
    const size_t length = std::strlen(text);
    std::memcpy(line, text, length < lcd_columns ? length : lcd_columns);
    line[lcd_columns] = '\0';
    // 桁数が減ったときの残像を消し、変更行だけを更新してちらつきを抑える。
    if (std::memcmp(displayed_[row], line, sizeof(line)) == 0) return;
    lcd_.setCursor(0, row);
    lcd_.print(line);
    std::memcpy(displayed_[row], line, sizeof(line));
  }

  void render(uint32_t now) {
    writeLine(0, altitude_page_ ? "GPS ALTITUDE" : "GPS LAT / LON");
    if (!received_gps_) {
      writeLine(1, "Waiting for GPS...");
      writeLine(2, "via CAN 125kbps");
      writeLine(3, "No GPS data");
      return;
    }

    char line[48];
    if (altitude_page_) {
      // 巨大な異常値も固定長表示に収める。
      if (altitude_ >= -999999.9 && altitude_ <= 9999999.9) {
        std::snprintf(line, sizeof(line), "Alt: %.1f m", altitude_);
      } else {
        std::snprintf(line, sizeof(line), "Alt: %.3e m", altitude_);
      }
      writeLine(1, line);
      writeLine(2, "GPS altitude [m]");
    } else {
      std::snprintf(line, sizeof(line), "Lat: %.6f", latitude_);
      writeLine(1, line);
      std::snprintf(line, sizeof(line), "Lon: %.6f", longitude_);
      writeLine(2, line);
    }

    // 既存GPSパケットには測位有効フラグがないため、受信状態のみを示す。
    // 途絶後も最終値は残し、STALEと経過秒数で古いデータと区別する。
    const uint32_t age_ms = now - last_gps_ms_;
    std::snprintf(line, sizeof(line), "%s U:%02X %lus",
                  age_ms >= gps_timeout_ms ? "STALE" : "CAN RX",
                  static_cast<unsigned>(origin_unit_id_),
                  static_cast<unsigned long>(age_ms / 1000));
    writeLine(3, line);
  }
};

Main main_;

} // namespace

void setup() {
  Serial.begin(115200);
  status_indicator.begin();
  error_indicator.begin();
  error_indicator.set(true);

  kernel::setUnitId(unit_id);
  if (!kernel::begin(module_id, true)) return;
  status_indicator.blink_on_change();
  serial_bus.begin();
  can_bus.begin();
  if (!main_.begin()) return;

  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  status_indicator.update();
  error_indicator.update();
  delay(1);
}
