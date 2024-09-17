#include "rplora.h"
#define LORA_ADDR E220::BROADCAST

namespace component {

LoRa::LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t tx, pin_t rx, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    lora_serial_(tx, rx, 256),
    e220_(lora_serial_, aux, m0, m1),
    antenna_switch_(false),
    tx_pin_(tx),
    rx_pin_(rx),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel){
}

LoRa::LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, pin_t tx, pin_t rx, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    lora_serial_(tx, rx, 256),
    e220_(lora_serial_, aux, m0, m1),
    antenna_switch_(true),
    antenna_A_(antenna_A),
    antenna_B_(antenna_B),
    tx_pin_(tx),
    rx_pin_(rx),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel)
    {
}

void LoRa::setup() {
  if (antenna_switch_) {
    pinMode(antenna_A_, OUTPUT);
    pinMode(antenna_B_, OUTPUT);
    digitalWrite(antenna_A_, HIGH);
    digitalWrite(antenna_B_, LOW);
  }
  
  lora_serial_.begin(9800);
  

  bool ok = true;
  ok &= e220_.begin();
  delay(1000);
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
  //lora_serial_.flush();
  //lora_serial_.end();
  //lora_serial_.begin(115200);

  delay(100);

  if (ok) {
    LOG("LoRa setup complete.");
  } else {
    LOG("LoRa setup error.");
  }
}

void LoRa::loop() {
  uint8_t data[255];
  unsigned len = e220_.receive(data);

  while(e220_.isBusy()){}

  if (len > 0) {
    unsigned data_size = len - 1;
    uint8_t received_checksum = data[data_size];
    uint8_t* received_data = data;
    wcpp::Packet packet_received = decodePacket(received_data);
    uint8_t calculated_checksum = packet_received.checksum(received_data, data_size);

    if (calculated_checksum == received_checksum) {
      int rssi = e220_.getRSSI();
      wcpp::Packet packet = newPacket(packet_received.size() + 10);
      packet.copy(packet_received);
      packet.append("Ss").setInt(rssi);
      sendPacket(packet);
    } else {
      LOG("fail to receive");
    }
  }
}

}
