#include "gps.h"
#include <Arduino.h>

namespace component {
GPS::GPS(int rxPin, int txPin, uint32_t gpsBaud, uint8_t unit_id , unsigned sample_freq_hz)
    : process::Component("GPS", component_id),
    rxPin_(rxPin), 
    txPin_(txPin), 
    gpsBaud_(gpsBaud),
    ss_(2),
    unit_id_(unit_id),
    sample_timer_(ss_ ,*this, gps_, unit_id, 1000 / sample_freq_hz)
    {    
}

void GPS::setup(){
    ss_.begin(gpsBaud_, SERIAL_8N1, rxPin_, txPin_);
    start(sample_timer_);
}

GPS::SampleTimer::SampleTimer(HardwareSerial& ss_ref, GPS& gps_ref, TinyGPSPlus& tinygps_ref, uint8_t unit_id_ref, unsigned interval_ms)
  : process::Timer("GPS", interval_ms),
    gps_(tinygps_ref), GPS_(gps_ref), unit_id_(unit_id_ref), ss_(ss_ref){
}

void GPS::SampleTimer::callback(){
    int start = millis();
    while(ss_.available() && millis()-start<500){
        gps_.encode(ss_.read());
    }
    wcpp::Packet packet = newPacket(64);
    packet.telemetry(telemetry_id, component_id(), unit_id_, 0xFF, 1234);
    packet.append("LA").setFloat64(gps_.location.lat());
    packet.append("LO").setFloat64(gps_.location.lng());
    packet.append("AL").setInt((int)gps_.altitude.meters());
    
    // 時刻情報の文字列を作成
    char timeStr[24];
    sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d", 
            gps_.date.year(), gps_.date.month(), gps_.date.day(),
            gps_.time.hour(), gps_.time.minute(), gps_.time.second());
    
    // 文字列としてパケットに追加
    packet.append("UT").setString(timeStr);

    
    sendPacket(packet);
    }
}