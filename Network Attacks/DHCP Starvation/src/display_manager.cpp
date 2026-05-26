#include "display_manager.h"
#include <Wire.h>

Adafruit_SSD1306 g_display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool g_display_present = false;

void display_manager_init() {
    Wire.begin(OLED_SDA, OLED_SCL);
    delay(100);
    if (!g_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[OLED] Not found");
        g_display_present = false;
        return;
    }
    g_display_present = true;
    g_display.clearDisplay();
    g_display.setTextSize(1);
    g_display.setTextColor(SSD1306_WHITE);
    g_display.cp437(true);
    g_display.display();
    Serial.println("[OLED] OK");
}

void displayClearBuffer() {
    if (!g_display_present) return;
    g_display.clearDisplay();
}

void displaySendBuffer() {
    if (!g_display_present) return;
    g_display.display();
}

void displayDrawStr(int x, int y, const char* str) {
    if (!g_display_present) return;
    g_display.setCursor(x, y - 7);
    g_display.print(str);
}

void drawCenteredStr(int y, const char* str) {
    if (!g_display_present) return;
    int w = strlen(str) * MENU_FONT_W;
    if (w > SCREEN_WIDTH) w = SCREEN_WIDTH;
    g_display.setCursor((SCREEN_WIDTH - w) / 2, y - 7);
    g_display.print(str);
}

void displayDrawFrame(int x, int y, int w, int h) {
    if (!g_display_present) return;
    g_display.drawRect(x, y, w, h, SSD1306_WHITE);
}

void displayDrawBox(int x, int y, int w, int h) {
    if (!g_display_present) return;
    g_display.fillRect(x, y, w, h, SSD1306_WHITE);
}

void displayDrawLine(int x1, int y1, int x2, int y2) {
    if (!g_display_present) return;
    g_display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
}

void displaySetDrawColor(uint8_t color) {
    if (!g_display_present) return;
    g_display.setTextColor(color ? SSD1306_WHITE : SSD1306_BLACK);
}

int displayGetStrWidth(const char* str) {
    return strlen(str) * MENU_FONT_W;
}

void display_show_splash() {
    if (!g_display_present) return;
    displayClearBuffer();
    g_display.setTextSize(2);
    drawCenteredStr(16, "DHCP");
    drawCenteredStr(32, "STARVE");
    g_display.setTextSize(1);
    drawCenteredStr(56, "Pool Exhauster");
    displaySendBuffer();
    delay(2000);
}

void display_show_message(const char* line1, const char* line2, const char* line3) {
    if (!g_display_present) return;
    displayClearBuffer();
    if (line1) drawCenteredStr(line2 ? 16 : 28, line1);
    if (line2) drawCenteredStr(line3 ? 32 : 40, line2);
    if (line3) drawCenteredStr(48, line3);
    displaySendBuffer();
}

void display_show_wifi_connecting() {
    if (!g_display_present) return;
    displayClearBuffer();
    drawCenteredStr(28, "Connecting...");
    displaySendBuffer();
}
