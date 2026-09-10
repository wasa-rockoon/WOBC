#include <Arduino.h>
#include <Preferences.h>
#include <esp32_can.h>
#include <driver/twai.h>
#include <library/core/can_bus.h>  // Share the production baudrate default.
#include <modules/Separation/hardware.h>

namespace {
bool can_ready = false;
uint32_t received = 0;
uint32_t extended = 0;
uint32_t last_report = 0;
unsigned printed = 0;
uint8_t stored_id = 0;
const char* id_status = "unavailable";

void readModuleId() {
  Preferences prefs;
  // Read only: never clear or replace the production identity.
  if (!prefs.begin("KVS", true)) {
    id_status = "KVS_absent_or_open_failed";
    return;
  }
  const char key[] = {static_cast<char>(0xFF), static_cast<char>(0xFF), 0};
  if (!prefs.isKey(key)) {
    id_status = "not_stored";
  } else if (prefs.getBytesLength(key) != 1 ||
             prefs.getBytes(key, &stored_id, 1) != 1) {
    id_status = "invalid_or_read_failed";
  } else {
    id_status = stored_id == 'S' ? "MATCH" : "MISMATCH";
  }
  prefs.end();
}
}

void setup() {
  using namespace separation_hardware;
  digitalWrite(separation::high_side, separation::safe_level);
  digitalWrite(separation::low_side, separation::safe_level);
  pinMode(separation::high_side, OUTPUT);
  pinMode(separation::low_side, OUTPUT);
  digitalWrite(separation::high_side, separation::safe_level);
  digitalWrite(separation::low_side, separation::safe_level);

  Serial.begin(115200);
  const uint32_t started = millis();
  while (!Serial && millis() - started < 3000) delay(10);
  readModuleId();
  CAN0.setCANPins(static_cast<gpio_num_t>(can::rx),
                  static_cast<gpio_num_t>(can::tx));
  can_ready = CAN0.begin(WOBC_CAN_BUS_BAUDRATE) != 0;
  if (can_ready) CAN0.watchFor();
  // No callback: both standard and extended frames go to the read queue.
  // Normal CAN mode acknowledges received frames; no application frames sent.
}

void loop() {
  CAN_FRAME frame;
  // Bound each pass so continuous traffic cannot starve the status report.
  for (unsigned i = 0; can_ready && i < 64 && CAN0.read(frame); ++i) {
    ++received;
    if (frame.extended) ++extended;
    if (printed++ < 20) {
      Serial.printf("RX id=%08lX ext=%u rtr=%u len=%u data=",
                    static_cast<unsigned long>(frame.id),
                    unsigned(frame.extended), unsigned(frame.rtr),
                    unsigned(frame.length));
      if (!frame.rtr) {
        for (unsigned j = 0; j < frame.length && j < 8; ++j)
          Serial.printf("%02X ", frame.data.uint8[j]);
      }
      Serial.println();
    }
  }
  if (millis() - last_report >= 1000) {
    last_report = millis();
    printed = 0;
    Serial.printf("DIAG module_expected=53 stored=%02X id=%s CAN=%s rx_pin=%u tx_pin=%u baud=%u RX=%lu EXT=%lu\n",
                  unsigned(stored_id), id_status, can_ready ? "OK" : "INIT_FAILED",
                  unsigned(separation_hardware::can::rx),
                  unsigned(separation_hardware::can::tx),
                  unsigned(WOBC_CAN_BUS_BAUDRATE),
                  static_cast<unsigned long>(received),
                  static_cast<unsigned long>(extended));
    twai_status_info_t status{};
    if (can_ready && twai_get_status_info(&status) == ESP_OK) {
      Serial.printf("TWAI state=%u TEC=%lu REC=%lu bus_errors=%lu rx_missed=%lu rx_overrun=%lu\n",
                    unsigned(status.state),
                    static_cast<unsigned long>(status.tx_error_counter),
                    static_cast<unsigned long>(status.rx_error_counter),
                    static_cast<unsigned long>(status.bus_error_count),
                    static_cast<unsigned long>(status.rx_missed_count),
                    static_cast<unsigned long>(status.rx_overrun_count));
    }
  }
  delay(1);
}
