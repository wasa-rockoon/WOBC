#include <SoftwareSerial.h>
SoftwareSerial MWSerial(17, 16);

#define N 256
#define TWE_BUF_SIZE 256
uint8_t message[N];

void setup() {
  Serial.begin(115200);
  MWSerial.begin(115200);
}
void loop(){
  int i;
  char c;
  uint8_t buf;

  message[0] = 0x1A;
  message[1] = 0x2B;
  message[2] = 0x3C;
  message[3] = 0x4D;
  message[4] = 0x5E;
  unsigned message_len = 5;


  if(Serial.available()){
    c = Serial.read();
    send(message, message_len);
    Serial.println("sent a message.");
    delay(100);
    /*while (MWSerial.available()){
      //Serial.println("reseive feadback.");
      buf = (uint8_t)MWSerial.read();
      Serial.print(c);
    }*/
  }
    //デバック用
  /*Serial.print("len: ");
  Serial.println(len,HEX);
  Serial.print("ex_len: ");
  Serial.println(ex_len,HEX); 
  Serial.print("check sum: ");
  Serial.println(check_sum,HEX);
  Serial.print("send this: ");
  Serial.print(0xA5,HEX);
  Serial.print(0x5A,HEX);
  Serial.print(data_len,HEX);
  for(int i = 0; i < 4; i++) Serial.print(buf[i],HEX);
  for(int i = 0; i < 3; i++) Serial.print(message[i],HEX);
  Serial.println(check_sum,HEX);*/
}

void send(uint8_t *message, unsigned len){
  uint8_t buf[TWE_BUF_SIZE];
  uint8_t dest_id = 0x78;

  buf[0] = dest_id;
  buf[1] = 0xA0;
  buf[2] = 0x12;
  buf[3] = 0xFF;

  uint8_t ex_len = len + 4;
  uint16_t data_len = 0x8000 + ex_len;
  uint8_t check_sum;

  for(int i =0; i < 4; i++) check_sum ^= buf[i];
  for(int i =0; i < len; i++) check_sum ^= message[i];

  MWSerial.write(0xA5);
  MWSerial.write(0x5A);
  MWSerial.write(data_len);
  MWSerial.write(buf, 4);
  MWSerial.write(message, len);
  MWSerial.write(check_sum);

  Serial.printf("len: %d\n",len);
}