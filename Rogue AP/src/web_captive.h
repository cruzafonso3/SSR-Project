#ifndef ROGUE_AP_WEB_CAPTIVE_H
#define ROGUE_AP_WEB_CAPTIVE_H

#include <Arduino.h>
#include <WebServer.h>
#include "types.h"

void serveCaptivePortal(WebServer& server, bool isRogueApRunning);
void processCredentialCapture(WebServer& server, const String& currentRogueSsid);
String buildCaptivePortalPage(const AttackProfile& config);
void sendSuccessRedirect(WebServer& server);

#endif
