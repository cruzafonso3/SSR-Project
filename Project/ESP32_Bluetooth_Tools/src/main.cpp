/*
 * ESP32 Bluetooth Tools - BLE Scan/Spoof
 * Educational Proof of Concept
 * 
 * PINOUT: OLED SDA->21, SCL->22, BTN_UP->14, BTN_DOWN->27, BTN_SEL->26
 * Libraries: Adafruit_SSD1306, Adafruit_GFX
 * NO WiFi - Pure Bluetooth tool
 */

#include <Arduino.h>
#include "config.h"
#include "display_manager.h"
#include "input_manager.h"
#include "skull_logo.h"
#include "ble_utils.h"

// ===== Menu State =====
static int cursor = 0;
static int menuState = 0; // 0=splash, 1=main, 2=ble_spoof, 3=ble_results, 4=status
static int scrollOffset = 0;
static unsigned long lastRender = 0;
static unsigned long splashStart = 0;

// Custom spoof name
static char customName[24] = "ESP32_Custom";
static int editPos = 0;
static const char editChars[] = "abcdefghijklmnopqrstuvwxyz0123456789_- ";

void renderMain();
void renderBleSpoof();
void renderBleResults();
void renderStatus();

void setup() {
    Serial.begin(115200);
    delay(500);
    displayInit();
    inputInit();
    bleInit();
    
    splashStart = millis();
    Serial.println("[+] Bluetooth Tools Ready");
}

void loop() {
    unsigned long now = millis();
    ButtonEvent evt = inputGetEvent();
    
    // Splash
    if (menuState == 0 && now - splashStart < 2000) {
        if (evt == EVT_SELECT) splashStart = 0;
        if (now - lastRender > 50) {
            lastRender = now;
            displayClearBuffer();
            displayDrawXBMP(32, 0, SKULL_WIDTH, SKULL_HEIGHT, skull_bits);
            drawCenteredStr(56, "ESP32 BLUETOOTH");
            drawCenteredStr(64, "RECON TOOLS");
            displaySendBuffer();
        }
        return;
    }
    if (menuState == 0) menuState = 1;
    
    // Input handling
    if (menuState == 1) {
        if (evt == EVT_UP && cursor > 0) cursor--;
        if (evt == EVT_DOWN && cursor < 3) cursor++;  // FIX: was 4, now 3 items
        if (evt == EVT_SELECT) {
            if (cursor == 0) { bleScanStart(); menuState = 3; scrollOffset = 0; }
            if (cursor == 1) { menuState = 2; cursor = 0; }
            if (cursor == 2) { menuState = 4; }  // Status
            if (cursor == 3) { if (bleSpoofIsRunning()) bleSpoofStop(); }  // Stop Spoof
        }
    } else if (menuState == 2) {
        // BLE Spoofer submenu
        if (evt == EVT_UP && cursor > 0) cursor--;
        if (evt == EVT_DOWN && cursor < 3) cursor++;
        if (evt == EVT_SELECT) {
            if (cursor == 0) bleSpoofStart("AirPods Pro");
            if (cursor == 1) bleSpoofStart("Tesla Model 3");
            if (cursor == 2) bleSpoofStart("Samsung TV");
            if (cursor == 3) bleSpoofStart(customName);
            menuState = 1; cursor = 0;
        }
        if (evt == EVT_DOWN && cursor == 3) { /* edit custom name - simplified */ }
    } else if (menuState == 3) {
        int count = bleScanGetResultCount();
        if (!bleScanIsRunning() && count == 0 && evt == EVT_SELECT) menuState = 1;
        if (evt == EVT_UP && scrollOffset > 0) scrollOffset--;
        if (evt == EVT_DOWN && scrollOffset < count - 1) scrollOffset++;
        if (evt == EVT_SELECT) menuState = 1;
    } else if (menuState == 4) {
        if (evt == EVT_SELECT || evt == EVT_UP || evt == EVT_DOWN) menuState = 1;
    }
    
    if (now - lastRender > 50) {
        lastRender = now;
        switch (menuState) {
            case 1: renderMain(); break;
            case 2: renderBleSpoof(); break;
            case 3: renderBleResults(); break;
            case 4: renderStatus(); break;
        }
    }
    delay(5);
}

void renderMain() {
    displayClearBuffer();
    displayDrawStr(0, 8, "BLUETOOTH TOOLS");
    displayDrawLine(0, 10, 127, 10);
    const char* items[] = {"BLE Scanner", "BLE Spoofer", "Status", "Stop Spoof"};  // FIX: removed BT Classic
    for (int i = 0; i < 4; i++) {  // FIX: was 5
        int y = 22 + i * 10;
        if (i == cursor) displayDrawStr(0, y, ">");
        displayDrawStr(8, y, items[i]);
    }
    displaySendBuffer();
}

void renderBleSpoof() {
    displayClearBuffer();
    displayDrawStr(0, 8, "BLE SPOOFER");
    displayDrawLine(0, 10, 127, 10);
    const char* items[] = {"AirPods Pro", "Tesla M3", "Samsung TV", "Custom"};
    for (int i = 0; i < 4; i++) {
        int y = 22 + i * 11;
        if (i == cursor) displayDrawStr(0, y, ">");
        displayDrawStr(8, y, items[i]);
    }
    displaySendBuffer();
}

void renderBleResults() {
    displayClearBuffer();
    displayDrawStr(0, 8, "BLE DEVICES");
    displayDrawLine(0, 10, 127, 10);
    int count = bleScanGetResultCount();
    if (bleScanIsRunning()) {
        displayDrawStr(0, 30, "Scanning...");
    } else if (count == 0) {
        displayDrawStr(0, 30, "No devices.");
    } else {
        for (int i = scrollOffset; i < min(count, scrollOffset + 4); i++) {
            int y = 22 + (i - scrollOffset) * 11;
            if (i == scrollOffset) displayDrawStr(0, y, ">");
            displayDrawStr(8, y, bleScanGetResultName(i));
            char rssi[8]; sprintf(rssi, "%d", bleScanGetResultRssi(i));
            displayDrawStr(100, y, rssi);
        }
    }
    displaySendBuffer();
}

void renderStatus() {
    displayClearBuffer();
    displayDrawStr(0, 8, "STATUS");
    displayDrawLine(0, 10, 127, 10);
    char buf[32];
    sprintf(buf, "RAM:%dKB", ESP.getFreeHeap()/1024); displayDrawStr(0, 24, buf);
    displayDrawStr(0, 36, bleSpoofIsRunning() ? "Spoofing: ON" : "Spoofing: OFF");
    sprintf(buf, "BLE:%d", bleScanGetResultCount());
    displayDrawStr(0, 48, buf);
    unsigned long sec = millis()/1000;
    sprintf(buf, "UP:%02lu:%02lu", sec/60, sec%60); displayDrawStr(0, 60, buf);
    displaySendBuffer();
}
