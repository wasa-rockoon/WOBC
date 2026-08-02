#include "Tachometer.hpp"

// コンストラクタ：メンバ変数を初期化します．
Tachometer::Tachometer(uint8_t pin) 
    : _pin(pin), _last_time(0), _diff(0), _current_rpm(0.0) {}

// 割り込み処理（MicroPython版のcallbackに相当）
void IRAM_ATTR Tachometer::handleInterrupt(void* arg) {
    Tachometer* t = static_cast<Tachometer*>(arg);
    uint32_t current = micros(); // 現在の時刻をマイクロ秒で取得
    
    // チャタリング防止：
    // 前回の検知から2000マイクロ秒（2ミリ秒）以内の反応はノイズとみなして無視します．
    if (current - t->_last_time > 2000) {
        t->_diff = current - t->_last_time; // パルス周期を保存
        t->_last_time = current;            // 時刻を更新
    }
}

// センサーの準備
void Tachometer::begin() {
    // 内部プルアップを有効にして、信号の浮きを防ぎます．
    pinMode(_pin, INPUT_PULLUP);
    
    // 信号が立ち上がった（RISING：磁石検知時）瞬間にhandleInterruptを実行します．
    attachInterruptArg(digitalPinToInterrupt(_pin), handleInterrupt, this, RISING);
}

// 回転数の計算（MicroPython版のcalculateSpeedに相当）
void Tachometer::update() {
    uint32_t now = micros();

    // 停止判定：
    // 最後にパルスを検知してから1秒（1,000,000us）以上経過していたら回転停止とみなします．
    if (now - _last_time > 1000000) {
        _diff = 0;
        _current_rpm = 0.0f;
    } 
    else if (_diff > 0) {
        // 計算式：
        // 1,000,000 / 周期(us) = 1秒間のパルス数（Hz）
        // 磁石が2個（1回転2パルス）想定のため、さらに 2 で割ります．
        // それに 60 を掛けて、1分間あたりの回転数（RPM）に変換します．
        float rps = 1000000.0f / (float)_diff / 2.0f;
        _current_rpm = rps * 60.0f;
    }
}

// 最新のRPMを返却します．
float Tachometer::getRPM() const {
    return _current_rpm;
}