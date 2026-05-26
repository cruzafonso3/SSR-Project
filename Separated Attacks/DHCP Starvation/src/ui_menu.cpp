#include "ui_menu.h"
#include "display_manager.h"
#include "buttons.h"
#include "config.h"
#include "wifi_manager.h"
#include "dhcp_starvation.h"
#include <WiFi.h>

// ─── Pages ──────────────────────────────────────────────
enum Page {
    PAGE_MAIN,
    PAGE_MESSAGE,
    PAGE_STATUS,
    PAGE_SETTINGS,
    PAGE_EDIT_STRING,
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
    "Start Starvation",
    "Stop Starvation",
    "Status",
    "Settings",
    "Save Config",
};
#define MAIN_COUNT 7

// ─── Settings menu ──────────────────────────────────────
static const char* SETTINGS_ITEMS[] = {
    "SSID",
    "Password",
    "Cooldown",
    "Factory Reset",
    "Back",
};
#define SETTINGS_COUNT 5

// ─── Status lines ───────────────────────────────────────
#define MAX_STATUS 28
static String s_status_lines[MAX_STATUS];
static int s_status_count = 0;
static int s_status_scroll = 0;

static void rebuild_status() {
    s_status_count = 0;

    auto add = [](const String& s) { if (s_status_count < MAX_STATUS) s_status_lines[s_status_count++] = s; };
    char buf[22];

    add("  -- NETWORK --");
    snprintf(buf, sizeof(buf), " SSID: %s", g_config.ssid);
    add(buf);
    add(" PASS: ********");
    if (wifi_manager_is_connected()) {
        add(" IP: " + wifi_manager_get_ip());
    } else {
        add(" WiFi: DISCONNECTED");
    }
    add(" MAC: " + dhcp_get_original_mac());

    add("");
    add("  -- STATE --");
    add(String(" RUNNING: ") + (dhcp_is_running() ? "YES" : "NO"));
    snprintf(buf, sizeof(buf), " RECONNECTS: %d", dhcp_get_reconnect_count());
    add(buf);
    snprintf(buf, sizeof(buf), " COOLDOWN: %d ms", dhcp_get_cooldown());
    add(buf);

    add("");
    add("  -- LAST LEASE --");
    add(" IP: " + (dhcp_get_current_ip().length() > 0 ? dhcp_get_current_ip() : "N/A"));
    add(" MAC: " + (dhcp_is_running() ? dhcp_get_current_mac() : "N/A"));

    if (s_status_scroll > s_status_count - 6) s_status_scroll = s_status_count - 6;
    if (s_status_scroll < 0) s_status_scroll = 0;
}

// ─── Editor state ───────────────────────────────────────
enum EditType { EDIT_INT, EDIT_STRING };
static EditType s_edit_type;

static const char* s_edit_title = "";
static char* s_edit_target_str = nullptr;

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

// ─── Helpers ────────────────────────────────────────────
static void save_edit() {
    switch (s_edit_type) {
        case EDIT_INT:
            if (s_edit_int_ptr) *s_edit_int_ptr = s_edit_int_val;
            break;
        case EDIT_STRING:
            if (s_edit_target_str) strncpy(s_edit_target_str, s_edit_str_buf, s_edit_str_max);
            break;
    }
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

static void action_connect() {
    if (strlen(g_config.ssid) == 0) { msg_temp("No SSID set!", nullptr, nullptr, 1500); return; }
    if (wifi_manager_is_connected()) { msg_temp("Already connected", nullptr, nullptr, 1500); return; }
    display_show_wifi_connecting();
    bool ok = wifi_manager_connect(g_config.ssid, g_config.password);
    if (ok) {
        msg_temp("WiFi connected", wifi_manager_get_ip().c_str(), nullptr, 2000);
    } else {
        msg_temp("WiFi failed!", nullptr, nullptr, 1500);
    }
}

static void action_disconnect() {
    if (dhcp_is_running()) dhcp_stop();
    wifi_manager_disconnect();
    msg_temp("WiFi disconnected", nullptr, nullptr, 1500);
}

static void action_start() {
    if (strlen(g_config.ssid) == 0) { msg_temp("No SSID set!", nullptr, nullptr, 1500); return; }
    if (!wifi_manager_is_connected()) {
        display_show_wifi_connecting();
        if (!wifi_manager_connect(g_config.ssid, g_config.password)) {
            msg_temp("WiFi failed!", nullptr, nullptr, 1500);
            return;
        }
    }
    dhcp_start();
    msg_temp("Starvation ON", nullptr, nullptr, 1500);
    Serial.println("[Menu] Starvation started");
}

static void action_stop() {
    dhcp_stop();
    msg_temp("Starvation OFF", nullptr, nullptr, 1500);
    Serial.println("[Menu] Starvation stopped");
}

static void action_show_status() {
    rebuild_status();
    s_status_scroll = 0;
    s_page = PAGE_STATUS;
}

// ─── Editors entry points ───────────────────────────────
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

// ─── Update ─────────────────────────────────────────────
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
                    case 2: action_start(); break;
                    case 3: action_stop(); break;
                    case 4: action_show_status(); break;
                    case 5: s_cursor = 0; s_scroll = 0; s_page = PAGE_SETTINGS; break;
                    case 6: config_save(); msg_temp("Config saved", nullptr, nullptr, 1500); break;
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
                    case 2: start_int_editor(&g_config.cooldown_ms, "COOLDOWN", 500, 10000, 100); break;
                    case 3: s_confirm_cursor = 0; s_page = PAGE_CONFIRM; break;
                    case 4: s_cursor = 0; s_scroll = 0; s_page = PAGE_MAIN; break;
                }
            }
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

        default: break;
    }
}

// ─── Render ─────────────────────────────────────────────
void ui_menu_render() {
    if (s_page == PAGE_MESSAGE) return;

    if (s_page == PAGE_MAIN) {
        displayClearBuffer();
        displayDrawStr(0, 8, "=== DHCP STARVE ===");
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

        if (s_scroll > 0) g_display.fillTriangle(124, 1, 120, 5, 128, 5, SSD1306_WHITE);
        if (s_scroll + max_vis < MAIN_COUNT) g_display.fillTriangle(124, 62, 120, 58, 128, 58, SSD1306_WHITE);

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

        if (s_status_scroll > 0) g_display.fillTriangle(124, 13, 120, 17, 128, 17, SSD1306_WHITE);
        if (s_status_scroll + 6 < s_status_count) g_display.fillTriangle(124, 60, 120, 56, 128, 56, SSD1306_WHITE);

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

        if (s_scroll > 0) g_display.fillTriangle(124, 13, 120, 17, 128, 17, SSD1306_WHITE);
        if (s_scroll + max_vis < SETTINGS_COUNT) g_display.fillTriangle(124, 60, 120, 56, 128, 56, SSD1306_WHITE);

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
