#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void wifi_manager_init();
bool wifi_manager_connect(const char* ssid, const char* password);
void wifi_manager_disconnect();
bool wifi_manager_is_connected();
String wifi_manager_get_ip();
String wifi_manager_get_mac_str();

#endif
