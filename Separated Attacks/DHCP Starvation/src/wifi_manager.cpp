#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_wifi.h>

void wifi_manager_init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
}

bool wifi_manager_connect(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);
    Serial.print("[WiFi] Connecting");
    int retries = 40;
    while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Failed");
    return false;
}

void wifi_manager_disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
}

bool wifi_manager_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

String wifi_manager_get_ip() {
    return WiFi.localIP().toString();
}

String wifi_manager_get_mac_str() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
