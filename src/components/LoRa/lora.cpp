#include "lora.h"
#define LORA_ADDR E220::BROADCAST

namespace component {

LoRa::LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t tx, pin_t rx, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    lora_serial_(1),  // HardwareSerial(1)
    e220_(lora_serial_, aux, m0, m1),
    antenna_switch_(false),
    tx_pin_(tx),
    rx_pin_(rx),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel),
    all_packets_() {  // リスナーの初期化
}

LoRa::LoRa(pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, pin_t tx, pin_t rx, uint8_t channel, unsigned number)
  : process::Component("LoRa", component_id_base + number),
    lora_serial_(1),  // HardwareSerial(1)
    e220_(lora_serial_, aux, m0, m1),
    antenna_switch_(true),
    antenna_A_(antenna_A),
    antenna_B_(antenna_B),
    tx_pin_(tx),
    rx_pin_(rx),
    aux_pin_(aux),
    m0_pin_(m0),
    m1_pin_(m1),
    channel_(channel),
    all_packets_() {  // リスナーの初期化
}

void LoRa::setup() {
  if (antenna_switch_) {
    pinMode(antenna_A_, OUTPUT);
    pinMode(antenna_B_, OUTPUT);
  }
  
  lora_serial_.begin(9600, SERIAL_8N1, rx_pin_, tx_pin_);

  e220_.begin();
  delay(1000);
  bool ok = true;
  ok &= e220_.setMode(E220::Mode::CONFIG_DS);
  Serial.println(ok);
  ok &= e220_.setParametersToDefault();
  Serial.println(ok);
  ok &= e220_.setSerialBaudRate(115200);
  Serial.println(ok);
  ok &= e220_.setDataRate(E220::SF::SF9, E220::BW::BW125kHz);
  Serial.println(ok);
  ok &= e220_.setEnvRSSIEnable(true);
  Serial.println(ok);
  ok &= e220_.setSendMode(E220::SendMode::TRANSPARENT);
  Serial.println(ok);
  ok &= e220_.setModuleAddr(LORA_ADDR);
  Serial.println(ok);
  ok &= e220_.setChannel(channel_);
  Serial.println(ok);
  ok &= e220_.setRSSIEnable(true);
  Serial.println(ok);
  ok &= e220_.setMode(E220::Mode::NORMAL);
  Serial.println(ok);
  lora_serial_.flush();
  lora_serial_.begin(115200);

  delay(100);

  if (ok) {
    Serial.println("LoRa setup complete.");
  } else {
    Serial.println("LoRa setup error.");
  }

  // リスナーの設定
  listen(all_packets_, 8);  // キューサイズ 8 でリスナーを設定
}

void LoRa::loop() {
  // リスナーからパケットを取得して送信
  while (all_packets_) {
    wcpp::Packet packet = all_packets_.pop();
    
    if (!packet.isNull()) {
      wcpp::Packet lorapacket = newPacket(64);
      lorapacket.command(LoRa::send_command_id, LoRa::component_id_base + 0);
      lorapacket.append("Pa").setPacket(packet);
      
      onCommand(lorapacket);
    }
    delay(1000);
  }
}

void LoRa::onCommand(const wcpp::Packet& packet) {
  if (packet.packet_id() == send_command_id) { // 送信コマンド
  
    auto p = packet.find("Pa");
    if (!p) return;
    wcpp::Packet packet_to_send = (*p).getPacket();
    if (!packet_to_send) return;

    auto c = packet.find("Ch");
    unsigned channel = 0;
    if (c) channel = (*c).getInt();

    unsigned size = packet_to_send.size();
    const uint8_t* data = packet_to_send.encode();

    LOG("LoRa send %d", packet_to_send.size());

    // データを送信
    e220_.sendTransparent(data, size);
  }
}

}
