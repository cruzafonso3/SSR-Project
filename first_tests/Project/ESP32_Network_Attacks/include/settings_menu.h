#ifndef SETTINGS_MENU_H
#define SETTINGS_MENU_H

#include "input_manager.h"

void settingsMenuOpen();
bool settingsMenuIsActive();
void settingsMenuUpdate(ButtonEvent evt);
void settingsMenuRender();

#endif
