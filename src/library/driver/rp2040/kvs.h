#pragma once

#include "../kvs_if.h"

#include "EEPROM.h"

namespace driver {

class KVS: public KVS_IF {
  static const unsigned total_size = 4096;

public:
  KVS(): KVS_IF() {};

  bool begin() override;

  bool exists(key_t key) override;
  uint8_t sizeOf(key_t key) override;
  uint8_t read(key_t key, uint8_t* data) override;
  bool write(key_t key, const uint8_t* data, uint8_t size) override;
  bool clear(key_t key) override;
  bool clearAll() override;

protected:
  inline key_t keyAt(unsigned pos) {
    return EEPROM.read(pos + 1) | (EEPROM.read(pos + 2) << 8);
  }
  inline unsigned next(unsigned pos) {
    return pos + sizeAt(pos);
  }
  inline unsigned sizeAt(unsigned pos) {
    return EEPROM.read(pos);
  }
  int getPos(key_t key);
  void clearAt(unsigned pos);
};

}