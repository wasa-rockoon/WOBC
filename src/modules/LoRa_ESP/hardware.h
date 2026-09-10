#pragma once

#include <library/common.h>

namespace lora_esp_hardware {

// Board: LoRa ESP, MCU: ESP32-S3-WROOM-1, CAN transceiver: SN65HVD230.
namespace can {
constexpr pin_t rx = 44;
constexpr pin_t tx = 43;
}

namespace lora1 {
constexpr pin_t uart_tx = 13;
constexpr pin_t uart_rx = 12;
constexpr pin_t aux = 11;
constexpr pin_t m0 = 14;
constexpr pin_t m1 = 21;
constexpr pin_t rf_switch_1 = 39;
constexpr pin_t rf_switch_2 = 40;
}

namespace lora2 {
constexpr pin_t uart_tx = 7;
constexpr pin_t uart_rx = 18;
constexpr pin_t aux = 8;
constexpr pin_t m0 = 5;
constexpr pin_t m1 = 6;
constexpr pin_t rf_switch_1 = 9;
constexpr pin_t rf_switch_2 = 10;
}

namespace indicator {
constexpr pin_t error = 41;
constexpr pin_t status = 42;
constexpr uint8_t active_level = HIGH;
}

}  // namespace lora_esp_hardware
