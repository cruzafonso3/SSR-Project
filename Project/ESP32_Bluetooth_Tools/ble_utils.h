#ifndef BLE_UTILS_H
#define BLE_UTILS_H

#include <Arduino.h>

void bleInit();
void bleScanStart();
void bleScanStop();
bool bleScanIsRunning();
int bleScanGetResultCount();
const char* bleScanGetResultName(int idx);
const char* bleScanGetResultMac(int idx);
int bleScanGetResultRssi(int idx);

void bleSpoofStart(const char* name);
void bleSpoofStop();
bool bleSpoofIsRunning();

#endif