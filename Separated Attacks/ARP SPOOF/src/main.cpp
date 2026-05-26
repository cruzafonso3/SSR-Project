#include <Arduino.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0;
    }
}

#include "config.h"
#include "serial_cli.h"
#include "wifi_manager.h"
#include "arp_engine.h"
#include "packet_forwarder.h"
#include "display_manager.h"
#include "buttons.h"
#include "ui_menu.h"

static void app_task(void* pvParameters) {
    while (1) {
        serial_cli_task();
        arp_engine_task();
        ui_menu_update();
        ui_menu_render();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    config_init();
    wifi_manager_init();
    arp_engine_init();
    packet_forwarder_init();
    serial_cli_init();
    display_manager_init();
    ui_menu_init();
    buttons_init();
    xTaskCreatePinnedToCore(app_task, "app_task", 4096, NULL, 1, NULL, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(100));
}
