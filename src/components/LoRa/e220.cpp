#include "e220.h"

E220::E220(Stream& stream, pin_t aux, pin_t m0, pin_t m1)
  : stream_(stream), aux_(aux), m0_(m0), m1_(m1) {
  baud_ = 9600;
  RSSI_enabled_ = false;
  rssi_ = -255;
}

bool E220::begin() {
  pinMode(aux_, INPUT);
  pinMode(m0_, OUTPUT);
  pinMode(m1_, OUTPUT);

  setMode(Mode::NORMAL);

  unsigned long ms = millis();
  while (isBusy()) {
    if (millis() - ms > timeout_ms_ * 50) return false;
  }
  delay(200);
  return true;
}

bool E220::isBusy() {
  return !digitalRead(aux_);
}

bool E220::sendTransparent(const uint8_t* data, unsigned len) {
  if (isBusy()) return false;

  uint8_t buf[256];
  buf[0] = len;
  memcpy(buf + 1, data, len);
  stream_.write(buf, len + 1);
  return true;
}

bool E220::send(uint16_t addr, uint8_t channel, const uint8_t* data, unsigned len) {
  if (isBusy()) return false;

  uint8_t buf[260];
  buf[0] = (uint8_t)(addr >> 8);
  buf[1] = (uint8_t)addr;
  buf[2] = channel;
  buf[3] = len;
  memcpy(buf + 4, data, len);
  stream_.write(buf, 4 + len);
  return true;
}

unsigned E220::receive(uint8_t* data, unsigned max_len) {
  if (stream_.available() == 0) return 0;
  uint8_t len = stream_.peek();

  if (len != last_received_len_) {
    last_received_ms_ = millis();
    last_received_len_ = len;
  }

  if (RSSI_enabled_) {
    if (millis() - last_received_ms_ > E220_RECEIVE_TIMEOUT_MS) { // Receive timeout
      while (stream_.available()) stream_.read();
      last_received_len_ = 0;
      return 0;
    }
    
    if ((int)stream_.available() < len + 2) return 0;
    if (max_len > 0 && len > max_len) {
      for (int i = 0; i < len + 2; i++) stream_.read();
      return 0;
    }
    len = stream_.read();
    stream_.readBytes(data, len);
    rssi_ = - ((int)256 - (uint8_t)stream_.read());
    return len;
  }
  else {
    if ((int)stream_.available() < len + 1) return 0;
    if (max_len > 0 && len > max_len) {
      for (int i = 0; i < len + 1; i++) stream_.read();
      return 0;
    }
    len = stream_.read();
    stream_.readBytes(data, len);
    return len;
  }
}


bool E220::setMode(Mode mode) {
  unsigned long ms = millis();
  while (isBusy()) {
    if (millis() - ms > timeout_ms_ * 50) return false;
  }
  digitalWrite(m0_, static_cast<uint8_t>(mode) & 0b01 ? HIGH : LOW);
  digitalWrite(m1_, static_cast<uint8_t>(mode) & 0b10 ? HIGH : LOW);
  delay(100);

  ms = millis();
  while (isBusy()) {
    if (millis() - ms > timeout_ms_ * 50) return false;
  }
  stream_.flush();

  return true;
}

bool E220::setSerialBaudRate(unsigned baud) {
  uint8_t bits = 0b000;
  switch (baud) {
  case 1200:
    bits = 0b000;
    break;
  case 2400:
    bits = 0b001;
    break;
  case 4800:
    bits = 0b010;
    break;
  case 9600:
    bits = 0b011;
    break;
  case 19200:
    bits = 0b100;
    break;
  case 38400:
    bits = 0b101;
    break;
  case 57600:
    bits = 0b100;
    break;
  case 115200:
    bits = 0b111;
    break;
  default:
    return false;
  }

  if (writeRegisterWithMask(ADDR::REG0, 0b11100000, bits << 5)) baud_ = baud;

  return true;
}

bool E220::setModuleAddr(uint16_t addr) {
  uint8_t addr_high_low[2] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF) };
  return writeRegister(ADDR::ADDH, addr_high_low, 2);
}

