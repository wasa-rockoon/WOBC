#pragma once

// Minimal host-only Arduino interface used by test_e220.cpp.
#include <cstddef>
#include <cstdint>
#include <cstring>

constexpr int INPUT = 0;
constexpr int OUTPUT = 1;
constexpr int LOW = 0;
constexpr int HIGH = 1;

unsigned long millis();
void delay(unsigned long milliseconds);
void pinMode(uint8_t pin, int mode);
void digitalWrite(uint8_t pin, int value);
int digitalRead(uint8_t pin);

class Stream {
public:
  virtual ~Stream() = default;
  virtual int available() = 0;
  virtual int peek() = 0;
  virtual int read() = 0;
  virtual size_t write(const uint8_t* data, size_t length) = 0;
  virtual void flush() = 0;

  size_t readBytes(uint8_t* data, size_t length) {
    size_t count = 0;
    while (count < length && available() > 0) {
      data[count++] = static_cast<uint8_t>(read());
    }
    return count;
  }
};
