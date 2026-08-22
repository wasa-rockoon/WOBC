#include "Heater.h"

namespace component {

    const byte Heater::CONFIG_CH[4] = {
            0x88, // CH1 測定スタート (One-Shot, 16bit)
            0xA8, // CH2 測定スタート (One-Shot, 16bit)
            0xC8, // CH3 測定スタート (One-Shot, 16bit)
            0xE8  // CH4 測定スタート (One-Shot, 16bit)
            };

    float Heater::CalculatedTemperature[4] = {
            0.0f,
            0.0f,
            0.0f,
            0.0f
            };
    
    Heater::Heater(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz,
                   AdcResolution adc_resolution)
        : process::Component("Heater", component_id),
          wire_(wire),
          unit_id_(unit_id),
          adc_resolution_(adc_resolution),
          sample_timer_(*this, wire_, unit_id_, sample_freq_hz > 0 ? 1000 / sample_freq_hz : 1000) {
    }

    void Heater::setAdcResolution(AdcResolution adc_resolution) {
        adc_resolution_ = adc_resolution;
    }

    Heater::AdcResolution Heater::adcResolution() const {
        return adc_resolution_;
    }

    uint16_t Heater::conversionTimeoutMs() const {
        switch (adc_resolution_) {
            case AdcResolution::BIT_12: return 10;
            case AdcResolution::BIT_14: return 30;
            case AdcResolution::BIT_16: return 100;
            case AdcResolution::BIT_18: return 350;
        }
        return 350;
    }

    float Heater::voltsPerCount() const {
        switch (adc_resolution_) {
            case AdcResolution::BIT_12: return 0.001f;
            case AdcResolution::BIT_14: return 0.00025f;
            case AdcResolution::BIT_16: return 0.0000625f;
            case AdcResolution::BIT_18: return 0.000015625f;
        }
        return 0.0000625f;
    }

    void Heater::setup() {
        pinMode(HEATER_PIN, OUTPUT);
        digitalWrite(HEATER_PIN, LOW);

        wire_.beginTransmission(MCP3424_ADDR);
        if (wire_.endTransmission() != 0) {
            error("H", "Failed to initialize MCP3424!");
        }
        start(sample_timer_);
    }

    Heater::SampleTimer::SampleTimer(Heater& heater_ref, TwoWire& wire_ref, uint8_t unit_id_ref, unsigned interval_ms)
        : process::Timer("Heater", interval_ms),
          wire_(wire_ref),
          heater_(heater_ref),
          unit_id_(unit_id_ref) {
    }

    void Heater::SampleTimer::callback() {
        // Toggle the heater output once per timer period (one second by default).
        heater_.heater_output_high_ = !heater_.heater_output_high_;
        digitalWrite(Heater::HEATER_PIN,
                     heater_.heater_output_high_ ? HIGH : LOW);

        // MCP3424の各CHの測定をキックする
        for (uint8_t ch = 0; ch < 4; ch++) {
            wire_.beginTransmission(MCP3424_ADDR);
            wire_.write((CONFIG_CH[ch] & 0xF3) |
                        static_cast<byte>(heater_.adc_resolution_));
            wire_.endTransmission();

        // 変換完了(RDY=0)まで待ちながらポーリング
        const uint8_t response_size =
            heater_.adc_resolution_ == AdcResolution::BIT_18 ? 4 : 3;
        byte b[4] = {};
        bool conversion_ready = false;
        unsigned long t0 = millis();
        while (millis() - t0 < heater_.conversionTimeoutMs()) {
        wire_.requestFrom(MCP3424_ADDR, response_size);
        if (wire_.available() != response_size) {
            delay(5); 
            continue; 
        }
        for (uint8_t i = 0; i < response_size; i++) {
            b[i] = wire_.read();
        }
        if (!(b[response_size - 1] & 0x80)) {
            conversion_ready = true;
            break; 
        }  // RDY=0 で変換完了
        delay(5);
        }

        // 電圧値を計算
        if (!conversion_ready) {
            continue;
        }

        int32_t rawADC;
        if (heater_.adc_resolution_ == AdcResolution::BIT_18) {
            rawADC = ((int32_t)b[0] << 16) | ((int32_t)b[1] << 8) | b[2];
            if (rawADC & 0x20000) {
                rawADC |= 0xFFFC0000;
            }
        } else {
            rawADC = (int16_t)(((uint16_t)b[0] << 8) | b[1]);
        }
        float vOut = rawADC * heater_.voltsPerCount();

        // 温度計算
        if (vOut > 0.05 && vOut < 2.00) { 
            float rThr = (V_REF * R_DOWNSTREAM / vOut) - R_UPSTREAM - R_DOWNSTREAM;
            float invT = (1.0 / T0) + (1.0 / B_CONSTANT) * std::log(rThr / R0);
            float tempCelsius = (1.0 / invT) - 273.15;
            CalculatedTemperature[ch] = tempCelsius;
        }
        }
        wcpp::Packet packet = newPacket(32);
        packet.telemetry(telemetry_id, component_id(), unit_id_, 0xFF,
                         kernel::nextPacketSequence(unit_id_, 0xFF, component_id(),
                                                    wcpp::packet_type_mask | telemetry_id));
        packet.append("Ca").setFloat32(CalculatedTemperature[0]);
        packet.append("Cb").setFloat32(CalculatedTemperature[1]);
        packet.append("Cc").setFloat32(CalculatedTemperature[2]);
        packet.append("Cd").setFloat32(CalculatedTemperature[3]);
        packet.append("Hs").setBool(heater_.heater_output_high_);
        packet.append("Ts").setInt((int)millis());
        sendPacket(packet);
    }
    }
