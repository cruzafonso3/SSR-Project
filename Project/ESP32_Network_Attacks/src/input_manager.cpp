#include "input_manager.h"

static unsigned long lastPressTime = 0;
static bool waitingRelease[3] = {false, false, false};
static const int buttonPins[3] = {BTN_UP_PIN, BTN_DOWN_PIN, BTN_SELECT_PIN};

void inputInit() {
    for (int i = 0; i < 3; i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        waitingRelease[i] = false;
    }
    lastPressTime = 0;
}

ButtonEvent inputGetEvent() {
    unsigned long now = millis();
    
    // 200ms debounce window after any press
    if (now - lastPressTime < 200) {
        // Still in debounce window, just update release states without generating events
        for (int i = 0; i < 3; i++) {
            if (digitalRead(buttonPins[i]) == HIGH) {
                waitingRelease[i] = false;
            }
        }
        return EVT_NONE;
    }
    
    for (int i = 0; i < 3; i++) {
        bool pressed = (digitalRead(buttonPins[i]) == LOW);
        
        if (pressed && !waitingRelease[i]) {
            waitingRelease[i] = true;
            lastPressTime = now;
            if (i == 0) return EVT_UP;
            if (i == 1) return EVT_DOWN;
            if (i == 2) return EVT_SELECT;
        }
        
        if (!pressed) {
            waitingRelease[i] = false;
        }
    }
    
    return EVT_NONE;
}
