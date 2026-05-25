#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ----- Hardware Pins -----
#define OLED_SDA        21
#define OLED_SCL        22
#define BTN_UP_PIN      14
#define BTN_DOWN_PIN    27
#define BTN_SELECT_PIN  26

// ----- Display -----
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
#define MENU_FONT       u8g2_font_6x10_tf
#define MENU_FONT_H     10
#define MENU_FONT_W     6
#define MENU_ITEMS_PER_PAGE 5

// ----- Defaults -----
#define DEF_WIFI_SSID       "YOUR_LAB_SSID"
#define DEF_WIFI_PASS       "YOUR_LAB_PASSWORD"
#define DEF_TARGET_DOMAIN   "example.com"
#define DEF_SPOOFED_IP      "192.168.1.200"
#define DEF_VICTIM_IP       "192.168.1.100"
#define DEF_VICTIM_MAC      "00:00:00:00:00:00"
#define DEF_GATEWAY_IP      "192.168.1.1"
#define DEF_GATEWAY_MAC     "00:00:00:00:00:00"
#define DEF_ARP_INTERVAL    500
#define DEF_DEAUTH_BURST    10
#define DEF_ROGUE_SSID      "Free_WiFi"

// ----- Attack IDs -----
#define ATTACK_NONE     0
#define ATTACK_ARP      1
#define ATTACK_DNS      2
#define ATTACK_DHCP     3
#define ATTACK_DEAUTH   4
#define ATTACK_ROGUE_AP 5

// ----- App States -----
enum AppState {
    STATE_SPLASH,
    STATE_MAIN_MENU,
    STATE_NET_ATTACKS,
    STATE_BT_TOOLS,
    STATE_BLE_SPOOFER,
    STATE_STATUS,
    STATE_SETTINGS,
    STATE_EDIT_FIELD,
    STATE_BLE_SCAN_RES,
    STATE_BT_SCAN_RES
};

// ----- Field Types -----
enum FieldType {
    FT_STRING,
    FT_IP,
    FT_MAC,
    FT_INT
};

#endif