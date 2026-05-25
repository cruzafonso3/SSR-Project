#ifndef DEAUTH_UTILS_H
#define DEAUTH_UTILS_H

#include <Arduino.h>

void deauthStart();
void deauthStop();
int deauthUpdate();
void deauthSetChannel(int ch);

#endif