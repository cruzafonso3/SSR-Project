#include "settings_menu.h"
#include "display_manager.h"
#include "settings_manager.h"
#include "input_manager.h"

enum SettingsState {
    ST_MENU,
    ST_EDIT_IP,
    ST_EDIT_MAC,
    ST_EDIT_INT,
    ST_EDIT_STRING,
    ST_CONFIRM_RESET
};

static SettingsState state = ST_MENU;
static bool active = false;

static int menuCursor = 0;
static int menuScroll = 0;

static const char* menuLabels[] = {
    "Victim IP",
    "Victim MAC",
    "Gateway IP",
    "Gateway MAC",
    "Spoofed IP",
    "ARP Interval",
    "Target Domain",
    "WiFi SSID",
    "WiFi Password",
    "Factory Reset",
    "Back"
};
#define MENU_COUNT 11

// Editor state
static IPAddress* editIpTarget = nullptr;
static uint8_t* editMacTarget = nullptr;
static int* editIntTarget = nullptr;
static char* editStringTarget = nullptr;
static int editStringMaxLen = 0;
static const char* editPrefKey = nullptr;
static const char* editTitle = "";

static IPAddress editIpValue;
static int editOctetIdx = 0;
static uint8_t editMacValue[6];
static int editByteIdx = 0;
static int editIntValue = 0;
static int editIntStep = 1;
static char editStringBuf[65];
static int editCharIdx = 0;

static int confirmCursor = 0;

