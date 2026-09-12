#include <Arduino.h>
#include <library/wobc.h>
#include <components/LoRa/dual_lora.h>

// LoRa2026基板。ピン番号はESP32-S3側のGPIO。
// LoRa1はアップリンク、LoRa2はダウンリンク。通信相手と合わせる。
#define LORA1_CHANNEL 10
#define LORA2_CHANNEL 3

#define LORA1_TX_PIN 13
#define LORA1_RX_PIN 12
#define LORA1_AUX_PIN 11
#define LORA1_M0_PIN 14
#define LORA1_M1_PIN 21
#define LORA1_SW_A1 39
#define LORA1_SW_A2 40

#define LORA2_TX_PIN 7
#define LORA2_RX_PIN 18
#define LORA2_AUX_PIN 8
#define LORA2_M0_PIN 5
#define LORA2_M1_PIN 6
#define LORA2_SW_A1 9
#define LORA2_SW_A2 10

// 元のmodules/LoRa/main.cppと同様、通常はUSBシリアルで接続する。
#ifndef LORA_USE_CAN
#define LORA_USE_CAN 0
#endif


#if defined(LORA_ESP_ROLE_GROUND) == defined(LORA_ESP_ROLE_FLIGHT)
#error "Select exactly one of LORA_ESP_ROLE_GROUND or LORA_ESP_ROLE_FLIGHT"
#endif

constexpr uint8_t module_id = 0x4C;
#if defined(LORA_ESP_ROLE_GROUND)
constexpr uint8_t unit_id = 0x63;
constexpr bool is_ground = true;
#else
constexpr uint8_t unit_id = 0x61;
constexpr bool is_ground = false;
#endif

// Core機能
core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

// コンポーネント
// UART0はUSBコンソール用。無線には独立したUART2とUART1を使う。
constexpr component::DualLoRa::RadioConfig lora1_config = {
    2, LORA1_AUX_PIN, LORA1_M0_PIN, LORA1_M1_PIN,
    LORA1_RX_PIN, LORA1_TX_PIN, LORA1_SW_A1, LORA1_SW_A2, LORA1_CHANNEL};
constexpr component::DualLoRa::RadioConfig lora2_config = {
    1, LORA2_AUX_PIN, LORA2_M0_PIN, LORA2_M1_PIN,
    LORA2_RX_PIN, LORA2_TX_PIN, LORA2_SW_A1, LORA2_SW_A2, LORA2_CHANNEL};
component::DualLoRa dual_lora(lora1_config, lora2_config, is_ground);

interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());


void setup() {
  Serial.begin(115200);

  kernel::setUnitId(unit_id); // unit id を設定

  status_indicator.begin();
  error_indicator.begin();
  error_indicator.set(true);
  // 元のLoRa_ESPと同じモジュールID・起動設定。
  if (!kernel::begin(module_id, false)) return;
  status_indicator.blink_on_change();

  // Core機能を起動
#if LORA_USE_CAN
  can_bus.begin();
#endif
  serial_bus.begin();

  delay(1000);

  // コンポーネントを起動
  if (!dual_lora.begin()) {
    dual_lora.error("lrST", "Dual LoRa task start failed");
    return;
  }
  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  status_indicator.update();
  error_indicator.update();
  delay(1);
}
