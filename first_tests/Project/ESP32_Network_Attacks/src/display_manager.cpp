#include "display_manager.h"
#include <Wire.h>

#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void displayInit() {
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

void displayClearBuffer() {
    display.clearDisplay();
}

void displaySendBuffer() {
    display.display();
}

void displayDrawStr(int x, int y, const char* str) {
    display.setCursor(x, y - 7);
    display.print(str);
}

void drawCenteredStr(int y, const char* str) {
    int w = strlen(str) * MENU_FONT_W;
    display.setCursor((SCREEN_WIDTH - w) / 2, y - 7);
    display.print(str);
}

void displayDrawXBMP(int x, int y, int w, int h, const uint8_t* bitmap) {
    display.drawXBitmap(x, y, bitmap, w, h, SSD1306_WHITE);
}

void displayDrawFrame(int x, int y, int w, int h) {
    display.drawRect(x, y, w, h, SSD1306_WHITE);
}

void displayDrawBox(int x, int y, int w, int h) {
    display.fillRect(x, y, w, h, SSD1306_WHITE);
}

void displayDrawLine(int x1, int y1, int x2, int y2) {
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
}

void displaySetDrawColor(uint8_t color) {
    display.setTextColor(color ? SSD1306_WHITE : SSD1306_BLACK);
}

int displayGetStrWidth(const char* str) {
    return strlen(str) * MENU_FONT_W;
}
