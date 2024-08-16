#include "kvs.h"

namespace driver {

bool KVS::begin() {
  return prefs_.begin("KVS", false);
}


bool KVS::exists(key_t key) {
  uint8_t key_str[] = {key & 0xFF, key >> 8, 0};
  return prefs_.isKey((char*)key_str);
}

uint8_t KVS::sizeOf(key_t key) {
  uint8_t key_str[] = {key & 0xFF, key >> 8, 0};
  return prefs_.getBytesLength((char*)key_str);
}

uint8_t KVS::read(key_t key, uint8_t* data)  {
  uint8_t key_str[] = {key & 0xFF, key >> 8, 0};
  uint8_t size = prefs_.getBytesLength((char*)key_str);
  if (!prefs_.getBytes((char*)key_str, data, size)) return 0;
  return size;
}

bool KVS::write(key_t key, const uint8_t* data, uint8_t size) {
  uint8_t key_str[] = {key & 0xFF, key >> 8, 0};
  return prefs_.putBytes((char*)key_str, data, size);
}

bool KVS::clear(key_t key) {
  uint8_t key_str[] = {key & 0xFF, key >> 8, 0};
  return prefs_.remove((char*)key_str);
}
bool KVS::clearAll() {
  return prefs_.clear();
}


}