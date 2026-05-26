#ifndef DHCP_STARVATION_H
#define DHCP_STARVATION_H

#include <Arduino.h>

void dhcp_init();
void dhcp_start();
void dhcp_stop();
bool dhcp_is_running();
void dhcp_step();
int dhcp_get_reconnect_count();
String dhcp_get_current_ip();
String dhcp_get_current_mac();
String dhcp_get_original_mac();
int dhcp_get_cooldown();

#endif
