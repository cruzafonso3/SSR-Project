#include "buttons.h"

#define REPEAT_DELAY_MS    1000
#define REPEAT_INTERVAL_MS 80
#define DEBOUNCE_MS        150
#define DOUBLE_CLICK_MS    250

static bool s_btn_state[3] = {false, false, false};
static unsigned long s_press_start[3] = {0, 0, 0};
static unsigned long s_last_repeat[3] = {0, 0, 0};
static unsigned long s_next_event[3] = {0, 0, 0};
static const int s_pins[3] = {BTN_UP_PIN, BTN_DOWN_PIN, BTN_SELECT_PIN};

static bool s_select_pending = false;
static unsigned long s_last_select_press = 0;

void buttons_init() {
    for (int i = 0; i < 3; i++) {
        pinMode(s_pins[i], INPUT_PULLUP);
    }
}

ButtonEvent buttons_get_event() {
    unsigned long now = millis();

    if (s_select_pending && (now - s_last_select_press >= DOUBLE_CLICK_MS)) {
        s_select_pending = false;
        return EVT_SELECT;
    }

    for (int i = 0; i < 3; i++) {
        bool pressed = (digitalRead(s_pins[i]) == LOW);

        if (pressed && !s_btn_state[i]) {
            if (now < s_next_event[i]) continue;
            s_btn_state[i] = true;
            s_press_start[i] = now;
            s_last_repeat[i] = now;
            s_next_event[i] = now + DEBOUNCE_MS;

            if (i == 0) return EVT_UP;
            if (i == 1) return EVT_DOWN;
            if (i == 2) {
                if (s_select_pending && (now - s_last_select_press < DOUBLE_CLICK_MS)) {
                    s_select_pending = false;
                    return EVT_BACK;
                } else {
                    s_select_pending = true;
                    s_last_select_press = now;
                }
            }
        }

        if (pressed && s_btn_state[i] && (i == 0 || i == 1)) {
            if (now - s_press_start[i] >= REPEAT_DELAY_MS) {
                if (now - s_last_repeat[i] >= REPEAT_INTERVAL_MS) {
                    s_last_repeat[i] = now;
                    return (i == 0) ? EVT_UP : EVT_DOWN;
                }
            }
        }

        if (!pressed && s_btn_state[i]) {
            s_btn_state[i] = false;
        }
    }

    return EVT_NONE;
}
