#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "config.h"

extern Adafruit_SSD1306 display;

void displayInit();
void displayClearBuffer();
void displaySendBuffer();
void displayDrawStr(int x, int y, const char* str);
void drawCenteredStr(int y, const char* str);
void displayDrawXBMP(int x, int y, int w, int h, const uint8_t* bitmap);
void displayDrawFrame(int x, int y, int w, int h);
void displayDrawBox(int x, int y, int w, int h);
void displayDrawLine(int x1, int y1, int x2, int y2);
int displayGetStrWidth(const char* str);

#endif