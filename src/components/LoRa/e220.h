#pragma once

#include <library/common.h>

#ifndef E220_REGISTER_TIMEOUT_MS 
#define E220_REGISTER_TIMEOUT_MS 100
#endif

#ifndef E220_RECEIVE_TIMEOUT_MS 
#define E220_RECEIVE_TIMEOUT_MS 100
#endif

class E220 {
public:
  static const uint16_t BROADCAST = 0xFFFF;

  enum class Mode : uint8_t {
    NORMAL = 0b00,
    WOR_TX = 0b01,
    WOR_RX = 0b10,
    CONFIG_DS = 0b11,
  };

  enum class ADDR : uint8_t {
    ADDH = 0x00,
    ADDL = 0x01,
    REG0 = 0x02,
    REG1 = 0x03,
    REG2 = 0x04,
    REG3 = 0x05,
    CRYPT_H = 0x06,
    CRYPT_L = 0x07,
    VERSION = 0x08,
  };

  enum class SF : uint8_t {
    SF5 = 0b000,
    SF6 = 0b001,
    SF7 = 0b010,
    SF8 = 0b011,
    SF9 = 0b100,
    SF10 = 0b101,
    SF11 = 0b110,
  };

  enum class BW : uint8_t {
    BW125kHz = 0b00,
    BW250kHz = 0b01,
    BW500kHz = 0x10,
  };

  enum class Power : uint8_t {
    POWER13dBm = 0b01,
    POWER7dBm = 0b10,
    POWER0dBm = 0b11,
  };

  enum class SendMode : uint8_t {
    TRANSPARENT = 0b0,
    FIXED = 0b1,
  };

  E220(Stream& stream, pin_t aux, pin_t m0, pin_t m1);

  bool begin();
  bool setMode(Mode mode);

  bool isBusy();

  bool sendTransparent(const uint8_t* data, unsigned len);
  bool send(uint16_t addr, uint8_t channel, const uint8_t* data, unsigned len);

  unsigned receive(uint8_t* data, unsigned max_len = 0);

  bool setModuleAddr(uint16_t addr);
  bool setSerialBaudRate(unsigned baud);
  bool setDataRate(SF sf, BW bw);
  bool setEnvRSSIEnable(bool enable);
  bool setPower(Power power);
  bool setChannel(uint8_t channel);
  bool setRSSIEnable(bool enable);
  bool setSendMode(SendMode mode);

  bool setParametersToDefault();

  inline int getRSSI() const { return rssi_; };
  int getEnvRSSI();

  bool writeRegister(ADDR addr, const uint8_t* parameters, uint8_t len = 1);
  bool writeRegisterWithMask(ADDR addr, uint8_t mask, uint8_t value);
  bool readRegister(ADDR addr, uint8_t* parameters, uint8_t len = 1);

private:
  Stream& stream_;
  pin_t aux_;
  pin_t m0_;
  pin_t m1_;

  unsigned long last_received_ms_;
  unsigned last_received_len_;

  bool RSSI_enabled_;
  unsigned baud_;
  int rssi_;
};

