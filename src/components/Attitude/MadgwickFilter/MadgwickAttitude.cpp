#include "MadgwickAttitude.h"

namespace component {

MadgwickAttitude::MadgwickAttitude(uint8_t unit_id, float sample_freq_) : process::Component("MadgwickAttitude", component_id) 
,unit_id_(unit_id), sample_freq_(sample_freq_) {

}

void MadgwickAttitude::setup() {
    my_listener_.telemetry();
    my_listener_.component(0x30); 
    listen(my_listener_, WOBC_MadgwickAttitude_PACKET_QUEUE_SIZE);
    delay(10);
    gyro_offset_ = calibrate_gyro();
    //bias_magnetometer_ = calibrate_magnetometer();
    filter.begin(sample_freq_);
}

void MadgwickAttitude::loop() {
    while (my_listener_) {
        wcpp::Packet p = my_listener_.pop();
        if (!(p.isTelemetry() && p.component_id() == 0x30)) {
            continue;
        }

        auto ax = p.find("Ax");
        auto ay = p.find("Ay");
        auto az = p.find("Az");
        auto gx = p.find("Gx");
        auto gy = p.find("Gy");
        auto gz = p.find("Gz");
        auto mx = p.find("Mx");
        auto my = p.find("My");
        auto mz = p.find("Mz");
        if (!(ax && ay && az && gx && gy && gz && mx && my && mz)) {
            continue;
        }

        float Ax = (*ax).getFloat32();
        float Ay = (*ay).getFloat32();
        float Az = (*az).getFloat32();
        float Gx = (*gx).getFloat32();
        float Gy = (*gy).getFloat32();
        float Gz = (*gz).getFloat32();

        float Gx_cal = Gx - gyro_offset_[0];
        float Gy_cal = Gy - gyro_offset_[1];
        float Gz_cal = Gz - gyro_offset_[2];
        filter.updateIMU(Gx_cal, Gy_cal, Gz_cal, Ax, Ay, Az);

        float roll  = filter.getRoll();
        float pitch = filter.getPitch();
        float yaw   = filter.getYaw();
        wcpp::Packet packet = newPacket(64);
        packet.telemetry(telemetry_id, component_id, unit_id_, 0xFF, 1234);
        packet.append("Ro").setFloat32(roll);
        packet.append("Pi").setFloat32(pitch);
        packet.append("Ya").setFloat32(yaw);
        packet.append("Ts").setInt((int)millis());
        sendPacket(packet);
    }
}

std::array<float, 3> MadgwickAttitude::calibrate_gyro() {
    float gyro_x = 0.0f, gyro_y = 0.0f, gyro_z = 0.0f;
    int sample_count = 0;
    for (int i = 0; i < 200; i++) {
        if (my_listener_) {
            wcpp::Packet p = my_listener_.pop();
            if (p.isTelemetry() && p.component_id() == 0x30) {
                auto ex = p.find("Gx");
                auto ey = p.find("Gy");
                auto ez = p.find("Gz");
                if (ex && ey && ez) {
                    gyro_x += (*ex).getFloat32();
                    gyro_y += (*ey).getFloat32();
                    gyro_z += (*ez).getFloat32();
                    sample_count++;
                }
            }
        }
        delay(5);
    }
    if (sample_count == 0) {
        return {0.0f, 0.0f, 0.0f};
    }

    float offset_gx = gyro_x / sample_count;
    float offset_gy = gyro_y / sample_count;
    float offset_gz = gyro_z / sample_count;
    return {offset_gx, offset_gy, offset_gz};
}

std::array<float, 3> MadgwickAttitude::calibrate_magnetometer() {
    // 磁力計の較正処理（未実装）
    return {0.0f, 0.0f, 0.0f};
}
}

