#ifndef ROGUE_AP_CONFIG_H
#define ROGUE_AP_CONFIG_H

#include <Arduino.h>

#define AP_IP_ADDR          IPAddress(192, 168, 4, 1) // 192.168.4.1
#define AP_SUBNET           IPAddress(255, 255, 255, 0)
#define DNS_PORT            53

#define CRED_FILE_PATH      "/creds.txt"
#define CONFIG_FILE_PATH    "/ap_config.json"

#define SCAN_INTERVAL_MS    15000UL
#define AP_SWITCH_DELAY_MS  500

#define WIFI_COUNTRY_CC     "CN"
#define WIFI_COUNTRY_SCHAN  1
#define WIFI_COUNTRY_NCHAN  13

#define DEFAULT_AP_SSID     "RogueAP"
#define DEFAULT_AP_PASS     "password123"

#define MAX_SCAN_RESULTS    20

#define AUTO_STOP_PRESETS   {5, 15, 30, 60, 120}
#define AUTO_STOP_DEFAULT   false
#define AUTO_STOP_MINUTES   30

#endif
