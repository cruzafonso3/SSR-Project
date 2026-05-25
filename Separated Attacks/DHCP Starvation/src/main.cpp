#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
    int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
        return 0;
    }
}

#include "wifi_manager.h"
#include "dhcp_glutton.h"
#include "serial_cli.h"

static DHCPGlutton* s_glutton = nullptr;

static void app_task(void* pvParameters) {
    while (1) {
        serial_cli_task();
        if (s_glutton && s_glutton->is_running()) {
            s_glutton->step();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup() {
    wifi_manager_init();
    serial_cli_init();
    xTaskCreatePinnedToCore(app_task, "app_task", 4096, NULL, 1, NULL, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}

DHCPGlutton* get_glutton() {
    return s_glutton;
}

void create_glutton(const char* ssid, const char* pass, int cooldown) {
    if (s_glutton) delete s_glutton;
    s_glutton = new DHCPGlutton(ssid, pass, cooldown);
}

void destroy_glutton() {
    if (s_glutton) {
        delete s_glutton;
        s_glutton = nullptr;
    }
}
