#pragma once

#include <stdint.h>

namespace core {
namespace can_packet_format {

constexpr uint32_t frame_index_mask = 0x1F;

inline uint32_t makeId(uint8_t type_and_id, uint8_t component_id,
                       uint8_t origin_unit_id, uint8_t frame_index = 0) {
  return (static_cast<uint32_t>(type_and_id) << 21)
       | (static_cast<uint32_t>(component_id) << 13)
       | (static_cast<uint32_t>(origin_unit_id) << 5)
       | (frame_index & frame_index_mask);
}

inline uint8_t typeAndId(uint32_t can_id) {
  return static_cast<uint8_t>(can_id >> 21);
}

inline uint8_t componentId(uint32_t can_id) {
  return static_cast<uint8_t>(can_id >> 13);
}

inline uint8_t originUnitId(uint32_t can_id) {
  return static_cast<uint8_t>(can_id >> 5);
}

inline uint8_t frameIndex(uint32_t can_id) {
  return static_cast<uint8_t>(can_id & frame_index_mask);
}

}  // namespace can_packet_format
}  // namespace core
