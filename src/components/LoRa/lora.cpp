#include "lora.h"
#define LORA_ADDR E220::BROADCAST

namespace component {

LoRa::LoRa(driver::GenericSerialClass& serial, pin_t aux, pin_t m0, pin_t m1, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    serial_(serial),
    e220_(serial, aux, m0, m1),
    antenna_switch_(false),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel) {
}

LoRa::LoRa(driver::GenericSerialClass& serial, pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    serial_(serial),
    e220_(serial, aux, m0, m1),
    antenna_switch_(true),
    antenna_A_(antenna_A),
    antenna_B_(antenna_B),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel) { 
}

void LoRa::setup() {
  if (antenna_switch_) {
    pinMode(antenna_A_, OUTPUT);
    pinMode(antenna_B_, OUTPUT);
  }
  
  serial_.begin(9600);

  e220_.begin();
  delay(1000);
  bool ok = true;
  ok &= e220_.setMode(E220::Mode::CONFIG_DS);
  ok &= e220_.setParametersToDefault();
  ok &= e220_.setSerialBaudRate(9600);
  ok &= e220_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  ok &= e220_.setEnvRSSIEnable(true);
  ok &= e220_.setSendMode(E220::SendMode::TRANSPARENT);
  ok &= e220_.setModuleAddr(LORA_ADDR);
  ok &= e220_.setChannel(channel_);
  ok &= e220_.setRSSIEnable(true);
  ok &= e220_.setMode(E220::Mode::NORMAL);
  // serial_.flush();
  // serial_.end();
  // serial_.begin(115200);

  delay(100);

  if (ok) {
    LOG("LoRa setup complete.");
  } else {
    error("STU", "LoRa setup error.");
  }
}

void LoRa::loop() {
  uint8_t data[256];
  unsigned len = e220_.receive(data);

  while(e220_.isBusy()){ delay(1); }

  if (len > 0) {
    unsigned data_size = len - 1;
    uint8_t received_checksum = data[data_size];
    uint8_t* received_data = data;
    wcpp::Packet packet_received = decodePacket(received_data);

    uint8_t calculated_checksum = packet_received.checksum(received_data, data_size);

    /*Serial.printf("size:%d\t", len);
    for(int i = 0; i < len; i++) {
      Serial.print(data[i], HEX);
    }
    Serial.printf("\t");*/

    if (calculated_checksum == received_checksum) {
      /*Serial.print(calculated_checksum,HEX);
      Serial.printf("\t");
      Serial.print(received_checksum,HEX);
      Serial.printf("\t");
      Serial.println("Checksum valid");*/

      int rssi = e220_.getRSSI();
      wcpp::Packet packet = newPacket(packet_received.size() + 10);
      packet.copy(packet_received);
      packet.append("Ss").setInt(rssi);

      sendPacket(packet);
    } 
    else {
      error("pCS", "check sum error: %X vs %X", calculated_checksum, received_checksum);
    }
  }
}

void LoRa::onCommand(const wcpp::Packet& packet) {
  while(e220_.isBusy());
  delay(100);
  
  if (packet.packet_id() != send_command_id) return;
  
  auto p = packet.find("Pa");
  if (!p) return;
  wcpp::Packet packet_to_send = (*p).getPacket();
  if (!packet_to_send) return;

  auto c = packet.find("Ch");
  unsigned channel = 0;
  if (c) channel = (*c).getInt();

  unsigned size = packet_to_send.size();
  const uint8_t* data = packet_to_send.encode();

  uint8_t checksum_value = packet_to_send.checksum(data, size);

  uint8_t data_with_checksum[size + 1];
  memcpy(data_with_checksum, data, size);
  data_with_checksum[size] = checksum_value;

  LOG("LoRa send %d %X", size + 1, checksum_value);

  // Serial.printf("size:%d\t", size + 1);
  // for(int i = 0; i < size + 1; i++) {
  //   Serial.print(data_with_checksum[i], HEX);
  // }
  // Serial.printf("\t");
  // Serial.print(checksum_value, HEX);
  // Serial.println();

    // データを送信
  e220_.sendTransparent(data_with_checksum, size + 1);
}

}
