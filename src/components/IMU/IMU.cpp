#include "IMU.h"
#include <Arduino_BMI270_BMM150.h> // 明示的に再度インクルード

namespace component {

IMU9::IMU9(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
  : process::Component("IMU", component_id),
    wire_(wire),
    unit_id_(unit_id) {
    
    // グローバル変数を直接利用
    if (!IMU.begin()) {  // IMUを使用
        // エラー処理
    }

    // ポインタの初期化を追加
    imu = &IMU;  // 正しいグローバル変数名
}
}
