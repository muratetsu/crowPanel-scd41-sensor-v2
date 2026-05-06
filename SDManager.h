#ifndef SDMANAGER_H
#define SDMANAGER_H

#include <Arduino.h>
#include <time.h>

void initSD();
void writeLogToSD(struct tm *timeinfo, uint16_t co2, float temperature, float humidity);
void loadHistoryFromSD(struct tm *now);
void loadDailyHistoryFromSD(struct tm *now);

#endif // SDMANAGER_H
