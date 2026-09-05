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
    
    Heater::Heater(TwoWire& wire, uint8_t unit_id, unsigned sample_freq_hz)
        : process::Component("Heater", component_id),
          wire_(wire),
          unit_id_(unit_id),
          sample_timer_(*this, wire_, unit_id_, sample_freq_hz > 0 ? 1000 / sample_freq_hz : 1000) {
    }

    void Heater::setup() {
        Wire.beginTransmission(MCP3424_ADDR);
        if (Wire.endTransmission() != 0) {
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
        // MCP3424の各CHの測定をキックする
        for (uint8_t ch = 0; ch < 4; ch++) {
            wire_.beginTransmission(MCP3424_ADDR);
            wire_.write(CONFIG_CH[ch]);
            wire_.endTransmission();

        // 変換完了(RDY=0)まで待ちながらポーリング
        byte b[3];
        unsigned long t0 = millis();
        while (millis() - t0 < 300) {
        Wire.requestFrom(MCP3424_ADDR, 3);
        if (Wire.available() != 3) {
            delay(5); 
            continue; 
        }
        for (int i = 0; i < 3; i++) {
            b[i] = Wire.read();
        }
        if (!(b[2] & 0x80)) {
            break; 
        }  // RDY=0 で変換完了
        delay(5);
        }

        // 電圧値を計算
        int16_t rawADC = (int16_t)(((uint16_t)b[0] << 8) | b[1]);
        float vOut = rawADC * 0.0000625f;

        // 温度計算
        if (vOut > 0.05 && vOut < 2.00) { 
            float rThr = (V_REF * R_DOWNSTREAM / vOut) - R_UPSTREAM - R_DOWNSTREAM;
            float invT = (1.0 / T0) + (1.0 / B_CONSTANT) * std::log(rThr / R0);
            float tempCelsius = (1.0 / invT) - 273.15;
            CalculatedTemperature[ch] = tempCelsius;
        }
        }
        wcpp::Packet packet = newPacket(32);
        packet.telemetry(telemetry_id, component_id(), unit_id_, 0xFF, 1234);
        packet.append("Ca").setFloat16(CalculatedTemperature[0]);
        packet.append("Cb").setFloat16(CalculatedTemperature[1]);
        packet.append("Cc").setFloat16(CalculatedTemperature[2]);
        packet.append("Bv").setFloat16(CalculatedTemperature[3]);
        packet.append("Ts").setInt((int)millis());
        sendPacket(packet);
    }
    }