#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <vector>

using byte = uint8_t;
constexpr int INPUT = 0, OUTPUT = 1, HIGH = 1, LOW = 0, SERIAL_8N1 = 0;
extern uint32_t test_ms;
extern bool test_aux_high;
extern unsigned test_aux_reads_until_busy;
inline uint32_t millis() { return test_ms; }
inline void delay(unsigned ms) { test_ms += ms; }
inline void pinMode(uint8_t, int) {}
inline void digitalWrite(uint8_t, int) {}
inline int digitalRead(uint8_t) {
    if (test_aux_reads_until_busy && --test_aux_reads_until_busy == 0) test_aux_high = false;
    return test_aux_high;
}
class Stream {
public:
    std::deque<uint8_t> rx;
    std::vector<std::vector<uint8_t>> tx;
    bool config = false;
    int available() { return rx.size(); }
    int peek() { return rx.empty() ? -1 : rx.front(); }
    int read() { if (rx.empty()) return -1; auto b = rx.front(); rx.pop_front(); return b; }
    unsigned readBytes(uint8_t* buf, unsigned size) {
        unsigned n = 0;
        while (n < size && available()) buf[n++] = read();
        return n;
    }
    unsigned write(const uint8_t* data, unsigned size) {
        if (config && size >= 3) {
            rx.push_back(0xC1); rx.push_back(data[1]); rx.push_back(data[2]);
            for (unsigned i = 0; i < data[2]; ++i) rx.push_back(size == 3 ? 0 : data[3 + i]);
        } else tx.emplace_back(data, data + size);
        return size;
    }
    void flush() {}
};
class HardwareSerial : public Stream {
public:
    explicit HardwareSerial(unsigned) {}
    void begin(unsigned, int = 0, int = 0, int = 0) {}
};
