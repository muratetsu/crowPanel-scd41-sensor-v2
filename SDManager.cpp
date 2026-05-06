#include "SDManager.h"
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
  resetHistory();

  struct tm tm_aligned = *now;
  tm_aligned.tm_sec = 0;
  time_t t_target_end = mktime(&tm_aligned);
  
  time_t t_scan = t_target_end - (HISTORY_POINTS * 60); 
  time_t t_cursor = t_scan; 
  time_t t_file_check = t_scan;
  
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
  
  while (t_cursor <= t_target_end) {
     addHistoryData(0, 0, 0);
     t_cursor += 60;
  }
  LOG_I("SD", "History (4H) loaded to memory.");
}

void loadDailyHistoryFromSD(struct tm *now) {
  resetDailyHistory();
  
  uint32_t dailySumCO2[HISTORY_DAILY_POINTS] = {0};
  float dailySumTemp[HISTORY_DAILY_POINTS] = {0};
  float dailySumHumid[HISTORY_DAILY_POINTS] = {0};
  int dailyCount[HISTORY_DAILY_POINTS] = {0};

  time_t t_now = mktime(now);
  time_t t_start = t_now - (24 * 3600); // 24時間前

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
                
                if (t_log >= t_start && t_log <= t_now) {
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
  
  for(int i = 0; i < HISTORY_DAILY_POINTS; i++) {
     if (dailyCount[i] > 0) {
        setDailyHistoryData(i, dailySumCO2[i] / dailyCount[i], dailySumTemp[i] / dailyCount[i], dailySumHumid[i] / dailyCount[i]);
     }
  }
  LOG_I("SD", "History (1D) loaded to memory.");
}
