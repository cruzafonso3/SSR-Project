#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "config.h"

enum ButtonEvent { EVT_NONE, EVT_UP, EVT_DOWN, EVT_SELECT };

void inputInit();
ButtonEvent inputGetEvent();

#endif
