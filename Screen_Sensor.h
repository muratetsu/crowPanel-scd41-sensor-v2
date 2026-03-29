#ifndef SCREEN_SENSOR_H
#define SCREEN_SENSOR_H

#include "Globals.h"

void createSensorUI(lv_obj_t *scr);
void updateSensorLabel();
void resetSensorUI_Fields();
void addChartData(uint16_t co2, float temp, float humid);

#endif // SCREEN_SENSOR_H
