#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_wifi.h>

static uint8_t s_bssid[6];

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
        Serial.printf("[WiFi] Connected, IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        memcpy(s_bssid, WiFi.BSSID(), 6);
        return true;
    }
    Serial.println("[WiFi] Connection failed");
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

void wifi_manager_get_mac(uint8_t* mac) {
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

String wifi_manager_get_mac_str() {
    uint8_t mac[6];
    wifi_manager_get_mac(mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

uint8_t* wifi_manager_get_bssid() {
    return s_bssid;
}
