#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void wifi_manager_init();
bool wifi_manager_set_channel(int channel);
void wifi_manager_promiscuous_start();
void wifi_manager_promiscuous_stop();
bool wifi_manager_is_promiscuous();
String wifi_manager_get_mac_str();

#endif
