#include "Arduino.h"
#include "../../../components/LoRa/e220.h"
// Keep the host suite self-contained for PlatformIO's native test discovery.
#include "../../../components/LoRa/e220.cpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

namespace {
unsigned long now_ms = 0;
unsigned delay_calls = 0;
bool aux_busy = false;

void require(bool condition, const char* expression, unsigned line) {
  if (!condition) {
    std::fprintf(stderr, "FAIL line %u: %s\n", line, expression);
    std::exit(EXIT_FAILURE);
  }
}
#define CHECK(expression) require((expression), #expression, __LINE__)

class FakeSerial : public Stream {
public:
  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> writes;
  std::array<uint8_t, 9> registers{};
  bool register_mode = false;
  bool acknowledge = true;
  bool corrupt_response = false;

  int available() override { return static_cast<int>(rx.size()); }
  int peek() override { return rx.empty() ? -1 : rx.front(); }
  int read() override {
    if (rx.empty()) return -1;
    const uint8_t value = rx.front();
    rx.pop_front();
    return value;
  }
  void flush() override {}

  size_t write(const uint8_t* data, size_t length) override {
    writes.emplace_back(data, data + length);
    if (!register_mode || !acknowledge || length < 3) return length;
    if (data[0] != 0xC0 && data[0] != 0xC1) return length;
    const unsigned address = data[1];
    const unsigned count = data[2];
    CHECK(address + count <= registers.size());
    if (data[0] == 0xC0) {
      CHECK(length == count + 3);
      std::memcpy(registers.data() + address, data + 3, count);
    } else {
      CHECK(length == 3);
    }
    rx.push_back(corrupt_response ? 0x00 : 0xC1);
    rx.push_back(data[1]);
    rx.push_back(data[2]);
    for (unsigned i = 0; i < count; ++i) rx.push_back(registers[address + i]);
    return length;
  }

