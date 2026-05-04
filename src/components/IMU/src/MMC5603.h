#include <Arduino.h>
#include <Wire.h>

#define MMC5603_I2C_ADDR 0x30
#define MMC5603_REG_DATA  0x00 // Xout0 (ここから6バイト読む)
#define MMC5603_REG_ODR   0x1A // Output data rate
#define MMC5603_REG_CTRL0 0x1B // Control register 0
#define MMC5603_REG_CTRL1 0x1C // Control register 1
#define MMC5603_REG_CTRL2 0x1D // Control register 2
#define MMC5603_REG_ID    0x39 // Product ID

#define MMC5603_ODR_100HZ 0x64 // ODR 100Hz
#define MMC5603_CTRL0 0xA0 // Continuous mode + Auto-set/reset
#define MMC5603_CTRL1 0x01 // BW 01(up to 150Hz)
#define MMC5603_CTRL2 0x13 // 100 sample for self-test, enter continuous mode

#define MMC5603_LSB_RESOLUTION 0.0625f // 0.0625μT/LSB (20-bit mode)

class MMC5603 {
  private:
    void writeRegister8(uint8_t reg, uint8_t data);
  public:
    struct MagData {
      float magX;
      float magY;
      float magZ;
    };
    bool init();
    struct MagData read();
};