#ifndef TACHOMETER_HPP
#define TACHOMETER_HPP

#include <Arduino.h>

class Tachometer {
private:
    uint8_t _pin;                     // センサーを接続するGPIOピン番号
    volatile uint32_t _last_time;     // 磁石を最後に検知した時刻（マイクロ秒）
    volatile uint32_t _diff;          // パルス間の時間差（周期：マイクロ秒）
    float _current_rpm;               // 計算された現在の回転数（RPM）

    // 磁石検知時に実行される割り込みハンドラ
    // IRAM_ATTRは、ESP32-S3の高速な内部メモリ（IRAM）にコードを配置し、レスポンスを速めるための指示です．
    static void IRAM_ATTR handleInterrupt(void* arg);

public:
    // コンストラクタ：使用するピンを指定します．
    Tachometer(uint8_t pin);
    
    // 初期化：ピンモードを設定し、割り込みを開始します．
    void begin();
    
    // 計算：メインループ内で呼び出し、最新の回転数を算出・更新します．
    void update();
    
    // 取得：計算済みの最新RPMを返します．
    float getRPM() const;
};

#endif