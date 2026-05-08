#include "ble_utils.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>

static BLEScan* pScan = nullptr;
static BLEAdvertising* pAdvertising = nullptr;
static bool scanRunning = false;
static bool spoofRunning = false;

#define MAX_BLE 15
struct BLEInfo {
    char name[24];
    char addr[14];
    int rssi;
};
static BLEInfo bleDevices[MAX_BLE];
static int bleCount = 0;

class MyAdvCallback: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (bleCount >= MAX_BLE) return;
        strlcpy(bleDevices[bleCount].name, advertisedDevice.getName().c_str(), 24);
        strlcpy(bleDevices[bleCount].addr, advertisedDevice.getAddress().toString().c_str(), 14);
        bleDevices[bleCount].rssi = advertisedDevice.getRSSI();
        bleCount++;
    }
};

void bleInit() {
    BLEDevice::init("ESP32");
    pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyAdvCallback());
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);
}

void bleScanStart() {
    bleCount = 0;
    scanRunning = true;
    pScan->start(10, nullptr, false);
}

void bleScanStop() {
    if (scanRunning) { pScan->stop(); scanRunning = false; }
}

bool bleScanIsRunning() { return scanRunning; }
int bleScanGetResultCount() { return bleCount; }
const char* bleScanGetResultName(int idx) { return (idx < bleCount) ? bleDevices[idx].name : ""; }
const char* bleScanGetResultMac(int idx) { return (idx < bleCount) ? bleDevices[idx].addr : ""; }
int bleScanGetResultRssi(int idx) { return (idx < bleCount) ? bleDevices[idx].rssi : 0; }

void bleSpoofStart(const char* name) {
    if (spoofRunning) bleSpoofStop();
    BLEDevice::deinit();
    BLEDevice::init(name);
    pAdvertising = BLEDevice::getAdvertising();
    if (strstr(name, "AirPods")) pAdvertising->setAppearance(0x03E0);
    else if (strstr(name, "Tesla")) pAdvertising->setAppearance(0x0080);
    else if (strstr(name, "Samsung")) pAdvertising->setAppearance(0x03EC);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();
    spoofRunning = true;
}

void bleSpoofStop() {
    if (spoofRunning && pAdvertising) pAdvertising->stop();
    spoofRunning = false;
}

bool bleSpoofIsRunning() { return spoofRunning; }
