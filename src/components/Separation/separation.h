#pragma once

#include <library/wobc.h>

namespace component {

// Hardware values belong to the board-specific module. This component owns
// the safe output initialization and Separation command/ACK protocol.
class Separation : public process::Component {
 public:
  static constexpr uint8_t component_id = 0x00;
  static constexpr uint8_t normal_led_command_id = 'n';
  static constexpr uint8_t ack_packet_id = 'a';

  struct Config {
    uint8_t unit_id;
    pin_t normal_led;
    pin_t high_side;
    pin_t low_side;
    uint8_t safe_level;
  };

  explicit Separation(const Config& config);
  void initializeOutputsSafe();

 protected:
  void setup() override;
  void loop() override;
  void onCommand(const wcpp::Packet& command) override;

 private:
  enum class Status : uint8_t { accepted = 0, executed = 1, rejected = 2,
                                duplicate = 3, invalid = 4 };
  struct CommandKey { uint8_t source; uint8_t command_id; uint16_t sequence; };
  struct Result { Status status; bool change_led; bool led_on; bool duplicate; };

  static constexpr unsigned duplicate_cache_size = 8;
  const Config config_;
  interface::Indicator normal_indicator_;
  CommandKey keys_[duplicate_cache_size]{};
  Result results_[duplicate_cache_size]{};
  bool valid_[duplicate_cache_size]{};
  unsigned next_ = 0;

  Result handleCommand(const wcpp::Packet& command, bool payload_valid,
                       bool requested_on);
  bool isDuplicate(const CommandKey& key, Result& result) const;
  void remember(const CommandKey& key, const Result& result);
  bool buildAck(wcpp::Packet& ack, const wcpp::Packet& command,
                const Result& result) const;
};

}  // namespace component
