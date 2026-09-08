#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

using pin_t = uint8_t;

struct FakeQueue {
  unsigned capacity;
  unsigned item_size;
  std::deque<std::vector<uint8_t>> items;
};
using QueueHandle_t = FakeQueue*;
constexpr int pdPASS = 1;

inline QueueHandle_t xQueueCreate(unsigned capacity, unsigned item_size) {
  return new FakeQueue{capacity, item_size, {}};
}

inline int xQueueSendFromISR(QueueHandle_t queue, const void* item, void*) {
  if (queue->items.size() == queue->capacity) return 0;
  const auto* bytes = static_cast<const uint8_t*>(item);
  queue->items.emplace_back(bytes, bytes + queue->item_size);
  return pdPASS;
}

inline int xQueueReceive(QueueHandle_t queue, void* item, unsigned) {
  if (queue->items.empty()) return 0;
  std::memcpy(item, queue->items.front().data(), queue->item_size);
  queue->items.pop_front();
  return pdPASS;
}

inline uint32_t millis() { return 1000; }
