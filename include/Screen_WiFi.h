#ifndef SCREEN_WIFI_H
#define SCREEN_WIFI_H

#include "Globals.h"

void createWiFiUI(lv_obj_t *scr);
void setWiFiErrorLabel(const char *msg);
void checkScanStatus();
void resetWiFiUI_Fields();

#endif // SCREEN_WIFI_H
