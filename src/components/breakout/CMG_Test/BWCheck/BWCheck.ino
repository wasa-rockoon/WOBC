#include <Arduino.h>

// センサー(LM393出力)をつないだピン番号
// Raspberry Pi Picoの「GP16」ピンを使用します
const int SENSOR_PIN = 20; 

void setup() {
  // シリアル通信の開始
  Serial.begin(115200);

  // 【Pico用】USBシリアルが接続されるまで待機する処理
  // これを入れないと、シリアルモニタを開く前のログが見えなくなることがあります
  // ※電源投入だけで動かしたい時は、この行をコメントアウトしてください
  // while (!Serial) delay(10);
  
  // ピンを入力モードに設定
  // 回路図通りに外付けの10kΩプルアップ抵抗をつけている場合は INPUT でOKです
  // もし抵抗をつけ忘れている場合は、ここを INPUT_PULLUP に変えると動くことがあります
  pinMode(SENSOR_PIN, INPUT);
}

void loop() {
  // 状態を読み取る (0 または 1)
  int sensorState = digitalRead(SENSOR_PIN);

  // 結果をシリアルモニタに表示
  Serial.print("Sensor Value: ");
  Serial.print(sensorState);
  
  // 白黒判定（LM393の接続によってHIGH/LOWの意味が変わることがあります）
  if (sensorState == HIGH) {
    Serial.println("  ---> [ High / 白 ]");
  } else {
    Serial.println("  ---> [ Low / 黒 ]");
  }

  // 表示が速すぎて読めないのを防ぐため0.1秒待つ
  delay(100);
}