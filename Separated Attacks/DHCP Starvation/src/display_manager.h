#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#define OLED_ADDR 0x3C
#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define MENU_FONT_W 6

extern Adafruit_SSD1306 g_display;
extern bool g_display_present;

void display_manager_init();
void displayClearBuffer();
void displaySendBuffer();
void displayDrawStr(int x, int y, const char* str);
void drawCenteredStr(int y, const char* str);
void displayDrawFrame(int x, int y, int w, int h);
void displayDrawBox(int x, int y, int w, int h);
void displayDrawLine(int x1, int y1, int x2, int y2);
void displaySetDrawColor(uint8_t color);
int displayGetStrWidth(const char* str);

void display_show_splash();
void display_show_message(const char* line1, const char* line2, const char* line3);
void display_show_wifi_connecting();

#endif
