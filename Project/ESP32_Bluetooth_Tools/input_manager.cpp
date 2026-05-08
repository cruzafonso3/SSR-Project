#include "input_manager.h"

struct Button {
    int pin;
    bool lastRaw;
    bool stable;
    unsigned long lastTime;
};

static Button buttons[3] = {
    {BTN_UP_PIN, true, true, 0},
    {BTN_DOWN_PIN, true, true, 0},
    {BTN_SELECT_PIN, true, true, 0}
};

void inputInit() {
    for (int i = 0; i < 3; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
        buttons[i].lastRaw = true;
        buttons[i].stable = true;
        buttons[i].lastTime = millis();
    }
}

ButtonEvent inputGetEvent() {
    unsigned long now = millis();
    for (int i = 0; i < 3; i++) {
        bool reading = digitalRead(buttons[i].pin);
        if (reading != buttons[i].lastRaw) {
            buttons[i].lastTime = now;
            buttons[i].lastRaw = reading;
        }
        if ((now - buttons[i].lastTime) > 50) {
            if (reading != buttons[i].stable) {
                buttons[i].stable = reading;
                if (reading == LOW) {
                    if (i == 0) return EVT_UP;
                    if (i == 1) return EVT_DOWN;
                    if (i == 2) return EVT_SELECT;
                }
            }
        }
    }
    return EVT_NONE;
}
