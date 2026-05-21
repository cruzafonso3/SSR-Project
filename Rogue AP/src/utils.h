#ifndef ROGUE_AP_UTILS_H
#define ROGUE_AP_UTILS_H

#include <Arduino.h>

String macAddressToString(const uint8_t* mac, uint8_t len);
String getUptimeString();
String formatUptimeMinutes(unsigned long minutes);
bool isValidHtmlFilename(const String& name);

#endif
