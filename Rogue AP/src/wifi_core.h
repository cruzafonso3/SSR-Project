#ifndef ROGUE_AP_WIFI_CORE_H
#define ROGUE_AP_WIFI_CORE_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include "config.h"
#include "types.h"

void configureCountryRegion();
void scanNearbyNetworks(std::vector<NetworkRecord>& results);
bool launchRogueAP(const NetworkRecord& target, IPAddress apIp);
void haltRogueAP();
uint8_t getConnectedClientCount();
const char* getDefaultSSID();

#endif
