#pragma once

#include "library/kernel/kernel.h"
#include "library/kernel/listener.h"

namespace process {

#define LOG(...) log( __FILE__, __LINE__, __VA_ARGS__)

class Component;

class Process {
public:
  using Listener = kernel::Listener;

  Process(const char* name) : name_(name){};

  inline void delay(unsigned ms) { vTaskDelay(ms); }

  void listen(kernel::Listener &listener, unsigned queue_size = 1, bool force_push = true);

  inline void start(Process& sub_process) { sub_process.startProcess(component_id_); };

  uint8_t component_id() const { return component_id_; };

  wcpp::Packet newPacket(uint8_t size);
  wcpp::Packet decodePacket(const uint8_t* buf);
  void sendPacket(const wcpp::Packet &packet);
  void sendPacket(const wcpp::Packet &packet, const Listener& exclude);

  wcpp::Packet loadPacket(uint8_t packet_id);
  bool storePacket(const wcpp::Packet& packet);

  template <class... Args>
  void log(const char* file, unsigned line, const char* format, Args... args) {
    char message[240];
    int len = snprintf(message, 240, format, args...);
    wcpp::Packet p = newPacket(4 + 2 + strlen(file) + 2 + 8 + 2 + len);
    p.telemetry(packet_id_log, component_id());
    p.append("Fn").setString(file);
    p.append("Ln").setInt(line);
    p.append("Ms").setString(message);
    sendPacket(p);
  }

  template <class... Args>
  void error(const char* code, const char* format, Args... args) {
    uint8_t buf[64];
    wcpp::Packet p = wcpp::Packet::empty(buf, 64);
    p.telemetry(packet_id_error, component_id());
    p.append("Cd").setString(code);
    auto e = p.append("Ms");
    snprintf(reinterpret_cast<char*>(buf) + p.size(), 64 - p.size(), format, args...);
    e.setString(reinterpret_cast<char*>(buf) + p.size());
    sendPacket(p);
  }

protected:
  const char *name_;
  uint8_t component_id_ = 0xFF;

  void startProcess(uint8_t component_id);
  virtual bool onStart() = 0;
};

}


