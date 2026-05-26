#include "ui_menu.h"
#include "display_manager.h"
#include "buttons.h"
#include "config.h"
#include "wifi_manager.h"
#include "arp_engine.h"
#include "packet_forwarder.h"
#include <WiFi.h>

// ─── Pages ──────────────────────────────────────────────
enum Page {
    PAGE_MAIN,
    PAGE_MESSAGE,
    PAGE_STATUS,
    PAGE_SETTINGS,
    PAGE_EDIT_STRING,
    PAGE_EDIT_IP,
    PAGE_EDIT_MAC,
    PAGE_EDIT_INT,
    PAGE_CONFIRM,
};

static Page s_page = PAGE_MAIN;
static int s_cursor = 0;
static int s_scroll = 0;
static unsigned long s_msg_until = 0;

// ─── Main menu ──────────────────────────────────────────
static const char* MAIN_ITEMS[] = {
    "Connect WiFi",
    "Disconnect WiFi",
    "Start ARP",
    "Stop ARP",
    "Toggle Forwarding",
    "Status",
    "Settings",
    "Save Config",
};
#define MAIN_COUNT 8

// ─── Settings menu ──────────────────────────────────────
static const char* SETTINGS_ITEMS[] = {
    "SSID",
    "Password",
    "Victim IP",
    "Victim MAC",
    "Gateway IP",
    "Gateway MAC",
    "ARP Cooldown",
    "Factory Reset",
    "Back",
};
#define SETTINGS_COUNT 9

// ─── Status lines ───────────────────────────────────────
#define MAX_STATUS 28
static String s_status_lines[MAX_STATUS];
static int s_status_count = 0;
static int s_status_scroll = 0;

