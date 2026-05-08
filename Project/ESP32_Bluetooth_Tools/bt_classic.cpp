#include "bt_classic.h"
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

#define MAX_BT 10
struct BTInfo {
    char name[24];
};
static BTInfo btDevices[MAX_BT];
static int btCount = 0;
static bool btScanning = false;

void btClassicInit() {
    SerialBT.begin("ESP32_BT", true);
}

void btClassicScanStart() {
    btCount = 0;
    btScanning = true;
}

void btClassicScanStop() {
    btScanning = false;
}

bool btClassicScanIsRunning() { return btScanning; }
int btClassicScanGetResultCount() { return btCount; }
const char* btClassicScanGetResultName(int idx) { return (idx < btCount) ? btDevices[idx].name : ""; }
