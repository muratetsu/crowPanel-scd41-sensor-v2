#ifndef SCREEN_DATETIME_H
#define SCREEN_DATETIME_H

#include "Globals.h"

void createDateTimeUI(lv_obj_t *scr);
void updateDateTimeLabel();
void resetDateTimeUI_Fields();
void addChartData(uint16_t co2, float temp, float humid);

#endif // SCREEN_DATETIME_H
