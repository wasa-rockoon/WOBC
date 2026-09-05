#pragma once
#include <Arduino.h>
#include <map>
#include <string>
#include <library/wcpp/cpp/packet.h>
using pin_t = uint8_t;
namespace kernel {
extern std::map<const uint8_t*, int> references;
inline void refChange(const wcpp::Packet& packet, int change) {
    auto it = references.find(packet.getBuf());
    if (it != references.end() && (it->second += change) == 0) {
        delete[] it->first;
        references.erase(it);
    }
}
struct Kernel {
    wcpp::Packet allocPacket(uint8_t size) {
        auto buf = new uint8_t[size]();
        references[buf] = 1;
        return wcpp::Packet::empty(buf, size, refChange);
    }
};
extern Kernel kernel_;
}
namespace process {
class Component {
public:
    std::vector<wcpp::Packet> published;
    std::vector<std::string> logs;
    Component(const char*, uint8_t) {}
    virtual ~Component() = default;
    virtual void setup() {}
    virtual void loop() {}
    virtual void onCommand(const wcpp::Packet&) {}
    wcpp::Packet newPacket(uint8_t size) { return kernel::kernel_.allocPacket(size); }
    void sendPacket(const wcpp::Packet& packet) { published.push_back(packet); }
    void log(const char* text) { logs.emplace_back(text); }
    template<class... Args> void log(const char* format, Args... args) {
        char text[256]; snprintf(text, sizeof(text), format, args...); logs.emplace_back(text);
    }
};
}
#define LOG(...) log(__VA_ARGS__)
