#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>

#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_DOMAIN_LEN 64

// ========== FACTORY DEFAULTS ==========
// Change these before flashing if you want different startup values.
// They will appear in the Settings menu on first boot or after Factory Reset.
#define DEFAULT_WIFI_SSID     "Cruz"
#define DEFAULT_WIFI_PASS     "passemaluca!!!"
#define DEFAULT_VICTIM_IP     "172.20.10.3"
#define DEFAULT_VICTIM_MAC    "E0:D4:64:C2:C5:54"
#define DEFAULT_GATEWAY_IP    "172.20.10.1"
#define DEFAULT_GATEWAY_MAC   "F6:52:93:84:2E:64"
#define DEFAULT_TARGET_DOMAIN "example.com"
#define DEFAULT_SPOOFED_IP    "192.168.1.200"
#define DEFAULT_ARP_INTERVAL  2000
#define SETTINGS_VERSION      1   // Bump to force a reset to defaults on flash
// ======================================

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
