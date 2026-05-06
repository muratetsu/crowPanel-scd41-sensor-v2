#include "HistoryManager.h"
#include "Logger.h"


uint16_t histCO2[HISTORY_POINTS] = {0};
float histTemp[HISTORY_POINTS] = {0};
float histHumid[HISTORY_POINTS] = {0};
int histIdx = 0;

uint16_t dailyHistCO2[HISTORY_DAILY_POINTS] = {0};
float dailyHistTemp[HISTORY_DAILY_POINTS] = {0};
float dailyHistHumid[HISTORY_DAILY_POINTS] = {0};
int dailyHistIdx = 0;

uint16_t getHistCO2(int idx) { int p = histIdx + idx; if (p >= HISTORY_POINTS) p -= HISTORY_POINTS; return histCO2[p]; }
float getHistTemp(int idx) { int p = histIdx + idx; if (p >= HISTORY_POINTS) p -= HISTORY_POINTS; return histTemp[p]; }
float getHistHumid(int idx) { int p = histIdx + idx; if (p >= HISTORY_POINTS) p -= HISTORY_POINTS; return histHumid[p]; }

uint16_t getDailyHistCO2(int idx) { int p = dailyHistIdx + idx; if (p >= HISTORY_DAILY_POINTS) p -= HISTORY_DAILY_POINTS; return dailyHistCO2[p]; }
float getDailyHistTemp(int idx) { int p = dailyHistIdx + idx; if (p >= HISTORY_DAILY_POINTS) p -= HISTORY_DAILY_POINTS; return dailyHistTemp[p]; }
float getDailyHistHumid(int idx) { int p = dailyHistIdx + idx; if (p >= HISTORY_DAILY_POINTS) p -= HISTORY_DAILY_POINTS; return dailyHistHumid[p]; }

void resetHistory() {
  histIdx = 0;
  for (int i=0; i<HISTORY_POINTS; i++) {
      histCO2[i] = 0;
      histTemp[i] = 0;
      histHumid[i] = 0;
  }
}

void resetDailyHistory() {
  dailyHistIdx = 0;
  for (int i=0; i<HISTORY_DAILY_POINTS; i++) {
      dailyHistCO2[i] = 0;
      dailyHistTemp[i] = 0;
      dailyHistHumid[i] = 0;
  }
}

void setDailyHistoryData(int idx, uint16_t co2, float temp, float humid) {
  if (idx >= 0 && idx < HISTORY_DAILY_POINTS) {
      dailyHistCO2[idx] = co2;
      dailyHistTemp[idx] = temp;
      dailyHistHumid[idx] = humid;
  }
}

void addHistoryData(uint16_t co2, float temp, float humid) {
    histCO2[histIdx] = co2;
    histTemp[histIdx] = temp;
    histHumid[histIdx] = humid;
    histIdx = (histIdx + 1) % HISTORY_POINTS;
}

static uint32_t dailySumCO2_rt = 0;
static float dailySumTemp_rt = 0;
static float dailySumHumid_rt = 0;
static int dailyCount_rt = 0;
static int _rtModeCurBucket = -1;

void updateDailyHistoryInRealTime(uint16_t co2, float temp, float humid) {
    if (co2 == 0) return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return;

    // 1バケツ6分間（360秒）。1Dモードと同じバケツ論理を使用。
    int cur_bkt = (timeinfo.tm_hour * 60 + timeinfo.tm_min) / 6;

    if (_rtModeCurBucket == -1) {
        _rtModeCurBucket = cur_bkt;
        dailySumCO2_rt = co2;
        dailySumTemp_rt = temp;
        dailySumHumid_rt = humid;
        dailyCount_rt = 1;
    } else if (_rtModeCurBucket == cur_bkt) {
        // 同じバケツ内のデータなので加算
        dailySumCO2_rt += co2;
        dailySumTemp_rt += temp;
        dailySumHumid_rt += humid;
        dailyCount_rt++;
    } else {
        // バケツ境界を越えた。リングバッファのインデックスを進める。
        int diff = (cur_bkt - _rtModeCurBucket + 240) % 240;
        if (diff > 240) diff = 240;

        for (int step = 0; step < diff; step++) {
            dailyHistCO2[dailyHistIdx] = 0;
            dailyHistTemp[dailyHistIdx] = 0;
            dailyHistHumid[dailyHistIdx] = 0;
            dailyHistIdx = (dailyHistIdx + 1) % HISTORY_DAILY_POINTS;
        }

        // 新しいバケツを開始
        _rtModeCurBucket = cur_bkt;
        dailySumCO2_rt = co2;
        dailySumTemp_rt = temp;
        dailySumHumid_rt = humid;
        dailyCount_rt = 1;
    }

    // 常に最新バケツに暫定移動平均を書き込む
    if (dailyCount_rt > 0) {
        int latestIdx = (dailyHistIdx + HISTORY_DAILY_POINTS - 1) % HISTORY_DAILY_POINTS;
        dailyHistCO2[latestIdx] = dailySumCO2_rt / dailyCount_rt;
        dailyHistTemp[latestIdx] = dailySumTemp_rt / dailyCount_rt;
        dailyHistHumid[latestIdx] = dailySumHumid_rt / dailyCount_rt;
    }
}

// (End of file, SD related loading functions have been moved to SDManager.cpp)
