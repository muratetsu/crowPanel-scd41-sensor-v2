#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <Arduino.h>
#include <time.h>

#define HISTORY_POINTS 240 // 1分ごとに1プロット (4時間 = 240点)
#define HISTORY_DAILY_POINTS 240 // 6分ごとに1プロット (24時間 = 240点)

// チャート用のメモリ内バッファ取得API (読み取り専用)
uint16_t getHistCO2(int idx);
float getHistTemp(int idx);
float getHistHumid(int idx);

uint16_t getDailyHistCO2(int idx);
float getDailyHistTemp(int idx);
float getDailyHistHumid(int idx);

void initSD();
void writeLogToSD(struct tm *timeinfo, uint16_t co2, float temperature, float humidity);
void loadHistoryFromSD(struct tm *now);
void loadDailyHistoryFromSD(struct tm *now);
void addHistoryData(uint16_t co2, float temp, float humid);
void updateDailyHistoryInRealTime(uint16_t co2, float temp, float humid);

#endif // HISTORYMANAGER_H
