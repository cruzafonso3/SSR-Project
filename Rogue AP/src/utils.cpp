#include "utils.h"
#include "config.h"

String macAddressToString(const uint8_t* mac, uint8_t len) {
    String result;
    for (uint8_t i = 0; i < len; i++) {
        if (mac[i] < 0x10) result += "0";
        result += String(mac[i], HEX);
        if (i < len - 1) result += ":";
    }
    return result;
}

String getUptimeString() {
    unsigned long totalSeconds = millis() / 1000;
    char buffer[20];
    sprintf(buffer, "%02lu:%02lu:%02lu",
            (totalSeconds / 3600) % 24,
            (totalSeconds / 60) % 60,
            totalSeconds % 60);
    return String(buffer);
}

String formatUptimeMinutes(unsigned long minutes) {
    if (minutes < 1) return "Just now";
    if (minutes < 60) return String(minutes) + " min ago";
    unsigned long hours = minutes / 60;
    unsigned long remainingMin = minutes % 60;
    String result = String(hours) + "h";
    if (remainingMin > 0) result += " " + String(remainingMin) + "m";
    result += " ago";
    return result;
}

bool isValidHtmlFilename(const String& name) {
    if (name.length() == 0 || name.length() > 64) return false;
    for (uint8_t i = 0; i < name.length(); i++) {
        char c = name.charAt(i);
        if (!isalnum(c) && c != '.' && c != '_' && c != '-') return false;
    }
    return true;
}
