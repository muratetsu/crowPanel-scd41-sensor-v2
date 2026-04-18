#include "SensorManager.h"
#include "Logger.h"
#include <Wire.h>

#ifndef USE_DUMMY_SENSOR
#include <SensirionI2cScd4x.h>
static SensirionI2cScd4x scd4x;
#endif

namespace SensorManager {

#ifdef USE_DUMMY_SENSOR
    static uint32_t lastRead = 0;
    static uint32_t lastAggTime = 0;
    static uint16_t last_error_code = 0;
    static uint16_t cached_asc = 1;
#else
    static uint32_t lastRead = 0;
    static int prevMinute = -1;
    static uint16_t last_error_code = 0;
    static uint16_t cached_asc = 0;
#endif

void init() {
#ifdef USE_DUMMY_SENSOR
    LOG_I("SensorManager", "Dummy mode: SCD41 hardware Init skipped.");
#else
    Wire.begin();
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);

    if (scd4x.wakeUp()) { LOG_E("SCD41", "wakeUp error"); }
    if (scd4x.stopPeriodicMeasurement()) { LOG_E("SCD41", "stop error"); }
    if (scd4x.reinit()) { LOG_E("SCD41", "reinit error"); }
    
    // アイドル状態の間にASC（自動キャリブレーション）の有効ステータスを読み取る
    scd4x.getAutomaticSelfCalibrationEnabled(cached_asc);
    
    uint64_t serial0;
    if (!scd4x.getSerialNumber(serial0)) {
        LOG_I("SCD41", "Serial: 0x%04x%08x", (uint32_t)(serial0 >> 32), (uint32_t)(serial0 & 0xFFFFFFFF));
    }
    
    if (scd4x.startPeriodicMeasurement()) { LOG_E("SCD41", "start error"); }
    LOG_I("SensorManager", "SCD41 Initialized.");
#endif
}

int readData(uint16_t &co2, float &temperature, float &humidity) {
#ifdef USE_DUMMY_SENSOR
    // ダミーモード時は1秒間隔でデータを素早く取得する
    if (millis() - lastRead < 1000) return 0; // SENSOR_NOT_READY
    
    static float phase = 0.0;
    co2 = 1200 + 800 * sin(phase);
    temperature = 25.0 + (random(-10, 10) / 10.0);
    humidity = 50.0 + (random(-50, 50) / 10.0);
    phase += 0.1;
    
    lastRead = millis();
    return 1; // SENSOR_SUCCESS

#else
    // 本番環境時は5秒間隔のポーリング
    if (millis() - lastRead < 5000) return 0; // SENSOR_NOT_READY

    bool isDataReady = false;
    uint16_t error = scd4x.getDataReadyStatus(isDataReady);
    if (error) last_error_code = error; // 保存
    
    if (error || !isDataReady) return 0; // SENSOR_NOT_READY (or busy)

    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
        last_error_code = error;
    } else if (co2 != 0) {
        last_error_code = 0; // 成功でクリア
        lastRead = millis();
        return 1; // SENSOR_SUCCESS
    }
    
    return -1; // SENSOR_ERROR
#endif
}

bool isAggregationTime(struct tm* timeinfo, bool gotTime) {
#ifdef USE_DUMMY_SENSOR
    // ダミーモード時：UIや履歴挙動のテストを高速化するため毎秒集計を発火させる
    if (millis() - lastAggTime >= 1000) {
        lastAggTime = millis();
        return true;
    }
    return false;

#else
    // 本番環境時：NTP取得済みの状態において「1分経過」したかどうかを判定
    if (!gotTime) return false;
    
    bool timeChanged = (prevMinute != -1 && prevMinute != timeinfo->tm_min);
    prevMinute = timeinfo->tm_min;
    
    return timeChanged;
#endif
}

uint16_t getLastError() {
    return last_error_code;
}

uint16_t getAscStatus() {
    return cached_asc;
}

} // namespace SensorManager
