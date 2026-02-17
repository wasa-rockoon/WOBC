#include <Arduino.h>

// RP2040 (Pico) の場合はFreeRTOSのヘッダを明示的にインクルード
#ifdef ARDUINO_ARCH_RP2350
#include <FreeRTOS.h>
#include <task.h>
#endif
// ESP32の場合はArduino.hにすでに含まれているので不要です

// 100Hz (10ms) でCMG制御と姿勢計算を行うタスク
void cmg_control_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 10ms周期

    while (true) {
        // ここに共通のハードウェアラッパー関数を呼ぶ
        // 例: imu_read_data();
        // 例: calculate_attitude();
        
        Serial.println("CMG Control: 100Hz Loop Running");

        // 次の10msのタイミングまで正確に待機（vTaskDelayより正確に周期を刻めます）
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// SDカードへデータを保存するタスク（優先度を下げる）
void sd_log_task(void *pvParameters) {
    while (true) {
        // キューからデータを受け取ってSDに書く処理などをここに
        Serial.println("SD Log: Saving data...");
        vTaskDelay(pdMS_TO_TICKS(50)); // 適当な周期
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000); // シリアルモニタの起動待ち

    Serial.println("FreeRTOS Start!");

    // タスクの作成 (関数名, タスク名, スタックサイズ, パラメータ, 優先度, ハンドル)
    // 制御タスクは優先度を高く(2)、SD書き込みは低く(1)する
    xTaskCreate(cmg_control_task, "CMG_Task", 4096, NULL, 2, NULL);
    xTaskCreate(sd_log_task, "SD_Task", 4096, NULL, 1, NULL);
}

void loop() {
    // FreeRTOSを使う場合、Arduinoの標準loop()は空っぽにするか、
    // 優先度の一番低い処理（Lチカなど）を置くのが定石です。
    // 今回はOSに処理を完全にお任せするため、loopタスク自体を削除します。
    vTaskDelete(NULL); 
}