#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <Arduino.h>
#include <time.h>

#define HISTORY_POINTS 60
#define HISTORY_DAILY_POINTS 48 // 30分ごとに1プロット (24時間 = 48点)

// チャート用のメモリ内バッファ (1H用)
extern uint16_t histCO2[HISTORY_POINTS];
extern float histTemp[HISTORY_POINTS];
extern float histHumid[HISTORY_POINTS];

// チャート用のメモリ内バッファ (1D用)
extern uint16_t dailyHistCO2[HISTORY_DAILY_POINTS];
extern float dailyHistTemp[HISTORY_DAILY_POINTS];
extern float dailyHistHumid[HISTORY_DAILY_POINTS];

void initSD();
void writeLogToSD(struct tm *timeinfo, uint16_t co2, float temperature, float humidity);
void loadHistoryFromSD(struct tm *now);
void loadDailyHistoryFromSD(struct tm *now);
void addHistoryData(uint16_t co2, float temp, float humid);

#endif // HISTORYMANAGER_H
