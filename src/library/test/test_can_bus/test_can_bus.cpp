// Build with the stubs include directory before src (see README.md).
#include "../../core/can_bus.cpp"
#include "../../wcpp/cpp/Packet.cpp"
#include "../../wcpp/cpp/float16.cpp"

#include <cstdio>
#include <cstdlib>

driver::CAN* driver::CAN::active = nullptr;

namespace {
void require(bool condition, const char* expression, unsigned line) {
  if (!condition) {
    std::fprintf(stderr, "FAIL line %u: %s\n", line, expression);
    std::exit(EXIT_FAILURE);
  }
}
#define CHECK(expression) require((expression), #expression, __LINE__)

void roundTrip(const wcpp::Packet& packet, unsigned expected_frames) {
  core::CANBus bus(44, 43);
  bus.begin();
  driver::CAN& can = *driver::CAN::active;
  bus.enqueue(packet);
  bus.step();
  CHECK(can.transmitted.size() == expected_frames);
  for (const auto& frame : can.transmitted) {
    CHECK(frame.extended);
    CHECK(!frame.rtr);
    CHECK(frame.id <= 0x1FFFFFFF);
    CHECK(frame.length >= 1 && frame.length <= 8);
    can.inject(frame);
    bus.step();
  }
  CHECK(bus.errors == 0);
  CHECK(bus.delivered.size() == 1);
  const wcpp::Packet received = wcpp::Packet::decode(bus.delivered.front().data());
  CHECK(received.isTelemetry() == packet.isTelemetry());
  CHECK(received.isCommand() == packet.isCommand());
  CHECK(received.packet_id() == packet.packet_id());
  CHECK(received.component_id() == packet.component_id());
  CHECK(received.origin_unit_id() == packet.origin_unit_id());
  CHECK(received.dest_unit_id() == packet.dest_unit_id());
  CHECK(received.size() == packet.size());
  CHECK(std::memcmp(received.encode(), packet.encode(), packet.size()) == 0);
  CHECK(can.transmitted.size() == expected_frames);
}
} // namespace

int main() {
  uint8_t gps_buffer[64];
  wcpp::Packet gps = wcpp::Packet::empty(gps_buffer, sizeof(gps_buffer));
  gps.telemetry('M', 0x15, 0x61, 0xFF, 1234);
  CHECK(gps.append("LA").setFloat32(35.681236f));
  CHECK(gps.append("LO").setFloat32(139.767125f));
  CHECK(gps.append("AL").setInt(-12));
  CHECK(gps.append("Ss").setInt(-90));
  roundTrip(gps, 3);

  uint8_t local_buffer[16];
  wcpp::Packet local = wcpp::Packet::empty(local_buffer, sizeof(local_buffer));
  local.telemetry('M', 0x15);
  CHECK(local.append("AL").setInt(12));
  roundTrip(local, 1);

  uint8_t command_buffer[16];
  wcpp::Packet command = wcpp::Packet::empty(command_buffer, sizeof(command_buffer));
  command.command('s', 0x10, 0x64, 0x61, 42);
  roundTrip(command, 1);
  std::puts("PASS: CAN preserves GPS telemetry, local telemetry, and commands");
}
