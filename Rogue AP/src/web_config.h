#ifndef ROGUE_AP_WEB_CONFIG_H
#define ROGUE_AP_WEB_CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include "types.h"

void serveConfigPage(WebServer& server, const AttackProfile& config);
void applyConfigChanges(WebServer& server, AttackProfile& config);
void serveUploadPage(WebServer& server);
void applyHtmlUpload(WebServer& server);

#endif
