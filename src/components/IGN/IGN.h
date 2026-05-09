#include <library/wobc.h>

namespace component{

class IGN : public process::Component{
public:
    static const uint8_t component_id = 0x23;
    static const uint8_t telemetry_id = 'I';
  public:
    IGN(unsigned sample_freq_hz = 1);

    void setup() override;
  
  protected:
    class SampleTimer: public process::Timer {
        public:
            SampleTimer(IGN& ign_ref, uint8_t unit_id_ref, unsigned interval_ms);

        protected:
            void callback() override;


        private:
            IGN& ign_;
            uint8_t unit_id_;
    } sample_timer_;
}
}