bool E220::setDataRate(SF sf, BW bw) {
  return writeRegisterWithMask(
      ADDR::REG0, 0b00011111,
      static_cast<uint8_t>(sf) << 2 | static_cast<uint8_t>(bw)
  );
}

bool E220::setEnvRSSIEnable(bool enable) {
  return writeRegisterWithMask(ADDR::REG1, 0b00100000, (enable ? 0b1 : 0b0) << 5);
}

bool E220::setPower(Power power) {
  bool ok = true;
  uint8_t reg1;
  ok &= readRegister(ADDR::REG1, &reg1);
  reg1 = (reg1 & 0b00000011) | static_cast<uint8_t>(power);
  ok &= writeRegister(ADDR::REG1, &reg1);
  return ok;
}

bool E220::setChannel(uint8_t channel) {
  return writeRegister(ADDR::REG2, &channel);
}

bool E220::setRSSIEnable(bool enable) {
  bool ok = writeRegisterWithMask(ADDR::REG3, 0b10000000,
                                  (enable ? 0b1 : 0b0) << 7);
  if (ok) RSSI_enabled_ = enable;
  return ok;
}

bool E220::setSendMode(SendMode mode) {
  return writeRegisterWithMask(ADDR::REG3, 0b01000000, static_cast<uint8_t>(mode) << 6);
}

int E220::getEnvRSSI() {
  if (isBusy() || stream_.available() > 0) return -255;
  uint8_t cmd[6] = { 0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x02 };
  stream_.write(cmd, 6);

  unsigned long ms = millis();
  while (isBusy() || (int)stream_.available() < 5) {
    if (millis() - ms > timeout_ms_) return -255;
  }

  uint8_t rx[5];
  stream_.readBytes(rx, 5);

  return - ((int)256 - (rx[3]));
}

bool E220::setParametersToDefault() {
  uint8_t parameters[] = { 0x00, 0x00, 0x62, 0x00, 0x0F, 0x03, 0x00, 0x00 };
  return writeRegister(ADDR::ADDH, parameters, 8);
}

bool E220::writeRegister(ADDR addr, const uint8_t* parameters, uint8_t len) {
  bool ok = true;

  stream_.flush();

  uint8_t cmd[10];
  cmd[0] = 0xC0;
  cmd[1] = static_cast<uint8_t>(addr);
  cmd[2] = len;
  memcpy(cmd + 3, parameters, len);
  stream_.write(cmd, 3 + len);

  unsigned long ms = millis();
  while (isBusy() || (int)stream_.available() < 3 + len) {
    if (millis() - ms > timeout_ms_) return false;
  }

  uint8_t rx[16];
  stream_.readBytes(rx, 3 + len);

  cmd[0] = 0xC1;
  ok &= memcmp(cmd, rx, 3) == 0;
  ok &= memcmp(parameters, rx + 3, len) == 0;
//  ok &= memcmp(parameters, rx + 3, len) == 0;
  delay(10);
  return ok;
}

bool E220::writeRegisterWithMask(ADDR addr, uint8_t mask, uint8_t value) {
  bool ok = true;
  uint8_t reg;
  ok &= readRegister(addr, &reg);
  reg = (reg & ~mask) | (value & mask);
  ok &= writeRegister(addr, &reg);
  return ok;
}

bool E220::readRegister(ADDR addr, uint8_t* parameters, uint8_t len) {
  bool ok = true;

  stream_.flush();

  uint8_t header[3] = { 0xC1, static_cast<uint8_t>(addr), len};
  stream_.write(header, 3);

  unsigned long ms = millis();
  while ((int)stream_.available() < 3 + len) {
    if (millis() - ms > timeout_ms_) return false;
  }

  uint8_t rx[16];
  stream_.readBytes(rx, 3 + len);

  ok &= memcmp(header, rx, 3) == 0;
  if (ok) memcpy(parameters, rx + 3, len);

  ms = millis();
  while (isBusy()) {
    if (millis() - ms > timeout_ms_) return false;
  }
  stream_.flush();

  delay(10);

  return ok;
}