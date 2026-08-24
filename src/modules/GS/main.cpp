// #define NDEBUG

#include <library/wobc.h>
#include <components/Telemeter/telemeter.h>
#include <components/Logger/logger.h>
#include <components/LoRa/lora.h>
#include <SPI.h>

// SPI / SD Card Pin Definitions
#define SPI0_SCK_PIN 12
#define SPI0_MOSI_PIN 11
#define SPI0_MISO_PIN 13
#define SPI0_CS_PIN 10

#define SD_INSERTED_PIN 9
#define SDCARD_MOSI_PIN SPI0_MOSI_PIN
#define SDCARD_MISO_PIN SPI0_MISO_PIN
#define SDCARD_SS_PIN SPI0_CS_PIN
#define SDCARD_SCK_PIN SPI0_SCK_PIN

// GS&LoRa一体型基板 LoRa Module Pin Definitions
#define LORA_CHANNEL 3
#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

constexpr uint8_t module_id = 0x47;
constexpr uint8_t unit_id = 0x64; // 書き込むユニットごとに設定

// Core Interfaces
core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);
interface::WatchIndicator<unsigned> status_indicator(42, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(41, kernel::errorCount());

// Components
component::Logger logger(SPI, SPI0_CS_PIN, SD_INSERTED_PIN);
//component::LiPoPowerSimple power(Wire);
component::Telemeter telemeter;
component::LoRa lora(LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_TX_PIN, LORA_RX_PIN, LORA_CHANNEL, 0);

class Main: public process::Component {
public:
  Main(): process::Component("main", 0x00) {}
  kernel::Listener pc_listener_;

  void setup() override {
    listen(pc_listener_, 8);
  }

  void loop() override {
    while (pc_listener_) {
      wcpp::Packet packet = pc_listener_.pop();

      // LoRa宛の送信指示コマンド自体は再度LoRa送信コマンドにラップしない
      if (packet.component_id() == (component::LoRa::component_id_base + 0) &&
          packet.packet_id() == component::LoRa::send_command_id) {
        continue;
      }

      // Tracker等からLoRa受信されて内部カーネルに放流された受領パケット（"Ss" エントリを持つ）はLoRa再送信しない
      if (packet.find("Ss")) {
        continue;
      }

      // PC等からSerialBus経由で届いたパケットを LoRa 送信用コマンドパケットに包んで送信
      wcpp::Packet lorapacket = newPacket(packet.size() + 32);
      lorapacket.command(lora.send_command_id, lora.component_id_base + 0);
      lorapacket.append("Pa").setPacket(packet);
      sendPacket(lorapacket, pc_listener_);
    }
  }
} main_;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial0.setPins(2, 1);

  kernel::setUnitId(unit_id); // unit id を設定（mainモジュールのみ）
  if (!kernel::begin(module_id, true)) return; // check module id

  //Wire.setPins();
  SPI.begin(SDCARD_SCK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN, SDCARD_SS_PIN);

  status_indicator.begin();
  status_indicator.blink_on_change();
  error_indicator.begin();
  error_indicator.set(true);

  delay(1000);

  can_bus.begin();
  serial_bus.begin();

  logger.begin();
  //power.begin();
  telemeter.begin();
  lora.begin();
  main_.begin();

  error_indicator.set(false);
  error_indicator.blink_on_change(100);
}

void loop() {
  // put your main code here, to run repeatedly:

  status_indicator.update();
  error_indicator.update();
}



