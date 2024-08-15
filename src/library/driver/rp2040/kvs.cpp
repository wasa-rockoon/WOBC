#include "kvs.h"

namespace driver {

bool KVS::begin() {
  EEPROM.begin(total_size);
  return true;
}


bool KVS::exists(key_t key) {
  return getPos(key) >= 0;
}

uint8_t KVS::sizeOf(key_t key) {
  int pos = getPos(key);
  if (pos < 0) return 0;
  return sizeAt(pos);
}

uint8_t KVS::read(key_t key, uint8_t* data)  {
  int pos = getPos(key);
  if (pos < 0) return 0;
  uint8_t size = sizeAt(pos);
  for (int i = 0; i < size; i++) data[i] = EEPROM.read(pos + 3 + i);
  return size;
}

bool KVS::write(key_t key, const uint8_t* data, uint8_t size) {
  int pos = getPos(key);
  if (pos >= 0) clearAt(pos);
  else pos = 0;
  while (sizeAt(pos) != 0) {
    if (pos >= total_size) return false;
    pos = next(pos);
  }
  if (pos + size + 3 >= total_size) return false;
  EEPROM.write(pos,     size);
  EEPROM.write(pos + 1, key & 0xFF);
  EEPROM.write(pos + 2, key >> 8);
  for (int i = 0; i < size; i++) EEPROM.write(pos + 3 + i, data[i]);
  EEPROM.write(pos + 3 + size, 0);
  EEPROM.commit();
  return true;
}

bool KVS::clear(key_t key) {
  int pos = getPos(key);
  if (pos <= 0) return false;
  clearAt(pos);
  EEPROM.commit(); 
  return true;
}
bool KVS::clearAll() {
  EEPROM.write(0, 0);
  EEPROM.commit(); 
  return true;
}

int KVS::getPos(key_t key) {
  unsigned pos = 0;
  while (pos < total_size) {
    if (sizeAt(pos) == 0) break;
    if (keyAt(pos) == key) return pos;
    pos = next(pos);
  }
  return -1;
}

void KVS::clearAt(unsigned pos) {
  unsigned size = sizeAt(pos);
  for (int i = pos; i < total_size - size - 3; i++) EEPROM.write(i, EEPROM.read(i + size + 3));
}

}