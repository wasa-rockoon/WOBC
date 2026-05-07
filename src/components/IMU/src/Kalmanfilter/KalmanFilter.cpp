#include <Arduino.h>
#include "KalmanFilter.h"

KalmanFilter::angle KalmanFilter::update(float gx, float gy, float gz, float ax, float ay, float az, int mode, float mag_yaw) {
    currentTime = millis();
    float dt = (currentTime - lastUpdateTime) / 1000.0f; // 秒単位の経過時間
    lastUpdateTime = currentTime;

    gx_radian = gx * (PI / 180.0f); // ジャイロデータをラジアンに変換
    gy_radian = gy * (PI / 180.0f);
    gz_radian = gz * (PI / 180.0f);

    //予測値の計算
    float sp = sinf(angle_result.pitch);
    float sr = sinf(angle_result.roll);
    float cr = cosf(angle_result.roll);
    float cp;

    if (cosf(angle_result.pitch) == 0.0f) {
        cp = 1e-10f; // cos(pitch)が0に近い場合の安全策
    } else {
        cp = cosf(angle_result.pitch);
    }

    float tp = sp / cp;

    float droll = gx_radian + sr * tp * gy_radian + cr * tp * gz_radian;
    float dpitch = cr * gy_radian - sr * gz_radian;
    float dyaw = sr / cp * gy_radian + cr / cp * gz_radian;

    float predicted_roll = angle_result.roll + droll * dt;
    float predicted_pitch = angle_result.pitch + dpitch * dt;
    float predicted_yaw = angle_result.yaw + dyaw * dt;

    float gyro_noise = GYRO_NOISE_GAIN * dt * dt;
    float mag_noise = MAG_NOISE_GAIN * dt * dt;
    float accel_noise = ACCEL_NOISE_GAIN * dt * dt;

    //観測値の計算
    float observed_roll = atan2f(ay, az);
    float observed_pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
    float observed_yaw = mag_yaw * (PI / 180.0f); // 磁気センサからの観測値をラジアンに変換

    float accel_magnitude = ax * ax + ay * ay + az * az;
    float gimbal_lock_penalty = accel_noise * K_GIMBAL_LOCK_PENALTY;
    float dynamic_q_roll = gyro_noise + (tp * tp) * gimbal_lock_penalty;

    P_roll += dynamic_q_roll;
    P_pitch += gyro_noise;
    P_yaw += gyro_noise;

    float accel_error = fabsf(accel_magnitude - 1.0f);
    float dynamic_accel_noise = accel_noise + accel_error * accel_error * K_ACCEL_PENALTY;

    //各軸のカルマンゲインの計算・角度の更新
    K_roll = P_roll / (P_roll + dynamic_accel_noise);
    angle_result.roll = predicted_roll + K_roll * (observed_roll - predicted_roll);
    P_roll = (1 - K_roll) * P_roll;

    K_pitch = P_pitch / (P_pitch + dynamic_accel_noise);
    angle_result.pitch = predicted_pitch + K_pitch * (observed_pitch - predicted_pitch);
    P_pitch = (1 - K_pitch) * P_pitch;

    if (mode == MODE_9DOF) {
        mag_noise = mag_noise + fabsf(tp) * fabsf(tp) * K_MAG_TILT_PENALTY;//傾いているときに磁気センサを無視
        K_yaw = P_yaw / (P_yaw + mag_noise);
        float yaw_error = fabsf(observed_yaw - predicted_yaw);
        // -PI ~ PIの範囲に正規化
        while (yaw_error > PI)  yaw_error -= 2.0f * PI;
        while (yaw_error < -PI) yaw_error += 2.0f * PI;
        angle_result.yaw = predicted_yaw + K_yaw * (observed_yaw - predicted_yaw);
        P_yaw = (1 - K_yaw) * P_yaw;
    } else {
        angle_result.yaw = predicted_yaw; // 6DOFモードではジャイロ積分値を使用
    }

    return angle_result;
}