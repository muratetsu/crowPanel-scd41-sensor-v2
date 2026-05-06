#include "HistoryManager.h"
#include "Logger.h"
#include <SD.h>
#include <FS.h>
#include <SPI.h>

#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS_PIN 5

extern SPIClass SD_SPI;
SPIClass SD_SPI;

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

void initSD() {
  SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS_PIN, SD_SPI, 40000000)) {
    LOG_E("SD", "Mount Failed");
  } else {
    LOG_I("SD", "Initialized");
  }
}

void writeLogToSD(struct tm *timeinfo, uint16_t co2, float temperature, float humidity) {
  char logFileName[24];
  strftime(logFileName, sizeof(logFileName), "/%Y%m%d.csv", timeinfo);

  File file = SD.open(logFileName, FILE_APPEND);
  if (file) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    file.printf("%s, %d, %.2f, %.2f\n", timeStr, co2, temperature, humidity);
    file.close();
  } else {
    LOG_E("SD", "Failed to open log file: %s", logFileName);
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

static void processLogFile(const char* logFileName, time_t &t_cursor, time_t t_target_end) {
    File file = SD.open(logFileName, FILE_READ);
    if (!file) return;

    LOG_I("SD", "Loading log: %s", logFileName);
      
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() < 20) continue;
        
        int year, month, day, hour, minute, second;
        int co2;
        float temp, humid;
        
        int items = sscanf(line.c_str(), "%d-%d-%d %d:%d:%d, %d, %f, %f", 
                           &year, &month, &day, &hour, &minute, &second, 
                           &co2, &temp, &humid);
                           
        if (items >= 9) {
          struct tm tm_line = {0};
          tm_line.tm_year = year - 1900;
          tm_line.tm_mon = month - 1;
          tm_line.tm_mday = day;
          tm_line.tm_hour = hour;
          tm_line.tm_min = minute;
          // 各分ごとの最初のエントリのみ取得するため、秒は0に揃える
          tm_line.tm_sec = 0;
          tm_line.tm_isdst = -1;
          
          time_t t_log = mktime(&tm_line);
          
          if (t_log < t_cursor) continue;
          if (t_log > t_target_end) break;
          
          while (t_cursor < t_log) {
             addHistoryData(0, 0, 0); // Invalid値
             t_cursor += 60;
          }
          
          if (t_cursor == t_log) {
              addHistoryData(co2, temp, humid);
              t_cursor += 60;
          }
        }
    }
    file.close();
}

void loadHistoryFromSD(struct tm *now) {
  // バッファをゼロ（無効値）クリアし、インデックスをリセット
  histIdx = 0;
  for (int i=0; i<HISTORY_POINTS; i++) {
      histCO2[i] = 0;
      histTemp[i] = 0;
      histHumid[i] = 0;
  }

  struct tm tm_aligned = *now;
  tm_aligned.tm_sec = 0;
  time_t t_target_end = mktime(&tm_aligned);
  
  // 過去 HISTORY_POINTS 分だけ遡る
  time_t t_scan = t_target_end - (HISTORY_POINTS * 60); 
  time_t t_cursor = t_scan; 
  time_t t_file_check = t_scan;
  
  // 日付をまたぐ可能性を考慮してファイルを探査
  while (t_file_check <= t_target_end) {
    struct tm *tm_current = localtime(&t_file_check);
    
    struct tm start_of_day = *tm_current;
    start_of_day.tm_hour = 0;
    start_of_day.tm_min = 0;
    start_of_day.tm_sec = 0;
    start_of_day.tm_isdst = -1;

    char logFileName[24];
    strftime(logFileName, sizeof(logFileName), "/%Y%m%d.csv", tm_current);
    
    processLogFile(logFileName, t_cursor, t_target_end);
    
    start_of_day.tm_mday += 1;
    time_t t_next_day = mktime(&start_of_day);
    t_file_check = t_next_day;
  }
  
  // ログが途切れて現在に至る場合は残りを埋める
  while (t_cursor <= t_target_end) {
     addHistoryData(0, 0, 0);
     t_cursor += 60;
  }
  LOG_I("SD", "History (4H) loaded to memory.");
}

void loadDailyHistoryFromSD(struct tm *now) {
  // バッファをゼロ（無効値）クリア
  for (int i=0; i<HISTORY_DAILY_POINTS; i++) {
      dailyHistCO2[i] = 0;
      dailyHistTemp[i] = 0;
      dailyHistHumid[i] = 0;
  }
  
  uint32_t dailySumCO2[HISTORY_DAILY_POINTS] = {0};
  float dailySumTemp[HISTORY_DAILY_POINTS] = {0};
  float dailySumHumid[HISTORY_DAILY_POINTS] = {0};
  int dailyCount[HISTORY_DAILY_POINTS] = {0};

  time_t t_now = mktime(now);
  time_t t_start = t_now - (24 * 3600); // 24時間前

  // 昨日と今日のファイルを走査する
  for (int d = -1; d <= 0; d++) {
     time_t t_day = t_now + (d * 24 * 3600);
     struct tm *tm_day = localtime(&t_day);
     char logFileName[24];
     strftime(logFileName, sizeof(logFileName), "/%Y%m%d.csv", tm_day);
     
     File file = SD.open(logFileName, FILE_READ);
     if (file) {
         LOG_I("SD", "Loading daily logs from: %s", logFileName);
         while(file.available()) {
            String line = file.readStringUntil('\n');
            if (line.length() < 20) continue;
            
            int year, month, day, hour, min, sec;
            int co2; float temp, humid;
            if (sscanf(line.c_str(), "%d-%d-%d %d:%d:%d, %d, %f, %f", 
                           &year, &month, &day, &hour, &min, &sec, 
                           &co2, &temp, &humid) >= 9) {
                struct tm tm_line = {0};
                tm_line.tm_year = year - 1900;
                tm_line.tm_mon = month - 1;
                tm_line.tm_mday = day;
                tm_line.tm_hour = hour;
                tm_line.tm_min = min;
                tm_line.tm_sec = sec;
                tm_line.tm_isdst = -1;
                time_t t_log = mktime(&tm_line);
                
                // 直近24時間以内かチェック
                if (t_log >= t_start && t_log <= t_now) {
                    // バケツに入れる（0〜239番目。1バケツ＝6分）
                    long diff = t_log - t_start;
                    int bucket = diff / (6 * 60);
                    if (bucket >= 0 && bucket < HISTORY_DAILY_POINTS) {
                        dailySumCO2[bucket] += co2;
                        dailySumTemp[bucket] += temp;
                        dailySumHumid[bucket] += humid;
                        dailyCount[bucket]++;
                    }
                }
            }
         }
         file.close();
     }
  }
  
  // バケツごとに平均値を計算
  for(int i = 0; i < HISTORY_DAILY_POINTS; i++) {
     if (dailyCount[i] > 0) {
        dailyHistCO2[i] = dailySumCO2[i] / dailyCount[i];
        dailyHistTemp[i] = dailySumTemp[i] / dailyCount[i];
        dailyHistHumid[i] = dailySumHumid[i] / dailyCount[i];
     }
  }
  dailyHistIdx = 0;
  LOG_I("SD", "History (1D) loaded to memory.");
}
