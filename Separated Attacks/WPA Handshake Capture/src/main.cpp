#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0;
    }
}

#include "wifi_manager.h"
#include "handshake_sniffer.h"
#include "serial_cli.h"

static void app_task(void* pvParameters) {
    while (1) {
        serial_cli_task();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    wifi_manager_init();
    sniffer_init();
    serial_cli_init();
    xTaskCreatePinnedToCore(app_task, "app_task", 8192, NULL, 1, NULL, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
