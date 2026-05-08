#ifndef BT_CLASSIC_H
#define BT_CLASSIC_H

#include <Arduino.h>

void btClassicInit();
void btClassicScanStart();
void btClassicScanStop();
bool btClassicScanIsRunning();
int btClassicScanGetResultCount();
const char* btClassicScanGetResultName(int idx);

#endif