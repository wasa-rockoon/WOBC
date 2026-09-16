#include <Arduino.h>
#include <time.h>
#include <math.h>

#define GYRO_NOISE_GAIN 0.0174f
#define MAG_NOISE_GAIN 0.01f
#define ACCEL_NOISE_GAIN 1.0f
#define K_GIMBAL_LOCK_PENALTY 1.0f
#define K_ACCEL_PENALTY 10.0f
#define K_MAG_TILT_PENALTY 0.2f
#define MODE_6DOF 0
#define MODE_9DOF 1

class KalmanFilter {
    private:
    float gx_radian;
    float gy_radian;
    float gz_radian;
    unsigned long lastUpdateTime;
    unsigned long currentTime;
    float P_roll;
    float P_pitch;
    float P_yaw;
    float K_roll;
    float K_pitch;
    float K_yaw;

    public:
    struct angle {
        float roll;
        float pitch;
        float yaw;
    };
    angle angle_result;
    angle update(float gx, float gy, float gz, float ax, float ay, float az, int mode = MODE_6DOF, float mag_yaw = 0.0f);
};