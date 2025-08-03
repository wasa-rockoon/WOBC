#pragma once

#include "../kvs_if.h"

#include <Preferences.h>

namespace driver {

class KVS: public KVS_IF {
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
  Preferences prefs_;

};

}