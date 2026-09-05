// Compile the production LoRa/E220/Packet sources against deterministic I/O stubs.
#include <components/LoRa/lora.h>
#include <cassert>
#include <cstdio>
#include <limits>

uint32_t test_ms = 0;
bool test_aux_high = true;
unsigned test_aux_reads_until_busy = 0;
namespace kernel { Kernel kernel_; std::map<const uint8_t*, int> references; }

class Radio : public component::LoRa {
public:
    Radio() : LoRa(1, 2, 3, 4, 5, 3) {
        test_ms = 3000; test_aux_high = true; test_aux_reads_until_busy = 0;
        ready_ = true;
        enableTrackerScheduling();
        tick();
    }
    ~Radio() { delete tx_mutex_; }
    void tick(unsigned ms = 0) { test_ms += ms; loop(); }
    Stream& serial() { return lora_serial_; }
    unsigned ackCount() { return ack_queue_.count; }
    unsigned awaitingAck() { return awaiting_ack_; }
    bool telemetryPending() { return bool(telemetry_packet_); }
    void legacy(const wcpp::Packet& p) { onCommand(p); }
    void lock() { xSemaphoreTake(tx_mutex_, 0); }
    void unlock() { xSemaphoreGive(tx_mutex_); }
};
using Priority = component::LoRa::TxPriority;
static wcpp::Packet packet(char id, bool command = false) {
    auto p = kernel::kernel_.allocPacket(64);
    if (command) p.command(id, 0x10);
    else p.telemetry(id, 0);
    return p;
}
static void receive(Stream& serial, const wcpp::Packet& p, bool rssi = false) {
    serial.rx.push_back(p.size() + 1);
    for (unsigned i = 0; i < p.size(); ++i) serial.rx.push_back(p.encode()[i]);
    serial.rx.push_back(p.checksum());
    if (rssi) serial.rx.push_back(180);
}
static char sentId(Radio& r, unsigned n) {
    const auto& frame = r.serial().tx.at(n);
    assert(frame[0] + 1 == frame.size());
    assert(wcpp::Packet::checksum(frame.data() + 1, frame.size() - 2) == frame.back());
    return wcpp::Packet::decode(frame.data() + 1).packet_id();
}
int main() {
    {
        Radio r;
        auto telemetry = packet('M'); auto ack = packet('a');
        assert(r.canSendTelemetry());
        assert(r.queuePacket(telemetry, Priority::Telemetry));
        assert(r.queuePacket(ack, Priority::Ack));
        r.tick();
        assert(sentId(r, 0) == 'a');
        assert(r.telemetryPending());
        r.tick(1000); r.tick(100);
        assert(sentId(r, 1) == 'M');
        puts("PASS ACK precedes already queued telemetry");
    }
    {
        Radio r; auto ack = packet('a');
        test_aux_high = false;
        assert(r.queuePacket(ack, Priority::Ack));
        r.tick(10000);
        assert(r.serial().tx.empty() && r.ackCount() == 1);
        assert(!r.canSendTelemetry());
        test_aux_high = true; r.tick(100);
        assert(sentId(r, 0) == 'a' && r.ackCount() == 0);
        puts("PASS busy beyond old timeout retains ACK and resumes once");
    }
    {
        Radio r; auto telemetry = packet('M'); auto uplink = packet('t', true);
        assert(r.queuePacket(telemetry, Priority::Telemetry));
        assert(r.queuePacket(packet('c', true), Priority::Normal));
        receive(r.serial(), uplink);
        r.tick(); r.tick(500);
        assert(r.serial().tx.empty() && r.awaitingAck() == 1);
        assert(r.published.size() == 1 && r.published[0].find("Ss"));
        auto ack = packet('a');
        assert(r.queuePacket(ack, Priority::Ack));
        r.tick(); assert(sentId(r, 0) == 'a');
        puts("PASS RX-to-ACK scheduling gap blocks telemetry and normal TX");
    }
    {
        Radio r; auto ack = packet('a');
        assert(r.queuePacket(ack, Priority::Ack));
        r.serial().rx.push_back(10); r.serial().rx.push_back(8);
        r.tick(200); assert(r.serial().tx.empty());
        r.serial().rx.clear(); r.tick(100);
        assert(sentId(r, 0) == 'a');
        puts("PASS partial UART frame inhibits TX even with AUX high");
    }
    {
        Radio r; auto ack = packet('a');
        for (unsigned i = 0; i < 8; ++i) assert(r.queuePacket(ack, Priority::Ack));
        assert(!r.queuePacket(ack, Priority::Ack));
        r.tick(); assert(r.ackCount() == 7);
        assert(r.queuePacket(ack, Priority::Ack));
        assert(r.ackCount() == 8);
        r.tick(10); assert(r.serial().tx.size() == 1);
        r.tick(1000); r.tick(100); assert(r.serial().tx.size() == 2);
        puts("PASS queue backpressure and AUX-high TX guard");
    }
    {
        Radio r; auto ack = packet('a');
        assert(r.queuePacket(ack, Priority::Ack));
        test_aux_reads_until_busy = 2;
        r.tick(); assert(r.serial().tx.empty() && r.ackCount() == 1);
        test_aux_high = true; r.tick(100); assert(sentId(r, 0) == 'a');
        r.lock(); assert(!r.queuePacket(ack, Priority::Ack)); assert(!r.canSendTelemetry()); r.unlock();
        puts("PASS final busy recheck retains packet; mutex contention is nonblocking");
    }
    {
        Radio r; auto inner = packet('c', true);
        auto envelope = packet('s', true); envelope.append("Pa").setPacket(inner);
        r.legacy(envelope); envelope.clear(); inner.clear();
        r.tick(); assert(sentId(r, 0) == 'c');
        puts("PASS legacy envelope format and independent queued packet lifetime");
    }
    {
        Radio r; auto telemetry = packet('M');
        assert(r.queuePacket(telemetry, Priority::Telemetry)); r.tick();
        r.tick(1000); r.tick(100); assert(!r.canSendTelemetry());
        r.tick(900); assert(r.canSendTelemetry());
        puts("PASS actual telemetry writes are spaced at least 2 seconds");
    }
    {
        Radio r; auto ack = packet('a');
        test_ms = std::numeric_limits<uint32_t>::max() - 50;
        r.tick(); assert(r.queuePacket(ack, Priority::Ack)); r.tick();
        assert(r.queuePacket(ack, Priority::Ack));
        r.tick(1000); r.tick(100); assert(r.serial().tx.size() == 2);
        puts("PASS millis rollover retains correct TX spacing");
    }
    {
        Radio r;
        r.serial().rx = {5, 99, 0, 0, 0, 0}; r.tick();
        assert(r.published.empty() && r.awaitingAck() == 0);
        puts("PASS malformed frame is rejected before packet decode");
    }
    {
        Stream serial; E220 e(serial, 1, 2, 3);
        serial.config = true; test_aux_high = true;
        assert(e.setRSSIEnable(true)); serial.config = false;
        auto p = packet('M'); uint8_t data[255];
        for (unsigned i = 0; i < 15; ++i) {
            test_ms += 2000;
            receive(serial, p, true);
            assert(e.receive(data, sizeof(data)) == p.size() + 1);
            assert(e.getRSSI() == -76 && serial.available() == 0);
        }
        puts("PASS equal-length RSSI telemetry continues beyond 10 seconds");
    }
    assert(kernel::references.empty());
    puts("11 scenarios passed; no packet allocations retained");
}
