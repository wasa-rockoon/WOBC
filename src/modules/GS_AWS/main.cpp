// GS_AWS for feature/packetsend.
// Keep the GS CAN, SerialBus and SD logger behavior unchanged; replace only
// the legacy Telemeter component with the AWS forwarder.

#include <library/wobc.h>
#include <components/Logger/logger.h>
#include <SPI.h>

#include "aws_forwarder.h"

#define SPI0_SCK_PIN 12
#define SPI0_MOSI_PIN 11
#define SPI0_MISO_PIN 13
#define SPI0_CS_PIN 10
#define SD_INSERTED_PIN 9

constexpr uint8_t module_id = 0x47;
constexpr uint8_t unit_id = 0x64;

core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
AwsForwarder aws_forwarder;

void setup() {
  Serial.begin(115200);
  Serial0.setPins(2, 1);

  kernel::setUnitId(unit_id);
  if (!kernel::begin(module_id, true)) return;

  SPI.begin(SPI0_SCK_PIN, SPI0_MISO_PIN, SPI0_MOSI_PIN, SPI0_CS_PIN);

  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  delay(1000);

  // Preserve the feature/packetsend GS startup order.
  can_bus.begin();
  serial_bus.begin();
  logger.begin();
  aws_forwarder.begin();

  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  status_indicator.update();
  error_indicator.update();
}
