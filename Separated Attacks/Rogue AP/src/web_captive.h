#ifndef ROGUE_AP_WEB_CAPTIVE_H
#define ROGUE_AP_WEB_CAPTIVE_H

#include <Arduino.h>
#include <WebServer.h>
#include "types.h"

void serveCaptivePortal(WebServer& server, bool isRogueApRunning, bool hasPassword, const String& ssid);
void processCredentialCapture(WebServer& server, const String& currentRogueSsid);
String buildCaptivePortalPage(const AttackProfile& config, bool hasPassword, const String& ssid);
void sendSuccessRedirect(WebServer& server);

#endif
