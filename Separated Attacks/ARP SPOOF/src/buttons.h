#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

#define BTN_UP_PIN 14
#define BTN_DOWN_PIN 27
#define BTN_SELECT_PIN 26

enum ButtonEvent {
    EVT_NONE,
    EVT_UP,
    EVT_DOWN,
    EVT_SELECT,
    EVT_BACK
};

void buttons_init();
ButtonEvent buttons_get_event();

#endif
