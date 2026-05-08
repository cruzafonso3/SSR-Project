#include "input_manager.h"

#define DEBOUNCE_MS 200
#define LONG_PRESS_MS 800

static unsigned long lastEventTime = 0;
static bool waitingRelease[3] = {false, false, false};
static bool longPressSent[3] = {false, false, false};
static unsigned long pressTime[3] = {0, 0, 0};
static const int buttonPins[3] = {BTN_UP_PIN, BTN_DOWN_PIN, BTN_SELECT_PIN};

void inputInit() {
    for (int i = 0; i < 3; i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        waitingRelease[i] = false;
        longPressSent[i] = false;
        pressTime[i] = 0;
    }
    lastEventTime = 0;
}

ButtonEvent inputGetEvent() {
    unsigned long now = millis();
    
    // Global cooldown: update release states but don't emit events
    if (now - lastEventTime < DEBOUNCE_MS) {
        for (int i = 0; i < 3; i++) {
            if (digitalRead(buttonPins[i]) == HIGH && waitingRelease[i]) {
                waitingRelease[i] = false;
                longPressSent[i] = false;
            }
        }
        return EVT_NONE;
    }
    
    for (int i = 0; i < 3; i++) {
        bool pressed = (digitalRead(buttonPins[i]) == LOW);
        
        if (pressed && !waitingRelease[i]) {
            waitingRelease[i] = true;
            pressTime[i] = now;
            longPressSent[i] = false;
        }
        
        if (pressed && waitingRelease[i] && !longPressSent[i]) {
            if (i == 2 && (now - pressTime[i] >= LONG_PRESS_MS)) {
                longPressSent[i] = true;
                lastEventTime = now;
                return EVT_BACK;
            }
        }
        
        if (!pressed && waitingRelease[i]) {
            waitingRelease[i] = false;
            if (!longPressSent[i]) {
                lastEventTime = now;
                if (i == 0) return EVT_UP;
                if (i == 1) return EVT_DOWN;
                if (i == 2) return EVT_SELECT;
            }
            longPressSent[i] = false;
        }
    }
    
    return EVT_NONE;
}
