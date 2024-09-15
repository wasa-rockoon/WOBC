#include <library/wobc.h>
#include "TinyGPSPlus.h"

namespace component{

class GPS : public process::Component{
public:
    static const uint8_t component_id = 21;
    static const uint8_t telemetry_id = 'M';

  public:
    GPS(int rxPin, int txPin, uint32_t gpsBaud, unsigned sample_freq_hz = 1);

    void setup() override;

  protected:
    int rxPin_;
    int txPin_;
    uint32_t gpsBaud_;
    TinyGPSPlus gps_;
    HardwareSerial ss_;

    class SampleTimer: public process::Timer {
  public:
    SampleTimer(HardwareSerial& ss_ref, GPS& gps_ref, TinyGPSPlus& tinygps_ref, unsigned interval_ms);

  protected:
    void callback() override;
  
  private:
    TinyGPSPlus& gps_;
    GPS& GPS_;
    HardwareSerial& ss_;
    } sample_timer_;
};
}