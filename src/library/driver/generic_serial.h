#pragma once

#include "library/common.h"

namespace driver {

class GenericSerialClass: public Stream {
public:
  virtual void begin(unsigned baud) = 0;
  virtual void end() = 0;
};

template <class SerialClass>
class GenericSerial: public GenericSerialClass {
public:
  GenericSerial(SerialClass& serial): serial_(serial) {}

  inline void begin(unsigned baud) override { serial_.begin(baud); }
  inline void end() override { serial_.end(); }

  inline int available() override { return serial_.available(); }
  inline int read() override { return serial_.read(); }
  inline int peek() override { return serial_.peek(); }
  inline size_t write(uint8_t c) override { return serial_.write(c); }
  inline size_t write(const uint8_t *buffer, size_t size) override { return serial_.write(buffer, size); };

protected:
  SerialClass& serial_;
};

}
