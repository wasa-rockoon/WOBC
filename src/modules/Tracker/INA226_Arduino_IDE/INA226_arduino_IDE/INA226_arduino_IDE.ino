#include "INA226.h"

INA226 INA1(0x4F);   //Lipoのコネクタから充放電モジュールまで
INA226 INA2(0x4D);   //充放電とバックアップからDCDCまで
INA226 INA3(0x4E);   //DCDC後


void setup()
{
  Serial.begin(115200);
  delay(1000);
  //Serial.println(__FILE__);
  //Serial.print("INA226_LIB_VERSION: ");
  //Serial.println(INA226_LIB_VERSION);

  Wire.begin(17, 16);
  if (!INA1.begin() )
  {
    Serial.println("could not connect. Fix and Reboot");
    while(1);
  }
  INA1.setMaxCurrentShunt(1, 0.05);
  if (!INA2.begin() )
  {
    Serial.println("could not connect. Fix and Reboot");
    while(1);
  }
  INA2.setMaxCurrentShunt(1, 0.05);
  if (!INA3.begin() )
  {
    Serial.println("could not connect. Fix and Reboot");
    while(1);
  }
  INA3.setMaxCurrentShunt(1, 0.05);
}


void loop()
{
  // Serial.println("\nBUS\tSHUNT\tCURRENT\tPOWER");
  Serial.print(INA1.getBusVoltage(), 3);
  Serial.print("\t");
  //Serial.print(INA.getShuntVoltage_mV(), 3);
  //Serial.print("\t");
  Serial.print(INA1.getCurrent_mA(), 8);
  Serial.print("\t");
  Serial.print("||");
  // Serial.print(INA.getPower_mW(), 3);
  Serial.print(INA2.getBusVoltage(), 3);
  Serial.print("\t");
  Serial.print(INA2.getCurrent_mA(), 8);
  Serial.print("\t");
  Serial.print("||");
  Serial.print(INA3.getBusVoltage(), 3);
  Serial.print("\t");
  Serial.print(INA3.getCurrent_mA(), 8);
  Serial.print("\t");  
  Serial.println();
  delay(1);
}

