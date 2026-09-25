#include <components/Separation/SeparationAckProtocol.h>
#include <cassert>
#include <cstdio>
#include <cstring>

using Protocol = component::SeparationAckProtocol;

int main() {
    Protocol protocol;
    uint8_t command_buffer[64] = {}, ack_buffer[64] = {};
    auto command = wcpp::Packet::empty(command_buffer, sizeof(command_buffer));
    auto ack = wcpp::Packet::empty(ack_buffer, sizeof(ack_buffer));
    auto reset = [&](uint16_t sequence) {
        command.command('q', 0x26, 0x64, 0x41, sequence);
        ack = wcpp::Packet::empty(ack_buffer, sizeof(ack_buffer));
    };
    reset(65535);
    assert(protocol.reply(command, ack));
    assert(ack.isTelemetry() && ack.packet_id() == 'a' && ack.component_id() == 0x26);
    assert(ack.origin_unit_id() == 0x41 && ack.dest_unit_id() == 0x64);
    assert(ack.sequence() == 65535);
    assert((*ack.find("Ri")).getInt() == 'q');
    assert((*ack.find("Sq")).getInt() == 65535);
    assert((*ack.find("St")).getInt() == 0);
    assert((*ack.find("Dp")).getInt() == 0);
    reset(65535);
    assert(protocol.reply(command, ack));
    assert((*ack.find("Dp")).getInt() == 1);
    reset(0);
    assert(protocol.reply(command, ack));
    assert((*ack.find("Dp")).getInt() == 0);

    // Wrong type, command, component, origin, destination, local and short headers.
    for (unsigned field = 0; field < 7; ++field) {
        reset(123);
        switch (field) {
        case 0: command_buffer[1] |= 0x80; break;
        case 1: command_buffer[1] = 'n'; break;
        case 2: command_buffer[2] = 0x24; break;
        case 3: command_buffer[3] = 0x41; break;
        case 4: command_buffer[4] = 0xFF; break;
        case 5: command_buffer[3] = 0; break;
        case 6: command_buffer[0] = 6; break;
        }
        assert(!protocol.reply(command, ack));
        assert(ack.size() == 0);
    }
    reset(123);
    assert(command.append("On").setInt(1));
    assert(!protocol.reply(command, ack));
    assert(!Protocol::accepts(wcpp::Packet::null()));
    reset(123);
    uint8_t small_buffer[8] = {};
    auto small = wcpp::Packet::empty(small_buffer, sizeof(small_buffer));
    assert(!protocol.reply(command, small));
    assert(protocol.reply(command, ack));
    assert((*ack.find("Dp")).getInt() == 0); // Failed reply did not enter cache.

    for (unsigned i = 1; i <= Protocol::cache_size; ++i) {
        reset(i);
        assert(protocol.reply(command, ack));
    }
    reset(123);
    assert(protocol.reply(command, ack));
    assert((*ack.find("Dp")).getInt() == 0); // Bounded cache eviction.

    // CAN wire header mapping used by CANBus: type bit and remote header survive.
    for (bool telemetry : {false, true}) {
        reset(0x1234);
        if (telemetry) command.telemetry('a', 0x26, 0x41, 0x64, 0x1234);
        const uint32_t id = (uint32_t(command.type_and_id()) << 21)
                          | (uint32_t(command.component_id()) << 13)
                          | (uint32_t(command.origin_unit_id()) << 5);
        uint8_t decoded[64] = {};
        decoded[0] = command.size();
        decoded[1] = (id >> 21) & 0xFF;
        decoded[2] = (id >> 13) & 0xFF;
        decoded[3] = (id >> 5) & 0xFF;
        std::memcpy(decoded + 4, command.encode() + 4, command.size() - 4);
        const auto received = wcpp::Packet::decode(decoded);
        assert(received.isTelemetry() == telemetry);
        assert(received.dest_unit_id() == command.dest_unit_id());
        assert(received.sequence() == 0x1234);
    }
    std::puts("Separation ACK protocol tests passed");
}
