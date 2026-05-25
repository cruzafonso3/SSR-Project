#ifndef ROGUE_AP_TYPES_H
#define ROGUE_AP_TYPES_H

#include <Arduino.h>

struct NetworkRecord {
    String ssid;
    uint8_t channel;
    uint8_t bssid[6];
    String bssidString;
    String password;
};

struct AttackProfile {
    String pageTitle;
    String pageSubtitle;
    String pageBody;
    String customHtmlFile;
    bool useCustomHtml;
    bool autoStopEnabled;
    uint16_t autoStopMinutes;
};

#endif
