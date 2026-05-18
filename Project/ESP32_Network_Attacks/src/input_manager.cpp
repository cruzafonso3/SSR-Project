#include "input_manager.h"

#define REPEAT_DELAY_MS     1000   // hold > 1 second before repeating
#define REPEAT_INTERVAL_MS  80     // then fire every 80 ms (rapid)
#define DEBOUNCE_MS         150    // block bounce after press / release
#define DOUBLE_CLICK_MS     250    // two SELECT presses within this = BACK

static bool btnState[3]   = {false, false, false};
static unsigned long pressStart[3]   = {0, 0, 0};
static unsigned long lastRepeat[3]   = {0, 0, 0};
static unsigned long nextEventTime[3] = {0, 0, 0};
static const int buttonPins[3] = {BTN_UP_PIN, BTN_DOWN_PIN, BTN_SELECT_PIN};

// Double-click state for SELECT
static bool selectPending = false;
static unsigned long lastSelectPress = 0;

void inputInit() {
    for (int i = 0; i < 3; i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        btnState[i] = false;
        pressStart[i] = 0;
        lastRepeat[i] = 0;
        nextEventTime[i] = 0;
    }
    selectPending = false;
    lastSelectPress = 0;
}

ButtonEvent inputGetEvent() {
    unsigned long now = millis();
    
    // 1. Timeout a pending single SELECT into a real SELECT event
    if (selectPending && (now - lastSelectPress >= DOUBLE_CLICK_MS)) {
        selectPending = false;
        return EVT_SELECT;
    }
    
    for (int i = 0; i < 3; i++) {
        bool pressed = (digitalRead(buttonPins[i]) == LOW);
        
        if (pressed && !btnState[i]) {
            // New press
            if (now < nextEventTime[i]) continue;   // ignore bounce
            btnState[i] = true;
            pressStart[i] = now;
            lastRepeat[i] = now;
            nextEventTime[i] = now + DEBOUNCE_MS;
            
            if (i == 0) return EVT_UP;
            if (i == 1) return EVT_DOWN;
            if (i == 2) {
                // SELECT pressed
                if (selectPending && (now - lastSelectPress < DOUBLE_CLICK_MS)) {
                    // Double-click detected!
                    selectPending = false;
                    return EVT_BACK;
                } else {
                    // First click of a potential double-click
                    selectPending = true;
                    lastSelectPress = now;
                    // Do NOT return an event now; wait for double-click timeout
                }
            }
        }
        
        if (pressed && btnState[i]) {
            // Held down
            if ((i == 0 || i == 1) && (now - pressStart[i] >= REPEAT_DELAY_MS)) {
                if (now - lastRepeat[i] >= REPEAT_INTERVAL_MS) {
                    lastRepeat[i] = now;
                    if (i == 0) return EVT_UP;
                    if (i == 1) return EVT_DOWN;
                }
            }
        }
        
        if (!pressed && btnState[i]) {
            // Released
            btnState[i] = false;
        }
    }
    
    return EVT_NONE;
}
