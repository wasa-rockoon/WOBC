#pragma once

#include <library/common.h>

namespace separation_hardware {

// Board: Separation, MCU: ESP32-S3-WROOM-1, CAN transceiver: SN65HVD230.
namespace can {
constexpr pin_t rx = 44;
constexpr pin_t tx = 43;
}

namespace i2c {
constexpr pin_t sda = 17;
constexpr pin_t scl = 16;
constexpr uint8_t rtc_address = 0x51;              // PCF8563
constexpr uint8_t current_monitor_address = 0x4D;  // INA226
constexpr float current_shunt_ohms = 0.020f;
}

namespace micro_sd {
constexpr pin_t sck = 12;
constexpr pin_t mosi = 13;
constexpr pin_t miso = 11;
constexpr pin_t cs = 9;
constexpr pin_t detect = 10;
}

namespace gps {
constexpr pin_t rx = 38;  // MCU RX, connected to GPS_RTX.
constexpr pin_t tx = 39;  // MCU TX, connected to GPS_TRX.
}

namespace flight_pin {
constexpr pin_t detect = 2;
}

namespace launch_detection {
constexpr pin_t detect = 8;
}

namespace separation {
constexpr pin_t normal_led = 14;
constexpr pin_t high_side = 48;
constexpr pin_t low_side = 47;
constexpr uint8_t active_level = HIGH;
constexpr uint8_t safe_level = LOW;
}

namespace indicator {
constexpr pin_t error = 41;
constexpr pin_t status = 42;
constexpr uint8_t active_level = HIGH;
}

}  // namespace separation_hardware
