#include "network_attacks.h"
#include "settings_manager.h"
#include "deauth_utils.h"
#include <WiFi.h>
#include <esp_wifi.h>

// Global definitions
uint8_t g_esp32_mac[6];
IPAddress g_esp32_ip;
uint8_t g_bssid[6];
uint8_t VICTIM_MAC[6];
IPAddress VICTIM_IP;
uint8_t GATEWAY_MAC[6];
IPAddress GATEWAY_IP;
char TARGET_DOMAIN_STR[64];
IPAddress SPOOFED_IP;

static bool attackStates[6] = {false};
static int attackCounters[6] = {0};

static bool promiscuousActive = false;

void promiscuousStart() {
    if (promiscuousActive) return;
    esp_wifi_set_promiscuous(true);
    promiscuousActive = true;
}

void promiscuousStop() {
    if (!promiscuousActive) return;
    esp_wifi_set_promiscuous(false);
    promiscuousActive = false;
}

void attacksInit() {
    WiFi.macAddress(g_esp32_mac);
    g_esp32_ip = WiFi.localIP();
    memset(g_bssid, 0, sizeof(g_bssid));
    if (WiFi.status() == WL_CONNECTED) {
        const uint8_t* bssid = WiFi.BSSID();
        if (bssid != nullptr) {
            memcpy(g_bssid, bssid, 6);
        }
    }
}

void attacksUpdate() {
    if (WiFi.status() == WL_CONNECTED) {
        g_esp32_ip = WiFi.localIP();
        memcpy(g_bssid, WiFi.BSSID(), 6);
    }
    
    attackCounters[ATTACK_DEAUTH] += deauthUpdate();
}

void attacksStart(int id) {
    if (id >= 1 && id <= 5) {
        attackStates[id] = true;
        if (id == ATTACK_DEAUTH) {
            deauthStart();
            promiscuousStart();
        }
    }
}

void attacksStop(int id) {
    if (id >= 1 && id <= 5) {
        attackStates[id] = false;
        if (id == ATTACK_DEAUTH) {
            deauthStop();
            promiscuousStop();
        }
    }
}

void attacksStopAll() {
    for (int i = 1; i <= 5; i++) attacksStop(i);
}

bool attacksIsRunning(int id) {
    return (id >= 1 && id <= 5) ? attackStates[id] : false;
}

int attacksGetCount(int id) {
    return (id >= 1 && id <= 5) ? attackCounters[id] : 0;
}
