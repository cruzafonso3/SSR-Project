#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>

#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_DOMAIN_LEN 64

extern char g_wifi_ssid[MAX_SSID_LEN + 1];
extern char g_wifi_pass[MAX_PASS_LEN + 1];
extern uint8_t g_victim_mac[6];
extern IPAddress g_victim_ip;
extern uint8_t g_gateway_mac[6];
extern IPAddress g_gateway_ip;
extern char g_target_domain[MAX_DOMAIN_LEN + 1];
extern IPAddress g_spoofed_ip;
extern int g_arp_interval_ms;

void settingsInit();
void settingsSave();
void settingsReset();

void ipToString(const IPAddress& ip, char* buf, size_t len);
bool stringToIp(const char* str, IPAddress& ip);
void macToString(const uint8_t* mac, char* buf, size_t len);
bool stringToMac(const char* str, uint8_t* mac);

#endif
