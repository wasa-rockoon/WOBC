#include "src/components/IGN/IGNGPSTelemetry.h"
#include "src/components/IGN/IGNStartGate.h"
#include <cassert>
#include <cstdio>

int main() {
  uint8_t buffer[64] = {};
  auto packet = wcpp::Packet::empty(buffer, sizeof(buffer));
  // Existing MissionBus sender: 64-byte capacity, LA/LO/AL/UT only.
  packet.telemetry('M', 21, 0x62, 0xFF, 1);
  assert(packet.append("LA").setFloat64(35.0));
  assert(packet.append("LO").setFloat64(139.0));
  assert(packet.append("AL").setInt(10000));
  assert(packet.append("UT").setString("2026-09-16 12:34:56.78"));
  int64_t altitude = 0;
  uint64_t utc = 0;
  assert(component::decodeIGNGPS(packet, altitude, utc));
  assert(altitude == 10000 && utc == 2026091612345678ULL);

  component::IGNStartGate gate(15000);
  gate.setFlightPinRemoved(true, 0);
  gate.observeGPS(true, altitude, 1, utc, 3600000);
  assert(gate.ready(3600000, 3600000));
  // Continuous re-sends with a frozen UT cannot extend eligibility.
  for (uint32_t now = 3601000; now <= 3606000; now += 1000) {
    assert(component::decodeIGNGPS(packet, altitude, utc));
    gate.observeGPS(true, altitude, 1, utc, now);
  }
  assert(!gate.ready(3606000, 3600000));

  assert((*packet.find("UT")).setString("2026-09-16 12:35:02.78"));
  assert(component::decodeIGNGPS(packet, altitude, utc));
  gate.observeGPS(true, altitude, 1, utc, 3606001);
  assert(gate.ready(3606001, 3600000));

  assert((*packet.find("UT")).setString("2000-00-00 00:00:00.00"));
  assert(!component::decodeIGNGPS(packet, altitude, utc));
  assert((*packet.find("UT")).setString("2026-09-16 12:35:02.78x"));
  assert(!component::decodeIGNGPS(packet, altitude, utc));
  assert((*packet.find("UT")).setInt(123));
  assert(!component::decodeIGNGPS(packet, altitude, utc));
  assert((*packet.find("UT")).setString("2026-09-16 12:35:02.78"));
  assert((*packet.find("AL")).setFloat32(10000.0f));
  assert(!component::decodeIGNGPS(packet, altitude, utc));
  assert((*packet.find("AL")).setNull());
  assert(!component::decodeIGNGPS(packet, altitude, utc));
  (*packet.find("AL")).remove();
  assert(!component::decodeIGNGPS(packet, altitude, utc));
  std::puts("IGN existing GPS telemetry compatibility tests passed");
}
