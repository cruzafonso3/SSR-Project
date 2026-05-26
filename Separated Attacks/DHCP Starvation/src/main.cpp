#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0;
    }
}

#include "config.h"
#include "wifi_manager.h"
#include "dhcp_starvation.h"
#include "serial_cli.h"
#include "display_manager.h"
#include "buttons.h"
#include "ui_menu.h"

static void app_task(void* pvParameters) {
    while (1) {
        serial_cli_task();
        dhcp_step();
        ui_menu_update();
        ui_menu_render();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup() {
    config_init();
    wifi_manager_init();
    dhcp_init();
    serial_cli_init();
    display_manager_init();
    ui_menu_init();
    buttons_init();
    xTaskCreatePinnedToCore(app_task, "app_task", 4096, NULL, 1, NULL, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
