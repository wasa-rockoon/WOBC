#pragma once

#include <stdint.h>
#include <library/wcpp/cpp/packet.h>

namespace separation_protocol {

constexpr uint8_t component_id = 0x00;
constexpr uint8_t normal_led_command_id = 'n';
constexpr uint8_t ack_packet_id = 'a';
constexpr unsigned duplicate_cache_size = 8;

enum class Status : uint8_t {
  accepted = 0,
  executed = 1,
  rejected = 2,
  duplicate = 3,
  invalid = 4,
};

struct CommandKey {
  uint8_t source;
  uint8_t command_id;
  uint16_t sequence;
};

struct Result {
  Status status;
  bool change_led;
  bool led_on;
  bool duplicate;
};

class Handler {
public:
  Result handle(bool is_command, bool is_remote, uint8_t component,
                uint8_t source, uint8_t destination, uint16_t sequence,
                uint8_t command_id, bool payload_valid, bool requested_on) {
    if (!is_command || !is_remote || component != component_id ||
        destination != separation_unit_id || source == wcpp::unit_id_local) {
      return {Status::rejected, false, false, false};
    }

    CommandKey key{source, command_id, sequence};
    for (unsigned i = 0; i < duplicate_cache_size; ++i) {
      if (valid_[i] && equal(keys_[i], key)) {
        Result replay = results_[i];
        replay.change_led = false;
        replay.duplicate = true;
        return replay;
      }
    }

    Result result{Status::rejected, false, false, false};
    if (command_id == normal_led_command_id) {
      result = payload_valid
          ? Result{Status::executed, true, requested_on, false}
          : Result{Status::invalid, false, false, false};
    }
    remember(key, result);
    return result;
  }

  static constexpr uint8_t separation_unit_id = 0x41;

private:
  static bool equal(const CommandKey& a, const CommandKey& b) {
    return a.source == b.source && a.command_id == b.command_id &&
           a.sequence == b.sequence;
  }

  void remember(const CommandKey& key, const Result& result) {
    keys_[next_] = key;
    results_[next_] = result;
    valid_[next_] = true;
    next_ = (next_ + 1) % duplicate_cache_size;
  }

  CommandKey keys_[duplicate_cache_size]{};
  Result results_[duplicate_cache_size]{};
  bool valid_[duplicate_cache_size]{};
  unsigned next_ = 0;
};

inline bool buildAck(wcpp::Packet& ack, const wcpp::Packet& command,
                     const Result& result) {
  ack.telemetry(ack_packet_id, component_id, Handler::separation_unit_id,
                command.origin_unit_id(), command.sequence());
  return ack.append("Ri").setInt(command.packet_id()) &&
         ack.append("Sq").setInt(command.sequence()) &&
         ack.append("St").setEnum(static_cast<uint8_t>(result.status)) &&
         ack.append("Dp").setInt(result.duplicate);
}

}  // namespace separation_protocol
