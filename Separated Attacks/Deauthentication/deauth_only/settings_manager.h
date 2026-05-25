#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>
#include "config.h"

struct Settings {
    char wifi_ssid[33];
    char wifi_pass[65];
    char target_domain[33];
    IPAddress spoofed_ip;
    IPAddress victim_ip;
    uint8_t victim_mac[6];
    IPAddress gateway_ip;
    uint8_t gateway_mac[6];
    int arp_interval;
    int deauth_burst;
    char rogue_ap_ssid[33];
};

extern Settings g_settings;

void settingsInit();
bool settingsLoad();
bool settingsSave();
void settingsResetDefaults();
void settingsApplyToGlobals();

#endif