#include <library/wobc.h>
#include "hardware.h"
#include "protocol.h"

constexpr uint8_t module_id = 'S';  // Verified module ID: ASCII 'S' (0x53).
constexpr uint8_t unit_id = 0x41;

core::CANBus can_bus(separation_hardware::can::rx,
                     separation_hardware::can::tx);
core::SerialBus serial_bus(Serial);

interface::WatchIndicator<unsigned> status_indicator(
    separation_hardware::indicator::status, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(
    separation_hardware::indicator::error, kernel::errorCount());
interface::Indicator normal_indicator(separation_hardware::separation::normal_led);

class SeparationNode : public process::Component {
public:
  SeparationNode() : process::Component("Separation", separation_protocol::component_id) {}

protected:
  void onCommand(const wcpp::Packet& command) override {
    // Commands for another unit, local commands, and broadcast are ignored.
    if (!command.isRemote() || command.dest_unit_id() != unit_id ||
        command.origin_unit_id() == wcpp::unit_id_local) return;

    bool payload_valid = false;
    bool requested_on = false;
    if (command.packet_id() == separation_protocol::normal_led_command_id) {
      auto on = command.find("On");
      if (on != command.end() && (*on).isInt()) {
        const int value = (*on).getInt();
        payload_valid = value == 0 || value == 1;
        requested_on = value == 1;
      }
    }

    const separation_protocol::Result result = handler_.handle(
        command.isCommand(), command.isRemote(), command.component_id(),
        command.origin_unit_id(), command.dest_unit_id(), command.sequence(),
        command.packet_id(), payload_valid, requested_on);

    if (result.change_led) normal_indicator.set(result.led_on);

    wcpp::Packet ack = newPacket(48);
    if (ack && separation_protocol::buildAck(ack, command, result)) {
      sendPacket(ack);
    }
  }

private:
  separation_protocol::Handler handler_;
} separation_node;

void initializeSeparationOutputsSafe() {
  // Preload LOW before enabling the output driver, then enforce it again.
  // GPIO47 also has a 10 kOhm board pull-down. GPIO48 has no equivalent
  // explicit pull-down, so cold-boot behavior before setup remains a hardware
  // verification item.
  digitalWrite(separation_hardware::separation::high_side,
               separation_hardware::separation::safe_level);
  digitalWrite(separation_hardware::separation::low_side,
               separation_hardware::separation::safe_level);
  pinMode(separation_hardware::separation::high_side, OUTPUT);
  pinMode(separation_hardware::separation::low_side, OUTPUT);
  digitalWrite(separation_hardware::separation::high_side,
               separation_hardware::separation::safe_level);
  digitalWrite(separation_hardware::separation::low_side,
               separation_hardware::separation::safe_level);
}

void setup() {
  initializeSeparationOutputsSafe();

  Serial.begin(115200);
  delay(1000);

  kernel::setUnitId(unit_id);
  if (!kernel::begin(module_id, true)) return;

  can_bus.begin();
  serial_bus.begin();

  status_indicator.begin();
  status_indicator.blink_on_change();

  error_indicator.begin();
  error_indicator.set(false);
  error_indicator.blink_on_change(100);

  // Local, command-free board check. GPIO14 lights briefly after bring-up.
  normal_indicator.begin();
  normal_indicator.blink(250);

  separation_node.begin();
}

void loop() {
  status_indicator.update();
  error_indicator.update();
  normal_indicator.update();
}
