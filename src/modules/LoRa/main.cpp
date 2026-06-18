// #define NDEBUG

#include <Arduino.h>
#include <library/wobc.h>
#include <components/LoRa/rplora.h>

// ピン配置と定数の設定
#define LORA_CHANNEL 3
#define LORA_TX_PIN 28
#define LORA_RX_PIN 29
#define LORA_AUX_PIN 20
#define LORA_M0_PIN 18
#define LORA_M1_PIN 19
#define LORA_SW_A1 26
#define LORA_SW_A2 27

constexpr uint8_t module_id = 0x4C;

// 通信インスタンスの作成
core::CANBus can_bus(23, 22);
core::SerialBus serial_bus(Serial);

component::LoRa lora(LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_SW_A1, LORA_SW_A2, LORA_TX_PIN, LORA_RX_PIN, LORA_CHANNEL);

// メインクラス
class Main: public process::Component {
public:
  // コンストラクタ（初期化）
  Main(): process::Component("main", 0x12) {}

  void setup() override {
  }
  void loop() override {
    delay(1000);
    LOG("LoRa working");
  }
};

// インジケーターの設定
interface::WatchIndicator<unsigned> status_indicator(25, kernel::packetCount());
interface::WatchIndicator<unsigned> error_indicator(24, kernel::errorCount());
Main main_;

// Arduinoの初期化関数（プログラム起動時に一度だけ実行。各モジュールを順番に起動）
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!kernel::begin(module_id, false)) return;

  status_indicator.begin();
  status_indicator.blink_on_change(); // パケット数が変化したら点滅するように設定
  error_indicator.begin();
  error_indicator.set(true); // 起動中であることを示すために一度点灯させる

  // マイコンの処理を一時停止して、各モジュールの初期化が完了するまで待機
  delay(500); // 0.5秒待機

  can_bus.begin(); // CAN通信開始
  serial_bus.begin(); // シリアル通信開始
  main_.begin(); // メインクラスの処理開始

  delay(1000);
  lora.begin(); // LoRa通信開始
  
  // 起動完了
  error_indicator.set(false); // 起動完了を示すために消灯
  error_indicator.blink_on_change();  // エラー数が変化したら点滅するように設定
}

// Arduinoのメインループ関数（プログラム起動後、繰り返し実行される）
void loop() {
  // put your main code here, to run repeatedly:

  status_indicator.update();
  error_indicator.update();

  // log_d("[%d %d]", serial_bus.getMaximumStackUsage(), status_indicator.getMaximumStackUsage());
  // main_.LOG("stack: %d %d %d", F_CPU, serial_bus.getMaximumStackUsage(), can_bus.getMaximumStackUsage());
}


