#include "separation.h"

namespace component {

Separation::Separation(const Config& config)
    : process::Component("Separation", component_id), config_(config),
      normal_indicator_(config.normal_led) {}

void Separation::initializeOutputsSafe() {
  // Preload LOW before enabling the output driver, then enforce it again.
  // GPIO48 has no external pull-down, so its state before this code runs is a
  // hardware verification item; after this function both outputs are safe.
  digitalWrite(config_.high_side, config_.safe_level);
  digitalWrite(config_.low_side, config_.safe_level);
  pinMode(config_.high_side, OUTPUT);
  pinMode(config_.low_side, OUTPUT);
  digitalWrite(config_.high_side, config_.safe_level);
  digitalWrite(config_.low_side, config_.safe_level);
}

void Separation::setup() {
  // Preserve the former command-free board check without keeping indicator
  // lifecycle code in the module main file.
  normal_indicator_.begin();
  normal_indicator_.blink(250);
}

void Separation::loop() {
  normal_indicator_.update();
}

void Separation::onCommand(const wcpp::Packet& command) {
  // Commands for another unit, local commands, and self-originated packets
  // must never operate this board.
  if (!command.isRemote() || command.dest_unit_id() != config_.unit_id ||
      command.origin_unit_id() == wcpp::unit_id_local) return;

  bool payload_valid = false;
  bool requested_on = false;
  if (command.packet_id() == normal_led_command_id) {
    const auto on = command.find("On");
    if (on != command.end() && (*on).isInt()) {
      const int value = (*on).getInt();
      payload_valid = value == 0 || value == 1;
      requested_on = value == 1;
    }
  }

  const Result result = handleCommand(command, payload_valid, requested_on);
  if (result.change_led) normal_indicator_.set(result.led_on);

  wcpp::Packet ack = newPacket(48);
  if (ack && buildAck(ack, command, result)) sendPacket(ack);
}

Separation::Result Separation::handleCommand(const wcpp::Packet& command,
                                              bool payload_valid,
                                              bool requested_on) {
  if (!command.isCommand() || command.component_id() != component_id)
    return {Status::rejected, false, false, false};

  const CommandKey key{command.origin_unit_id(), command.packet_id(), command.sequence()};
  Result cached{};
  if (isDuplicate(key, cached)) {
    cached.change_led = false;
    cached.duplicate = true;
    return cached;
  }

  Result result{Status::rejected, false, false, false};
  if (command.packet_id() == normal_led_command_id) {
    result = payload_valid ? Result{Status::executed, true, requested_on, false}
                           : Result{Status::invalid, false, false, false};
  }
  remember(key, result);
  return result;
}

bool Separation::isDuplicate(const CommandKey& key, Result& result) const {
  for (unsigned i = 0; i < duplicate_cache_size; ++i) {
    if (valid_[i] && keys_[i].source == key.source &&
        keys_[i].command_id == key.command_id && keys_[i].sequence == key.sequence) {
      result = results_[i];
      return true;
    }
  }
  return false;
}

void Separation::remember(const CommandKey& key, const Result& result) {
  keys_[next_] = key;
  results_[next_] = result;
  valid_[next_] = true;
  next_ = (next_ + 1) % duplicate_cache_size;
}

bool Separation::buildAck(wcpp::Packet& ack, const wcpp::Packet& command,
                          const Result& result) const {
  ack.telemetry(ack_packet_id, component_id, config_.unit_id,
                command.origin_unit_id(), command.sequence());
  return ack.append("Ri").setInt(command.packet_id()) &&
         ack.append("Sq").setInt(command.sequence()) &&
         ack.append("St").setEnum(static_cast<uint8_t>(result.status)) &&
         ack.append("Dp").setInt(result.duplicate);
}

}  // namespace component