// String charset
static const char STRING_CHARSET[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    " .-_!@#$%^&*()[]{}|;:'\",<>?/`~\\=+"
    "\x01\x02\x03"; // SPC, DEL, END
#define CHARSET_LEN (sizeof(STRING_CHARSET) - 1)

static void clampMenuScroll() {
    if (menuCursor < menuScroll) menuScroll = menuCursor;
    if (menuCursor >= menuScroll + 4) menuScroll = menuCursor - 3;
}

void settingsMenuOpen() {
    state = ST_MENU;
    active = true;
    menuCursor = 0;
    menuScroll = 0;
}

bool settingsMenuIsActive() {
    return active;
}

static void startIpEditor(IPAddress* target, const char* key, const char* title) {
    state = ST_EDIT_IP;
    editIpTarget = target;
    editPrefKey = key;
    editTitle = title;
    editIpValue = *target;
    editOctetIdx = 0;
}

static void startMacEditor(uint8_t* target, const char* key, const char* title) {
    state = ST_EDIT_MAC;
    editMacTarget = target;
    editPrefKey = key;
    editTitle = title;
    memcpy(editMacValue, target, 6);
    editByteIdx = 0;
}

static void startIntEditor(int* target, const char* key, const char* title, int step) {
    state = ST_EDIT_INT;
    editIntTarget = target;
    editPrefKey = key;
    editTitle = title;
    editIntValue = *target;
    editIntStep = step;
}

static void startStringEditor(char* target, const char* key, const char* title, int maxLen) {
    state = ST_EDIT_STRING;
    editStringTarget = target;
    editPrefKey = key;
    editTitle = title;
    editStringMaxLen = maxLen;
    strlcpy(editStringBuf, target, sizeof(editStringBuf));
    editCharIdx = 0;
}

static void saveCurrentEdit() {
    switch (state) {
        case ST_EDIT_IP:
            if (editIpTarget) *editIpTarget = editIpValue;
            break;
        case ST_EDIT_MAC:
            if (editMacTarget) memcpy(editMacTarget, editMacValue, 6);
            break;
        case ST_EDIT_INT:
            if (editIntTarget) *editIntTarget = editIntValue;
            break;
        case ST_EDIT_STRING:
            if (editStringTarget) strlcpy(editStringTarget, editStringBuf, editStringMaxLen);
            break;
        default:
            break;
    }
    settingsSave();
    state = ST_MENU;
}

static void cancelCurrentEdit() {
    state = ST_MENU;
}

void settingsMenuUpdate(ButtonEvent evt) {
    if (evt == EVT_NONE) return;
    
    switch (state) {
        case ST_MENU:
            if (evt == EVT_UP && menuCursor > 0) menuCursor--;
            if (evt == EVT_DOWN && menuCursor < MENU_COUNT - 1) menuCursor++;
            clampMenuScroll();
            if (evt == EVT_SELECT) {
                switch (menuCursor) {
                    case 0: startIpEditor(&g_victim_ip, "victim_ip", "EDIT VICTIM IP"); break;
                    case 1: startMacEditor(g_victim_mac, "victim_mac", "EDIT VICTIM MAC"); break;
                    case 2: startIpEditor(&g_gateway_ip, "gw_ip", "EDIT GATEWAY IP"); break;
                    case 3: startMacEditor(g_gateway_mac, "gw_mac", "EDIT GATEWAY MAC"); break;
                    case 4: startIpEditor(&g_spoofed_ip, "spoof_ip", "EDIT SPOOFED IP"); break;
                    case 5: startIntEditor(&g_arp_interval_ms, "arp_int", "EDIT ARP INTERVAL", 100); break;
                    case 6: startStringEditor(g_target_domain, "domain", "EDIT DOMAIN", sizeof(g_target_domain)); break;
                    case 7: startStringEditor(g_wifi_ssid, "wifi_ssid", "EDIT WIFI SSID", sizeof(g_wifi_ssid)); break;
                    case 8: startStringEditor(g_wifi_pass, "wifi_pass", "EDIT WIFI PASS", sizeof(g_wifi_pass)); break;
                    case 9: state = ST_CONFIRM_RESET; confirmCursor = 0; break;
                    case 10: active = false; break;
                }
            }
            if (evt == EVT_BACK) {
                active = false;
            }
            break;
            
        case ST_EDIT_IP:
            if (evt == EVT_UP) {
                int v = (int)editIpValue[editOctetIdx] + 1;
                if (v > 255) v = 0;
                editIpValue[editOctetIdx] = (uint8_t)v;
            }
            if (evt == EVT_DOWN) {
                int v = (int)editIpValue[editOctetIdx] - 1;
                if (v < 0) v = 255;
                editIpValue[editOctetIdx] = (uint8_t)v;
            }
            if (evt == EVT_SELECT) {
                editOctetIdx++;
                if (editOctetIdx >= 4) {
                    saveCurrentEdit();
                }
            }
            if (evt == EVT_BACK) {
                cancelCurrentEdit();
            }
            break;
            
        case ST_EDIT_MAC:
            if (evt == EVT_UP) {
                int v = (int)editMacValue[editByteIdx] + 1;
                if (v > 255) v = 0;
                editMacValue[editByteIdx] = (uint8_t)v;
            }
            if (evt == EVT_DOWN) {
                int v = (int)editMacValue[editByteIdx] - 1;
                if (v < 0) v = 255;
                editMacValue[editByteIdx] = (uint8_t)v;
            }
            if (evt == EVT_SELECT) {
                editByteIdx++;
                if (editByteIdx >= 6) {
                    saveCurrentEdit();
                }
            }
            if (evt == EVT_BACK) {
                cancelCurrentEdit();
            }
            break;
            
        case ST_EDIT_INT:
            if (evt == EVT_UP) {
                editIntValue += editIntStep;
            }
            if (evt == EVT_DOWN) {
                editIntValue -= editIntStep;
                if (editIntValue < 0) editIntValue = 0;
            }
            if (evt == EVT_SELECT) {
                saveCurrentEdit();
            }
            if (evt == EVT_BACK) {
                cancelCurrentEdit();
            }
            break;
            
        case ST_EDIT_STRING:
            if (evt == EVT_UP) {
                editCharIdx++;
                if (editCharIdx >= CHARSET_LEN) editCharIdx = 0;
            }
            if (evt == EVT_DOWN) {
                editCharIdx--;
                if (editCharIdx < 0) editCharIdx = CHARSET_LEN - 1;
            }
            if (evt == EVT_SELECT) {
                char c = STRING_CHARSET[editCharIdx];
                int len = strlen(editStringBuf);
                if (c == 1) { // SPC
                    if (len < editStringMaxLen - 1) {
                        editStringBuf[len] = ' ';
                        editStringBuf[len+1] = '\0';
                    }
                } else if (c == 2) { // DEL
                    if (len > 0) {
                        editStringBuf[len-1] = '\0';
                    }
                } else if (c == 3) { // END
                    saveCurrentEdit();
                } else {
                    if (len < editStringMaxLen - 1) {
                        editStringBuf[len] = c;
                        editStringBuf[len+1] = '\0';
                    }
                }
            }
            if (evt == EVT_BACK) {
                cancelCurrentEdit();
            }
            break;
            
        case ST_CONFIRM_RESET:
            if (evt == EVT_UP && confirmCursor > 0) confirmCursor--;
            if (evt == EVT_DOWN && confirmCursor < 1) confirmCursor++;
            if (evt == EVT_SELECT) {
                if (confirmCursor == 1) {
                    settingsReset();
                }
                state = ST_MENU;
            }
            if (evt == EVT_BACK) {
                state = ST_MENU;
            }
            break;
    }
}

// --- Rendering ---

static void renderMenu() {
    displayClearBuffer();
    displayDrawStr(0, 8, "SETTINGS");
    displayDrawLine(0, 10, 127, 10);
    
    for (int i = 0; i < 4; i++) {
        int idx = menuScroll + i;
        if (idx >= MENU_COUNT) break;
        int y = 22 + i * 10;
        if (idx == menuCursor) {
            displayDrawBox(0, y - 8, 128, 10);
            displaySetDrawColor(0);
        }
        displayDrawStr(4, y, menuLabels[idx]);
        if (idx == menuCursor) displaySetDrawColor(1);
    }
    displaySendBuffer();
}

static void renderIpEditor() {
    displayClearBuffer();
    displayDrawStr(0, 8, editTitle);
    displayDrawLine(0, 10, 127, 10);
    
    int x = 4;
    for (int i = 0; i < 4; i++) {
        char buf[4];
        sprintf(buf, "%d", editIpValue[i]);
        int w = strlen(buf) * 6;
        if (i == editOctetIdx) {
            displayDrawBox(x, 14, w, 10);
            displaySetDrawColor(0);
        }
        displayDrawStr(x, 22, buf);
        displaySetDrawColor(1);
        x += w;
        if (i < 3) {
            displayDrawStr(x, 22, ".");
            x += 6;
        }
    }
    
    displayDrawStr(0, 40, "UP/DN: change");
    displayDrawStr(0, 50, "SEL: next");
    displayDrawStr(0, 60, "LONG: cancel");
    displaySendBuffer();
}

static void renderMacEditor() {
    displayClearBuffer();
    displayDrawStr(0, 8, editTitle);
    displayDrawLine(0, 10, 127, 10);
    
    int x = 4;
    for (int i = 0; i < 6; i++) {
        char buf[3];
        sprintf(buf, "%02X", editMacValue[i]);
        if (i == editByteIdx) {
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
    displayDrawStr(0, 60, "LONG: cancel");
    displaySendBuffer();
}

static void renderIntEditor() {
    displayClearBuffer();
    displayDrawStr(0, 8, editTitle);
    displayDrawLine(0, 10, 127, 10);
    char buf[16];
    sprintf(buf, "%d", editIntValue);
    drawCenteredStr(30, buf);
    displayDrawStr(0, 40, "UP/DN: change");
    displayDrawStr(0, 50, "SEL: save");
    displayDrawStr(0, 60, "LONG: cancel");
    displaySendBuffer();
}

static void renderStringEditor() {
    displayClearBuffer();
    displayDrawStr(0, 8, editTitle);
    displayDrawLine(0, 10, 127, 10);
    
    int len = strlen(editStringBuf);
    const char* displayStr = editStringBuf;
    if (len > 21) displayStr = editStringBuf + (len - 21);
    displayDrawStr(0, 22, displayStr);
    int cursorX = strlen(displayStr) * 6;
    displayDrawBox(cursorX, 15, 6, 10);
    
    char c = STRING_CHARSET[editCharIdx];
    char buf[8];
    if (c == 1) strcpy(buf, "[SPC]");
    else if (c == 2) strcpy(buf, "[DEL]");
    else if (c == 3) strcpy(buf, "[END]");
    else { buf[0] = c; buf[1] = '\0'; }
    
    drawCenteredStr(50, buf);
    
    displayDrawStr(0, 40, "UP/DN: char");
    displayDrawStr(0, 50, "SEL: add");
    displayDrawStr(0, 60, "LONG: cancel");
    displaySendBuffer();
}

static void renderConfirmReset() {
    displayClearBuffer();
    displayDrawStr(0, 8, "FACTORY RESET");
    displayDrawLine(0, 10, 127, 10);
    displayDrawStr(0, 22, "Are you sure?");
    
    const char* opts[] = {"No", "Yes"};
    for (int i = 0; i < 2; i++) {
        int y = 40 + i * 12;
        if (i == confirmCursor) {
            displayDrawBox(0, y - 8, 128, 10);
            displaySetDrawColor(0);
        }
        displayDrawStr(4, y, opts[i]);
        if (i == confirmCursor) displaySetDrawColor(1);
    }
    displaySendBuffer();
}

void settingsMenuRender() {
    switch (state) {
        case ST_MENU: renderMenu(); break;
        case ST_EDIT_IP: renderIpEditor(); break;
        case ST_EDIT_MAC: renderMacEditor(); break;
        case ST_EDIT_INT: renderIntEditor(); break;
        case ST_EDIT_STRING: renderStringEditor(); break;
        case ST_CONFIRM_RESET: renderConfirmReset(); break;
    }
}
