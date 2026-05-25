#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void wifi_manager_init();
bool wifi_manager_connect(const char* ssid, const char* password);
void wifi_manager_disconnect();
bool wifi_manager_is_connected();
void wifi_manager_get_mac(uint8_t* mac);
String wifi_manager_get_mac_str();
uint8_t* wifi_manager_get_bssid();

#endif
