#include <SoftwareSerial.h>
SoftwareSerial MWSerial(17, 16);

#define TWE_BUF_SIZE 256

void setup() {
  Serial.begin(115200);
  MWSerial.begin(115200);
}

void loop() {
  uint8_t buf[TWE_BUF_SIZE];
  int i = 0;
  if(MWSerial.available()) receive();
  /*if(MWSerial.available()){
  while (MWSerial.available()){
    buf[i] = MWSerial.read();
    i++;
  }*/
}

void receive(){
  uint8_t buf[TWE_BUF_SIZE];
  uint8_t buf2[TWE_BUF_SIZE];
  int i=0;
  while(MWSerial.available() > 0){
    buf[i] = MWSerial.read();
    Serial.printf("%2d: %02X\n",i,buf[i]);
    i++;
    if(i<4) continue;
    if(buf[0] != 0xA5 || buf[1] != 0x5A){
      Serial.println("head error");
      i = 0;
      continue;
    }
    uint16_t len = (((uint16_t)buf[2] & 0x7F) << 8) + buf[3];
    
    if(i < 6 + len) continue;
    uint8_t check_sum = 0;
    Serial.printf("len: ");
    Serial.println(len,DEC);

    for (int j = 0; j < len; j++) {
      buf2[j] = buf[4 + j];
      check_sum ^= buf2[j];
    }
    if (check_sum != buf[4 + len] || buf[5 + len] != 0x04){
      Serial.println("check sum error");
      continue;
    }else{
      uint8_t message_len = buf[17];
      uint8_t from_id = buf[4];
      uint8_t lqi = buf[15];

      Serial.println("received message");
      Serial.print("lqi: ");
      Serial.println(lqi,DEC);
      Serial.print("Contents: ");
      for(int j = 0; j < message_len; j++) Serial.printf("%02X",buf[j+len-1]);
      Serial.println();
    }
  }
}