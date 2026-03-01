#include <library/wobc.h>
#include <components/LiPoPower/lipo_power.h>
#include <components/LoRa/lora.h>
#include <components/Pressure/pressure.h>
#include <components/IMU/IMU.h>
#include <components/GPS/gps.h>
#include <components/Logger/logger.h>
#include <SPI.h>

constexpr uint8_t module_id = 0x4E;  // GOLIDEN module ID
constexpr uint8_t unit_id = 0x63;

core::SerialBus serial_bus(Serial);

component::IMU9 imu(Wire, unit_id, 100);

void setup() {
  Serial.begin(115200);
  kernel::setUnitId(unit_id);
  if (!kernel::begin(module_id, true)) return;

  Wire.begin(17, 16);  // SDA=17, SCL=16
  serial_bus.begin();

  delay(1000);

  imu.begin();
  
  Serial.println("GOLIDEN system initialized");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}