  void frame(const std::vector<uint8_t>& payload, bool with_rssi = true,
             uint8_t rssi = 200) {
    CHECK(payload.size() <= 255);
    rx.push_back(static_cast<uint8_t>(payload.size()));
    rx.insert(rx.end(), payload.begin(), payload.end());
    if (with_rssi) rx.push_back(rssi);
  }
};

struct Fixture {
  FakeSerial serial;
  E220 radio;
  Fixture() : radio(serial, 1, 2, 3) {
    now_ms = 100;
    delay_calls = 0;
    aux_busy = false;
  }
  void enableRssi() {
    serial.register_mode = true;
    CHECK(radio.setRSSIEnable(true));
    serial.register_mode = false;
    serial.writes.clear();
  }
};

void test_default_registers() {
  Fixture fixture;
  fixture.serial.register_mode = true;
  CHECK(fixture.radio.setParametersToDefault());
  const std::vector<uint8_t> expected = {
    0xC0, 0x00, 0x08, 0x00, 0x00, 0x62, 0x00, 0x0F, 0x03, 0x00, 0x00
  };
  CHECK(fixture.serial.writes.size() == 1);
  CHECK(fixture.serial.writes[0] == expected);
  CHECK(fixture.serial.rx.empty());
}

void test_register_failures_and_bounds() {
  Fixture fixture;
  fixture.serial.register_mode = true;
  fixture.serial.acknowledge = false;
  CHECK(!fixture.radio.setSerialBaudRate(115200));
  // A failed read must not be followed by a write with uninitialized data.
  CHECK(fixture.serial.writes.size() == 1);
  CHECK(fixture.serial.writes[0][0] == 0xC1);
  fixture.serial.writes.clear();
  CHECK(!fixture.radio.setParametersToDefault());
  CHECK(fixture.serial.writes.size() == 1);

  fixture.serial.writes.clear();
  std::array<uint8_t, 16> parameters{};
  CHECK(!fixture.radio.writeRegister(E220::ADDR::ADDH, parameters.data(), 9));
  CHECK(!fixture.radio.readRegister(E220::ADDR::ADDH, parameters.data(), 14));
  CHECK(fixture.serial.writes.empty());

  fixture.serial.acknowledge = true;
  fixture.serial.corrupt_response = true;
  CHECK(!fixture.radio.setParametersToDefault());
}

void test_transmit_length_boundary() {
  Fixture fixture;
  std::array<uint8_t, 256> payload{};
  for (unsigned i = 0; i < payload.size(); ++i) payload[i] = i;
  CHECK(fixture.radio.sendTransparent(payload.data(), 255));
  CHECK(fixture.serial.writes.size() == 1);
  CHECK(fixture.serial.writes[0].size() == 256);
  CHECK(fixture.serial.writes[0][0] == 255);
  CHECK(std::memcmp(fixture.serial.writes[0].data() + 1, payload.data(), 255) == 0);
  CHECK(!fixture.radio.sendTransparent(payload.data(), 256));
  CHECK(fixture.serial.writes.size() == 1);

  CHECK(fixture.radio.send(0x1234, 3, payload.data(), 255));
  CHECK(fixture.serial.writes[1].size() == 259);
  CHECK(fixture.serial.writes[1][0] == 0x12);
  CHECK(fixture.serial.writes[1][1] == 0x34);
  CHECK(fixture.serial.writes[1][2] == 3);
  CHECK(fixture.serial.writes[1][3] == 255);
  CHECK(!fixture.radio.send(0x1234, 3, payload.data(), 256));
  CHECK(fixture.serial.writes.size() == 2);
}

void test_receive_maximum_and_rssi() {
  Fixture fixture;
  fixture.enableRssi();
  std::vector<uint8_t> payload(255);
  for (unsigned i = 0; i < payload.size(); ++i) payload[i] = i;
  fixture.serial.frame(payload, true, 200);
  CHECK(fixture.serial.available() == 257);
  std::array<uint8_t, 257> guarded{};
  guarded.front() = 0xA5;
  guarded.back() = 0x5A;
  CHECK(fixture.radio.receive(guarded.data() + 1, 255) == 255);
  CHECK(std::memcmp(guarded.data() + 1, payload.data(), 255) == 0);
  CHECK(guarded.front() == 0xA5 && guarded.back() == 0x5A);
  CHECK(fixture.radio.getRSSI() == -56);
  CHECK(fixture.serial.rx.empty());

  fixture.serial.frame(payload);
  guarded.fill(0xA5);
  CHECK(fixture.radio.receive(guarded.data() + 1, 254) == 0);
  for (uint8_t byte : guarded) CHECK(byte == 0xA5);
  CHECK(fixture.serial.rx.empty());
}

void test_receive_same_length_after_idle() {
  Fixture fixture;
  fixture.enableRssi();
  uint8_t received[4]{};
  fixture.serial.frame({1, 2, 3, 4});
  CHECK(fixture.radio.receive(received, sizeof(received)) == 4);
  now_ms += E220_RECEIVE_TIMEOUT_MS + 1;
  fixture.serial.frame({5, 6, 7, 8}, true, 0);
  CHECK(fixture.radio.receive(received, sizeof(received)) == 4);
  CHECK(received[0] == 5 && received[3] == 8);
  CHECK(fixture.radio.getRSSI() == -256);
}

void test_receive_fragmentation_and_timeout() {
  Fixture fixture;
  fixture.enableRssi();
  uint8_t received[4]{};
  fixture.serial.rx = {4, 1, 2};
  CHECK(fixture.radio.receive(received, sizeof(received)) == 0);
  fixture.serial.rx.insert(fixture.serial.rx.end(), {3, 4, 200});
  CHECK(fixture.radio.receive(received, sizeof(received)) == 4);
  CHECK(received[0] == 1 && received[3] == 4);

  fixture.serial.rx = {4, 1};
  CHECK(fixture.radio.receive(received, sizeof(received)) == 0);
  now_ms += E220_RECEIVE_TIMEOUT_MS + 1;
  CHECK(fixture.radio.receive(received, sizeof(received)) == 0);
  CHECK(fixture.serial.rx.empty());
  fixture.serial.frame({5, 6, 7, 8});
  CHECK(fixture.radio.receive(received, sizeof(received)) == 4);
}

void test_busy_timeout_yields() {
  Fixture fixture;
  aux_busy = true;
  CHECK(!fixture.radio.setMode(E220::Mode::NORMAL));
  CHECK(delay_calls > 0);
  CHECK(now_ms < 10000);
}
} // namespace

// Advance on reads too so a regression to a busy spin fails rather than hangs.
unsigned long millis() { return now_ms++; }
void delay(unsigned long milliseconds) { now_ms += milliseconds; ++delay_calls; }
void pinMode(uint8_t, int) {}
void digitalWrite(uint8_t, int) {}
int digitalRead(uint8_t) { return aux_busy ? LOW : HIGH; }

int main() {
  test_default_registers();
  test_register_failures_and_bounds();
  test_transmit_length_boundary();
  test_receive_maximum_and_rssi();
  test_receive_same_length_after_idle();
  test_receive_fragmentation_and_timeout();
  test_busy_timeout_yields();
  std::puts("PASS: 7 E220 regression tests");
  return EXIT_SUCCESS;
}
