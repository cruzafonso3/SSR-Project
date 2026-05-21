#ifndef ROGUE_AP_WEB_ADMIN_H
#define ROGUE_AP_WEB_ADMIN_H

#include <Arduino.h>
#include <WebServer.h>
#include "types.h"

void serveAdminPanel(WebServer& server, const std::vector<NetworkRecord>& networks,
                     const NetworkRecord& activeTarget, bool isRogueApRunning,
                     const String& currentRogueSsid, unsigned long lastScanTime);
void handleControlAction(WebServer& server);
void handleScanRequest(WebServer& server);
void handleNetworkSelect(WebServer& server, std::vector<NetworkRecord>& networks,
                         NetworkRecord& activeTarget);
void handleClearCredentials(WebServer& server);

#endif
