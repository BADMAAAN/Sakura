#include "ram_profile.h"

#if defined(ENABLE_RAM_LOGGING)

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <inttypes.h>

void ramProfileLog(const char *stage) {
    // Default (8-bit capable) heap as reported by the Arduino layer.
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeEver = ESP.getMinFreeHeap();

    // INTERNAL DRAM is the metric that matters for Wi-Fi/BLE init on boards
    // without PSRAM: free total and, crucially, the LARGEST CONTIGUOUS block.
    uint32_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t internalLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    // DMA-capable internal block (some Wi-Fi/BLE buffers require this).
    uint32_t dmaLargest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

    uint32_t psramFree = ESP.getFreePsram();

    Serial.printf(
        "[RAMLOG] t=%6lums stage=%-20s | heap free=%7" PRIu32 " minEver=%7" PRIu32
        " | internal free=%7" PRIu32 " largest=%7" PRIu32 " dma=%7" PRIu32
        " | psram free=%8" PRIu32 "\n",
        millis(),
        stage,
        freeHeap,
        minFreeEver,
        internalFree,
        internalLargest,
        dmaLargest,
        psramFree
    );
    Serial.flush();
}

#endif // ENABLE_RAM_LOGGING
