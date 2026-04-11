#include <library/wobc.h>
#include <array>
#include <MadgwickAHRS.h>

namespace component {

#ifndef WOBC_MadgwickAttitude_PACKET_QUEUE_SIZE
#define WOBC_MadgwickAttitude_PACKET_QUEUE_SIZE 128
#endif

class MadgwickAttitude: public process::Component {
public:
  static const uint8_t component_id = 0x31; // TBD
  static const uint8_t telemetry_id = 'A'; // TBD

  Madgwick filter;
  MadgwickAttitude(uint8_t unit_id, float sample_freq_);

protected:
  uint8_t unit_id_;
  float sample_freq_;
  Listener my_listener_;
  std::array<float, 3> gyro_offset_ = {0.0f, 0.0f, 0.0f};
  std::array<float, 3> bias_magnetometer_ = {0.0f, 0.0f, 0.0f};
  void setup() override;
  void loop() override;
  
  std::array<float, 3> calibrate_gyro();
  std::array<float, 3> calibrate_magnetometer();
};
}