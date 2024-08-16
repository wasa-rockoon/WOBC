#include "lora.h"

namespace component {

LoRa::LoRa(Stream& stream, pin_t aux, pin_t m0, pin_t m1, unsigned number)
  : process::Component("Logger", component_id_base + number),
    e220_(stream, aux, m0, m1),
    antenna_switch_(false) {
}

LoRa::LoRa(Stream& stream, pin_t aux, pin_t m0, pin_t m1, pin_t antenna_A, pin_t antenna_B, unsigned number)
  : process::Component("Logger", component_id_base + number),
    e220_(stream, aux, m0, m1),
    antenna_switch_(true), antenna_A_(antenna_A), antenna_B_(antenna_B) {

}

void LoRa::setup() {
  if (antenna_switch_) {
    pinMode(antenna_A_, OUTPUT);
    pinMode(antenna_B_, OUTPUT);
  }
  // TODO
}

void LoRa::loop() {
  // TODO 受信

  if (false) {
    uint8_t* data;
    int rssi;
    wcpp::Packet packet_received = decodePacket(data);
    wcpp::Packet packet = newPacket(packet_received.size() + 10);
    packet.copy(packet_received);
    packet.append("Ss").setInt(rssi);
    sendPacket(packet);
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

    // TODO 送信(data, size)
  }
}

}

