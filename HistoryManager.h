#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <Arduino.h>
#include <time.h>

#define HISTORY_POINTS 60

// チャート用のメモリ内バッファ
extern uint16_t histCO2[HISTORY_POINTS];
extern float histTemp[HISTORY_POINTS];
extern float histHumid[HISTORY_POINTS];

void initSD();
void writeLogToSD(struct tm *timeinfo, uint16_t co2, float temperature, float humidity);
void loadHistoryFromSD(struct tm *now);
void addHistoryData(uint16_t co2, float temp, float humid);

#endif // HISTORYMANAGER_H