static void rebuild_status() {
    s_status_count = 0;
    uint8_t mac[6], bssid[6];
    wifi_manager_get_mac(mac);
    memcpy(bssid, wifi_manager_get_bssid(), 6);
    uint32_t cap, fwd, drop;
    packet_forwarder_get_stats(cap, fwd, drop);

    auto add = [](const String& s) { if (s_status_count < MAX_STATUS) s_status_lines[s_status_count++] = s; };
    char buf[22];

    add("  -- NETWORK --");
    snprintf(buf, sizeof(buf), " SSID: %s", g_config.ssid);
    add(buf);
    add(" PASS: ********");
    snprintf(buf, sizeof(buf), " MAC: %02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    add(buf);
    if (wifi_manager_is_connected()) {
        add(" IP: " + WiFi.localIP().toString());
        snprintf(buf, sizeof(buf), " BSSID: %02X%02X%02X%02X%02X%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
        add(buf);
    } else {
        add(" WiFi: DISCONNECTED");
    }

    add("");
    add("  -- VICTIM --");
    snprintf(buf, sizeof(buf), " IP: %s", g_config.target_ip);
    add(buf);
    snprintf(buf, sizeof(buf), " MAC: %s", g_config.target_mac);
    add(buf);

    add("");
    add("  -- GATEWAY --");
    snprintf(buf, sizeof(buf), " IP: %s", g_config.gateway_ip);
    add(buf);
    snprintf(buf, sizeof(buf), " MAC: %s", g_config.gateway_mac);
    add(buf);

    add("");
    add("  -- STATE --");
    add(String(" ARP: ") + (arp_engine_is_running() ? "ON" : "OFF"));
    add(String(" FWD: ") + (packet_forwarder_is_running() ? "ON" : "OFF"));
    snprintf(buf, sizeof(buf), " COOLDOWN: %d ms", g_config.arp_cooldown_ms);
    add(buf);

    add("");
    add("  -- STATS --");
    snprintf(buf, sizeof(buf), " CAPTURED: %u", cap);
    add(buf);
    snprintf(buf, sizeof(buf), " FORWARD: %u", fwd);
    add(buf);
    snprintf(buf, sizeof(buf), " DROPPED: %u", drop);
    add(buf);

    if (s_status_scroll > s_status_count - 6) s_status_scroll = s_status_count - 6;
    if (s_status_scroll < 0) s_status_scroll = 0;
}

// ─── Editor state ───────────────────────────────────────
enum EditType { EDIT_IP, EDIT_MAC, EDIT_INT, EDIT_STRING };
static EditType s_edit_type;

static const char* s_edit_title = "";
static char* s_edit_target_str = nullptr;

// IP editor
static uint8_t s_edit_ip[4];
static int s_edit_ip_idx;

// MAC editor
static uint8_t s_edit_mac[6];
static int s_edit_mac_idx;

// Int editor
static int s_edit_int_val;
static int s_edit_int_min;
static int s_edit_int_max;
static int s_edit_int_step;
static int* s_edit_int_ptr;

// String editor
static char s_edit_str_buf[65];
static int s_edit_str_max;
static int s_edit_str_charset_idx;
static const char STRING_CHARSET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    " .-_!@#$%^&*()[]{}|;:'\",<>?/`~\\=+"
    "\x01\x02\x03";
#define CHARSET_LEN (sizeof(STRING_CHARSET) - 1)

// Confirm dialog
static int s_confirm_cursor = 0;
static bool (*s_confirm_action)() = nullptr;

// ─── Helpers ────────────────────────────────────────────
static void clamp_scroll(int cursor, int& scroll, int max_visible, int count) {
    (void)cursor;
    if (scroll > count - max_visible) scroll = count - max_visible;
    if (scroll < 0) scroll = 0;
}

static void ip_to_str(const uint8_t* ip, char* buf, size_t len) {
    snprintf(buf, len, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

static void str_to_ip(const char* str, uint8_t* ip) {
    int a, b, c, d;
    if (sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        ip[0] = a; ip[1] = b; ip[2] = c; ip[3] = d;
    }
}

static void mac_to_str(const uint8_t* mac, char* buf, size_t len) {
    snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void str_to_mac(const char* str, uint8_t* mac) {
    unsigned int v[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
        for (int i = 0; i < 6; i++) mac[i] = v[i];
    }
}

static void save_edit() {
    char buf[20];
    switch (s_edit_type) {
        case EDIT_IP:
            ip_to_str(s_edit_ip, buf, sizeof(buf));
            if (s_edit_target_str) strncpy(s_edit_target_str, buf, 16);
            break;
        case EDIT_MAC:
            mac_to_str(s_edit_mac, buf, sizeof(buf));
            if (s_edit_target_str) strncpy(s_edit_target_str, buf, 18);
            break;
        case EDIT_INT:
            if (s_edit_int_ptr) *s_edit_int_ptr = s_edit_int_val;
            break;
        case EDIT_STRING:
            if (s_edit_target_str) strncpy(s_edit_target_str, s_edit_str_buf, s_edit_str_max);
            break;
    }
    config_update_cache();
    config_save();
    s_page = PAGE_SETTINGS;
}

static void cancel_edit() {
    s_page = PAGE_SETTINGS;
}

// ─── Actions ────────────────────────────────────────────
static void msg_temp(const char* l1, const char* l2, const char* l3, unsigned long ms) {
    display_show_message(l1, l2, l3);
    s_page = PAGE_MESSAGE;
    s_msg_until = millis() + ms;
}

static void action_start_arp() {
    if (!wifi_manager_is_connected()) { msg_temp("Not connected!", nullptr, nullptr, 1500); return; }
    if (!config_validate()) { msg_temp("Invalid config!", nullptr, nullptr, 1500); return; }
    arp_engine_start();
    packet_forwarder_start();
    msg_temp("ARP started", "Forwarding ON", nullptr, 1500);
    Serial.println("[Menu] ARP + Forward started");
}

static void action_stop_arp() {
    arp_engine_stop();
    packet_forwarder_stop();
    msg_temp("ARP stopped", "Forwarding OFF", nullptr, 1500);
    Serial.println("[Menu] ARP + Forward stopped");
}

static void action_toggle_fwd() {
    if (!wifi_manager_is_connected()) { msg_temp("Not connected!", nullptr, nullptr, 1500); return; }
    if (packet_forwarder_is_running()) {
        packet_forwarder_stop();
        msg_temp("Forwarding OFF", nullptr, nullptr, 1500);
    } else {
        if (!config_validate()) { msg_temp("Invalid config!", nullptr, nullptr, 1500); return; }
        packet_forwarder_start();
        msg_temp("Forwarding ON", nullptr, nullptr, 1500);
    }
}

static void action_connect() {
    if (strlen(g_config.ssid) == 0) { msg_temp("No SSID set!", nullptr, nullptr, 1500); return; }
    if (wifi_manager_is_connected()) { msg_temp("Already connected", nullptr, nullptr, 1500); return; }
    display_show_wifi_connecting();
    bool ok = wifi_manager_connect(g_config.ssid, g_config.password);
    if (ok) {
        msg_temp("WiFi connected", WiFi.localIP().toString().c_str(), nullptr, 2000);
    } else {
        msg_temp("WiFi failed!", nullptr, nullptr, 1500);
    }
}

static void action_disconnect() {
    arp_engine_stop();
    packet_forwarder_stop();
    wifi_manager_disconnect();
    msg_temp("WiFi disconnected", nullptr, nullptr, 1500);
}

static void action_show_status() {
    rebuild_status();
    s_status_scroll = 0;
    s_page = PAGE_STATUS;
}

// ─── Editors entry points ───────────────────────────────
static void start_ip_editor(char* target, const char* title) {
    s_edit_type = EDIT_IP;
    s_edit_title = title;
    s_edit_target_str = target;
    str_to_ip(target, s_edit_ip);
    s_edit_ip_idx = 0;
    s_page = PAGE_EDIT_IP;
}

static void start_mac_editor(char* target, const char* title) {
    s_edit_type = EDIT_MAC;
    s_edit_title = title;
    s_edit_target_str = target;
    str_to_mac(target, s_edit_mac);
    s_edit_mac_idx = 0;
    s_page = PAGE_EDIT_MAC;
}

static void start_int_editor(int* target, const char* title, int min, int max, int step) {
    s_edit_type = EDIT_INT;
    s_edit_title = title;
    s_edit_int_ptr = target;
    s_edit_int_val = *target;
    s_edit_int_min = min;
    s_edit_int_max = max;
    s_edit_int_step = step;
    s_page = PAGE_EDIT_INT;
}

static void start_string_editor(char* target, int max_len, const char* title) {
    s_edit_type = EDIT_STRING;
    s_edit_title = title;
    s_edit_target_str = target;
    s_edit_str_max = max_len;
    strncpy(s_edit_str_buf, target, sizeof(s_edit_str_buf));
    s_edit_str_buf[sizeof(s_edit_str_buf) - 1] = 0;
    s_edit_str_charset_idx = 0;
    s_page = PAGE_EDIT_STRING;
}

// ─── Init ───────────────────────────────────────────────
void ui_menu_init() {
    s_page = PAGE_MAIN;
    s_cursor = 0;
    s_scroll = 0;
    display_show_splash();
}

// ─── Update (call every loop) ───────────────────────────
void ui_menu_update() {
    ButtonEvent evt = buttons_get_event();
    if (evt == EVT_NONE) return;

    if (s_page == PAGE_MESSAGE) {
        if (millis() >= s_msg_until) { s_page = PAGE_MAIN; s_cursor = 0; s_scroll = 0; }
        return;
    }

    switch (s_page) {
        case PAGE_MAIN: {
            if (evt == EVT_UP) { s_cursor--; if (s_cursor < 0) s_cursor = MAIN_COUNT - 1; }
            if (evt == EVT_DOWN) { s_cursor++; if (s_cursor >= MAIN_COUNT) s_cursor = 0; }
            if (evt == EVT_SELECT) {
                switch (s_cursor) {
                    case 0: action_connect(); break;
                    case 1: action_disconnect(); break;
                    case 2: action_start_arp(); break;
                    case 3: action_stop_arp(); break;
                    case 4: action_toggle_fwd(); break;
                    case 5: action_show_status(); break;
                    case 6: s_cursor = 0; s_scroll = 0; s_page = PAGE_SETTINGS; break;
                    case 7: config_save(); msg_temp("Config saved", nullptr, nullptr, 1500); break;
                }
            }
            break;
        }

        case PAGE_STATUS: {
            if (evt == EVT_UP) { s_status_scroll--; if (s_status_scroll < 0) s_status_scroll = 0; }
            if (evt == EVT_DOWN) {
                s_status_scroll++;
                if (s_status_scroll > s_status_count - 6) s_status_scroll = s_status_count - 6;
                if (s_status_scroll < 0) s_status_scroll = 0;
            }
            if (evt == EVT_SELECT || evt == EVT_BACK) { s_cursor = 0; s_scroll = 0; s_page = PAGE_MAIN; }
            break;
        }

        case PAGE_SETTINGS: {
            if (evt == EVT_UP) { s_cursor--; if (s_cursor < 0) s_cursor = SETTINGS_COUNT - 1; }
            if (evt == EVT_DOWN) { s_cursor++; if (s_cursor >= SETTINGS_COUNT) s_cursor = 0; }
            if (evt == EVT_BACK) { s_cursor = 0; s_scroll = 0; s_page = PAGE_MAIN; }
            if (evt == EVT_SELECT) {
                switch (s_cursor) {
                    case 0: start_string_editor(g_config.ssid, sizeof(g_config.ssid), "EDIT SSID"); break;
                    case 1: start_string_editor(g_config.password, sizeof(g_config.password), "EDIT PASSWORD"); break;
                    case 2: start_ip_editor(g_config.target_ip, "EDIT VICTIM IP"); break;
                    case 3: start_mac_editor(g_config.target_mac, "EDIT VICTIM MAC"); break;
                    case 4: start_ip_editor(g_config.gateway_ip, "EDIT GATEWAY IP"); break;
                    case 5: start_mac_editor(g_config.gateway_mac, "EDIT GATEWAY MAC"); break;
                    case 6: start_int_editor(&g_config.arp_cooldown_ms, "ARP COOLDOWN", 100, 10000, 100); break;
                    case 7: s_confirm_cursor = 0; s_page = PAGE_CONFIRM; break;
                    case 8: s_cursor = 0; s_scroll = 0; s_page = PAGE_MAIN; break;
                }
            }
            break;
        }

        case PAGE_EDIT_IP: {
            if (evt == EVT_UP) { s_edit_ip[s_edit_ip_idx]++; }
            if (evt == EVT_DOWN) { s_edit_ip[s_edit_ip_idx]--; }
            if (evt == EVT_SELECT) {
                s_edit_ip_idx++;
                if (s_edit_ip_idx >= 4) { save_edit(); }
            }
            if (evt == EVT_BACK) { cancel_edit(); }
            break;
        }

        case PAGE_EDIT_MAC: {
            if (evt == EVT_UP) { s_edit_mac[s_edit_mac_idx]++; }
            if (evt == EVT_DOWN) { s_edit_mac[s_edit_mac_idx]--; }
            if (evt == EVT_SELECT) {
                s_edit_mac_idx++;
                if (s_edit_mac_idx >= 6) { save_edit(); }
            }
            if (evt == EVT_BACK) { cancel_edit(); }
            break;
        }

        case PAGE_EDIT_INT: {
            if (evt == EVT_UP) { s_edit_int_val += s_edit_int_step; if (s_edit_int_val > s_edit_int_max) s_edit_int_val = s_edit_int_max; }
            if (evt == EVT_DOWN) { s_edit_int_val -= s_edit_int_step; if (s_edit_int_val < s_edit_int_min) s_edit_int_val = s_edit_int_min; }
            if (evt == EVT_SELECT) { save_edit(); }
            if (evt == EVT_BACK) { cancel_edit(); }
            break;
        }

        case PAGE_EDIT_STRING: {
            if (evt == EVT_UP) { s_edit_str_charset_idx++; if (s_edit_str_charset_idx >= (int)CHARSET_LEN) s_edit_str_charset_idx = 0; }
            if (evt == EVT_DOWN) { s_edit_str_charset_idx--; if (s_edit_str_charset_idx < 0) s_edit_str_charset_idx = CHARSET_LEN - 1; }
            if (evt == EVT_SELECT) {
                char c = STRING_CHARSET[s_edit_str_charset_idx];
                int len = strlen(s_edit_str_buf);
                if (c == 1) {
                    if (len < s_edit_str_max - 1) { s_edit_str_buf[len] = ' '; s_edit_str_buf[len + 1] = 0; }
                } else if (c == 2) {
                    if (len > 0) s_edit_str_buf[len - 1] = 0;
                } else if (c == 3) {
                    save_edit();
                } else {
                    if (len < s_edit_str_max - 1) { s_edit_str_buf[len] = c; s_edit_str_buf[len + 1] = 0; }
                }
            }
            if (evt == EVT_BACK) { cancel_edit(); }
            break;
        }

        case PAGE_CONFIRM: {
            if (evt == EVT_UP) { s_confirm_cursor = 0; }
            if (evt == EVT_DOWN) { s_confirm_cursor = 1; }
            if (evt == EVT_SELECT) {
                if (s_confirm_cursor == 1) {
                    config_reset();
                    msg_temp("Factory reset done", nullptr, nullptr, 1500);
                    s_page = PAGE_MESSAGE;
                } else {
                    s_page = PAGE_SETTINGS;
                }
            }
            if (evt == EVT_BACK) { s_page = PAGE_SETTINGS; }
            break;
        }

        default:
            break;
    }
}

// ─── Render ─────────────────────────────────────────────
void ui_menu_render() {
    if (s_page == PAGE_MESSAGE) return;

    if (s_page == PAGE_MAIN) {
        displayClearBuffer();
        displayDrawStr(0, 8, "=== ARP SPOOF ===");
        displayDrawLine(0, 10, 127, 10);

        int max_vis = 4;
        if (s_cursor < s_scroll) s_scroll = s_cursor;
        if (s_cursor >= s_scroll + max_vis) s_scroll = s_cursor - max_vis + 1;

        for (int i = 0; i < max_vis; i++) {
            int idx = s_scroll + i;
            if (idx >= MAIN_COUNT) break;
            int y = 22 + i * 10;
            if (idx == s_cursor) {
                displayDrawBox(0, y - 8, 128, 10);
                displaySetDrawColor(0);
            }
            displayDrawStr(4, y, MAIN_ITEMS[idx]);
            if (idx == s_cursor) displaySetDrawColor(1);
        }

        if (s_scroll > 0) {
            g_display.fillTriangle(124, 1, 120, 5, 128, 5, SSD1306_WHITE);
        }
        if (s_scroll + max_vis < MAIN_COUNT) {
            g_display.fillTriangle(124, 62, 120, 58, 128, 58, SSD1306_WHITE);
        }

        displaySendBuffer();
        return;
    }

    if (s_page == PAGE_STATUS) {
        rebuild_status();
        displayClearBuffer();
        displayDrawStr(0, 8, "=== STATUS ===");
        displayDrawLine(0, 10, 127, 10);

        for (int i = 0; i < 6; i++) {
            int idx = s_status_scroll + i;
            if (idx >= s_status_count) break;
            int y = 20 + i * 8;
            displayDrawStr(4, y, s_status_lines[idx].c_str());
        }

        if (s_status_scroll > 0) {
            g_display.fillTriangle(124, 13, 120, 17, 128, 17, SSD1306_WHITE);
        }
        if (s_status_scroll + 6 < s_status_count) {
            g_display.fillTriangle(124, 60, 120, 56, 128, 56, SSD1306_WHITE);
        }

        displaySendBuffer();
        return;
    }

    if (s_page == PAGE_SETTINGS) {
        displayClearBuffer();
        displayDrawStr(0, 8, "=== SETTINGS ===");
        displayDrawLine(0, 10, 127, 10);

        int max_vis = 4;
        if (s_cursor < s_scroll) s_scroll = s_cursor;
        if (s_cursor >= s_scroll + max_vis) s_scroll = s_cursor - max_vis + 1;

        for (int i = 0; i < max_vis; i++) {
            int idx = s_scroll + i;
            if (idx >= SETTINGS_COUNT) break;
            int y = 22 + i * 10;
            if (idx == s_cursor) {
                displayDrawBox(0, y - 8, 128, 10);
                displaySetDrawColor(0);
            }
            displayDrawStr(4, y, SETTINGS_ITEMS[idx]);
            if (idx == s_cursor) displaySetDrawColor(1);
        }

        if (s_scroll > 0) {
            g_display.fillTriangle(124, 13, 120, 17, 128, 17, SSD1306_WHITE);
        }
        if (s_scroll + max_vis < SETTINGS_COUNT) {
            g_display.fillTriangle(124, 60, 120, 56, 128, 56, SSD1306_WHITE);
        }

        displaySendBuffer();
        return;
    }

    if (s_page == PAGE_EDIT_IP) {
        displayClearBuffer();
        displayDrawStr(0, 8, s_edit_title);
        displayDrawLine(0, 10, 127, 10);

        int x = 4;
        for (int i = 0; i < 4; i++) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", s_edit_ip[i]);
            int w = strlen(buf) * 6;
            if (i == s_edit_ip_idx) {
                displayDrawBox(x, 14, w, 10);
                displaySetDrawColor(0);
            }
            displayDrawStr(x, 22, buf);
            displaySetDrawColor(1);
            x += w + 2;
            if (i < 3) {
                displayDrawStr(x, 22, ".");
                x += 7;
            }
        }

        displayDrawStr(0, 40, "UP/DN: change");
        displayDrawStr(0, 50, "SEL: next");
        displayDrawStr(0, 60, "DBL SEL: back");
        displaySendBuffer();
        return;
    }

    if (s_page == PAGE_EDIT_MAC) {
        displayClearBuffer();
        displayDrawStr(0, 8, s_edit_title);
        displayDrawLine(0, 10, 127, 10);

        int x = 4;
        for (int i = 0; i < 6; i++) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02X", s_edit_mac[i]);
            if (i == s_edit_mac_idx) {
                displayDrawBox(x, 14, 12, 10);
                displaySetDrawColor(0);
            }
            displayDrawStr(x, 22, buf);
            displaySetDrawColor(1);
            x += 12;
            if (i < 5) {
                displayDrawStr(x, 22, ":");
                x += 6;
            }
        }

        displayDrawStr(0, 40, "UP/DN: change");
        displayDrawStr(0, 50, "SEL: next");
        displayDrawStr(0, 60, "DBL SEL: back");
        displaySendBuffer();
        return;
    }

    if (s_page == PAGE_EDIT_INT) {
        displayClearBuffer();
        displayDrawStr(0, 8, s_edit_title);
        displayDrawLine(0, 10, 127, 10);

        char buf[16];
        snprintf(buf, sizeof(buf), "%d ms", s_edit_int_val);
        drawCenteredStr(30, buf);

        displayDrawStr(0, 40, "UP/DN: change");
        displayDrawStr(0, 50, "SEL: save");
        displayDrawStr(0, 60, "DBL SEL: back");
        displaySendBuffer();
        return;
    }

    if (s_page == PAGE_EDIT_STRING) {
        displayClearBuffer();
        displayDrawStr(0, 8, s_edit_title);
        displayDrawLine(0, 10, 127, 10);

        int len = strlen(s_edit_str_buf);
        const char* disp = s_edit_str_buf;
        if (len > 20) disp = s_edit_str_buf + (len - 20);
        int cur_x = strlen(disp) * 6;
        if (cur_x > 120) cur_x = 120;
        displayDrawStr(0, 22, disp);
        displayDrawBox(cur_x, 15, 6, 10);

        char c = STRING_CHARSET[s_edit_str_charset_idx];
        char label[8];
        if (c == 1) strcpy(label, "[SPC]");
        else if (c == 2) strcpy(label, "[DEL]");
        else if (c == 3) strcpy(label, "[END]");
        else { label[0] = c; label[1] = 0; }
        drawCenteredStr(50, label);

        displayDrawStr(0, 40, "UP/DN: char");
        displayDrawStr(0, 50, "SEL: add");
        displayDrawStr(0, 60, "DBL SEL: back");
        displaySendBuffer();
        return;
    }

    if (s_page == PAGE_CONFIRM) {
        displayClearBuffer();
        displayDrawStr(0, 8, "FACTORY RESET");
        displayDrawLine(0, 10, 127, 10);
        displayDrawStr(0, 22, "Are you sure?");

        for (int i = 0; i < 2; i++) {
            int y = 40 + i * 12;
            if (i == s_confirm_cursor) {
                displayDrawBox(0, y - 8, 128, 10);
                displaySetDrawColor(0);
            }
            displayDrawStr(4, y, i == 0 ? "No" : "Yes");
            if (i == s_confirm_cursor) displaySetDrawColor(1);
        }
        displaySendBuffer();
        return;
    }
}
