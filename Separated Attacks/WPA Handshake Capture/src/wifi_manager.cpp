#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_wifi.h>

void wifi_manager_init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
}

bool wifi_manager_set_channel(int channel) {
    if (channel < 1 || channel > 14) return false;
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    return err == ESP_OK;
}

void wifi_manager_promiscuous_start() {
    esp_wifi_set_promiscuous(true);
}

void wifi_manager_promiscuous_stop() {
    esp_wifi_set_promiscuous(false);
}

bool wifi_manager_is_promiscuous() {
    bool en = false;
    esp_wifi_get_promiscuous(&en);
    return en;
}

String wifi_manager_get_mac_str() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
