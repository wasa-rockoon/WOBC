#include <library/wobc.h>
#include <Wire.h>
#include <cmath>

#define MCP3424_ADDR 0x6A

namespace component {
    class Heater: public process::Component {
    public:
        static const uint8_t component_id = 0x46;
        static const uint8_t telemetry_id = 'H';

        Heater(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz = 1);
        static float CalculatedTemperature[4];

    protected:
        static const constexpr float V_REF = 2.048;         // MCP1501の出力電圧 (2.048V)
        static const constexpr float R_UPSTREAM = 120.0;     // 上流の保護抵抗値 (120Ω)
        static const constexpr float R_DOWNSTREAM = 10000.0; // GND側の分圧抵抗値 (10kΩ)

        // サーミスタ 103JT-050 の特性値
        static const constexpr float B_CONSTANT = 3435.0; 
        static const constexpr float T0 = 298.15;          // 25℃ (25 + 273.15)
        static const constexpr float R0 = 10000.0;         // 25℃のときの10kΩ

        // ワンショットモードで各CHの測定をキックする設定バイト
        static const byte CONFIG_CH[4];

        TwoWire& wire_;
        uint8_t unit_id_;

        void setup() override;

    class SampleTimer: public process::Timer {
    public:
        SampleTimer(Heater& heater_ref, TwoWire& wire_ref, uint8_t unit_id_ref, unsigned interval_ms);

    protected:
        void callback() override;

    private:
        TwoWire& wire_;
        Heater& heater_;
        uint8_t unit_id_;
    } sample_timer_;
    };

}