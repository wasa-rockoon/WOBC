#include <Arduino.h>
#include "MMC5603.h"

bool MMC5603::init() {  //Wire.begin()はmainでやってね
  // 1. 接続確認 (Chip IDのチェック)
  writeRegister8(MMC5603_REG_ID, 0x00); // Dummy write to set register pointer
  Wire.requestFrom(MMC5603_I2C_ADDR, 1);
  if (Wire.read() != 0x10) {
    Serial.println("MMC5603 not found!");
    return false;
  }

  writeRegister8(MMC5603_REG_ODR, MMC5603_ODR_100HZ);
  delay(1);

  writeRegister8(MMC5603_REG_CTRL1, MMC5603_CTRL1);
  delay(1);
  
  writeRegister8(MMC5603_REG_CTRL0, MMC5603_CTRL0);
  delay(1);

  writeRegister8(MMC5603_REG_CTRL2, MMC5603_CTRL2);
  delay(10);
  
  Serial.println("MMC5603 initialized in 100Hz Continuous Mode.");
  return true;
}

// --- 高速読み出し処理 ---
struct MMC5603::MagData MMC5603::read() {
  struct MagData magData;
  // データの先頭レジスタを指定するためのダミー書き込み
  writeRegister8(MMC5603_REG_DATA, 0x00); 

  // X, Y, Z (各2バイト) ＝ 計6バイトを一括要求
  Wire.requestFrom(MMC5603_I2C_ADDR, 6);

  if (Wire.available() >= 6) {
    // MMC5603は MSB(上位ビット) -> LSB(下位ビット) の順で送られてくる
    uint16_t x_raw = (Wire.read() << 8) | Wire.read();
    uint16_t y_raw = (Wire.read() << 8) | Wire.read();
    uint16_t z_raw = (Wire.read() << 8) | Wire.read();

    // 16ビットモードの場合、生データは符号なし(0 ~ 65535)で、中心が32768。
    // そのため、32768を引いて ±32768 の符号付き整数（中心0）に変換する
    magData.magX = ((int32_t)x_raw - 32768) * MMC5603_LSB_RESOLUTION;
    magData.magY = ((int32_t)y_raw - 32768) * MMC5603_LSB_RESOLUTION;
    magData.magZ = ((int32_t)z_raw - 32768) * MMC5603_LSB_RESOLUTION;
  }
  return magData;
}

void MMC5603::writeRegister8(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(MMC5603_I2C_ADDR);
  Wire.write(reg);
  Wire.write(data);
  int error = Wire.endTransmission();
  if (error != 0) {
    Serial.print("I2C write error: ");
    Serial.println(error);
  }
}