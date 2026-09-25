#pragma once

#include <library/wcpp/cpp/packet.h>

namespace component {

// Communication test only: this protocol has no actuator operations.
class SeparationAckProtocol {
public:
    static constexpr uint8_t component_id = 0x26;
    static constexpr uint8_t command_id = 'q';
    static constexpr uint8_t ack_id = 'a';
    static constexpr uint8_t separation_unit_id = 0x41;
    static constexpr uint8_t ground_unit_id = 0x64;
    static constexpr unsigned cache_size = 16;

    static bool accepts(const wcpp::Packet& command) {
        // The only valid payload is empty. In particular, On is not supported.
        return command && command.size() == 7 && command.isRemote()
            && command.isCommand() && command.component_id() == component_id
            && command.packet_id() == command_id
            && command.origin_unit_id() == ground_unit_id
            && command.dest_unit_id() == separation_unit_id;
    }

    bool reply(const wcpp::Packet& command, wcpp::Packet& ack) {
        if (!accepts(command) || !ack || ack.size() != 0 || ack.size_remain() < 32)
            return false;
        bool duplicate = false;
        for (unsigned i = 0; i < count_; ++i) {
            if (sequences_[i] == command.sequence()) duplicate = true;
        }
        ack.telemetry(ack_id, component_id, separation_unit_id,
                      command.origin_unit_id(), command.sequence());
        if (!(ack.append("Ri").setInt(command_id)
            && ack.append("Sq").setInt(command.sequence())
            && ack.append("St").setInt(0)  // Accepted test, not separation success.
            && ack.append("Dp").setInt(duplicate ? 1 : 0))) return false;
        if (!duplicate) {
            sequences_[next_] = command.sequence();
            next_ = (next_ + 1) % cache_size;
            if (count_ < cache_size) ++count_;
        }
        return true;
    }

private:
    uint16_t sequences_[cache_size] = {};
    unsigned count_ = 0;
    unsigned next_ = 0;
};

}  // namespace component
