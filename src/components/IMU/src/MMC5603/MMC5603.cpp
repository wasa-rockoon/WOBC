#include <Arduino.h>
#include "MMC5603.h"

bool MMC5603::init() {  //Wire.begin()はmainでやること
  // 1. 接続確認 (Chip IDのチェック)
  Wire.beginTransmission(MMC5603_I2C_ADDR);
  Wire.write(MMC5603_REG_ID); 
  Wire.endTransmission(false); 
  Wire.requestFrom(MMC5603_I2C_ADDR, 1);
  if (Wire.read() != 0x10) {
    //Serial.println("MMC5603 not found!");
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
  
  //Serial.println("MMC5603 initialized in 100Hz Continuous Mode.");
  return true;
}

struct MMC5603::MagData MMC5603::read() {
  //int init_flag = 0;
  static struct MagData magData;

  if (!isMMCdataready()) {
    //Serial.println("MMC5603 data not ready");
    return magData;
  }

  Wire.beginTransmission(MMC5603_I2C_ADDR);
  Wire.write(MMC5603_REG_DATA); 
  Wire.endTransmission(false); 

  // X, Y, Z (各3バイト) ＝ 計9バイトを一括要求
  Wire.requestFrom(MMC5603_I2C_ADDR, 9);

  if (Wire.available() >= 9) {
    uint8_t buf[9];
    for(int i = 0; i < 9; i++) {
        buf[i] = Wire.read();
    }
    uint32_t x_raw = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[6] >> 4);
    uint32_t y_raw = ((uint32_t)buf[2] << 12) | ((uint32_t)buf[3] << 4) | ((uint32_t)buf[7] >> 4);
    uint32_t z_raw = ((uint32_t)buf[4] << 12) | ((uint32_t)buf[5] << 4) | ((uint32_t)buf[8] >> 4);

    // 20ビットモードの場合、生データは符号なし(0 ~ 1048575)で、中心が524288。
    // そのため、524288を引いて ±524288 の符号付き整数（中心0）に変換する
    magData.magX = ((int32_t)x_raw - 524288) * MMC5603_LSB_RESOLUTION;
    magData.magY = ((int32_t)y_raw - 524288) * MMC5603_LSB_RESOLUTION;
    magData.magZ = ((int32_t)z_raw - 524288) * MMC5603_LSB_RESOLUTION;

    
    /*if (magData.magX == -32768 && magData.magY == -32768 && magData.magZ == -32768) {
      init_flag += 1;
      if (init_flag > 5) { // 5回連続で異常値が出たら再初期化
        init_flag = 0;
        //Serial.println("MMC5603 read error, reinitializing...");
        init();
      }
    }*/
  }
  return magData;
}

void MMC5603::writeRegister8(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(MMC5603_I2C_ADDR);
  Wire.write(reg);
  Wire.write(data);
  int error = Wire.endTransmission();
  if (error != 0) {
    //Serial.print("I2C write error: ");
    //Serial.println(error);
  }
}

bool MMC5603::isMMCdataready() {
  Wire.beginTransmission(MMC5603_I2C_ADDR);
  Wire.write(MMC5603_Status1); 
  Wire.endTransmission(false); 
  Wire.requestFrom(MMC5603_I2C_ADDR, 1);
  if (Wire.available()) {
    uint8_t status = Wire.read();
    return (status & 0x40) != 0; // データ準備完了フラグが立っているか(6ビット目のビットマスク)
  }
  return false;
}