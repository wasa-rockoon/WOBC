#pragma once
#include <cassert>
using SemaphoreHandle_t = bool*;
constexpr unsigned portMAX_DELAY = ~0u;
constexpr int pdTRUE = 1;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new bool(false); }
inline int xSemaphoreTake(SemaphoreHandle_t mutex, unsigned wait) {
    if (*mutex) { assert(wait == 0); return 0; }
    *mutex = true;
    return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t mutex) { assert(*mutex); *mutex = false; }
