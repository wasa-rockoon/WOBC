#pragma once

#include "library/common.h"

namespace driver {

class KVS_IF {
public:
  using key_t = uint16_t;

  KVS_IF() {};

  virtual bool begin() = 0;

  virtual bool exists(key_t key) = 0;
  virtual uint8_t sizeOf(key_t key) = 0;
  virtual uint8_t read(key_t key, uint8_t* data) = 0;
  virtual bool write(key_t key, const uint8_t* data, uint8_t size) = 0;
  virtual bool clear(key_t key) = 0;
  virtual bool clearAll() = 0;
};

}