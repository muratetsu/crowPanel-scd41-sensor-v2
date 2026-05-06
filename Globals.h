#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Preferences.h>

// ============================================================
// ボード設定 (解像度)
// ============================================================
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

// ============================================================
// バックライト設定
// ============================================================
#define BACKLIGHT_PIN       27
#define PWM_FREQ            5000
#define PWM_RESOLUTION      8
#define BRIGHTNESS_DAY      128   // 昼間輝度 (6:00〜22:00)
#define BRIGHTNESS_NIGHT    16    // 夜間輝度 (22:00〜6:00)
#define BACKLIGHT_HOUR_DAWN 6     // 夜間→昼間切替時刻
#define BACKLIGHT_HOUR_DUSK 22    // 昼間→夜間切替時刻

// ============================================================
// 画面管理
// ============================================================
enum AppScreen {
  SCREEN_NONE,
  SCREEN_WIFI,      // 画面1: WiFi設定
  SCREEN_SENSOR,    // 画面2: センサー値・グラフ・日時表示
  SCREEN_MENU,      // 画面3: メニュー
  SCREEN_DATESET,   // 画面4: 手動日時設定
  SCREEN_TEST,      // 画面5: テストモード
  SCREEN_OTA        // 画面6: OTA更新進捗 (内部使用)
};

struct AppState {
  AppScreen currentScreen;
  bool bootConnecting;
  bool wifiConnecting;
  uint32_t wifiStartTime;
  uint16_t currentCO2;
  float currentTemp;
  float currentHumid;
  bool sensorDataValid;
};

extern AppState state;
extern TFT_eSPI lcd;
extern Preferences prefs;

extern const uint32_t WIFI_TIMEOUT_MS;

// 画面間で共有する変数の実体を持つ場合はここに extern で記述

// ============================================================
// 各種制御関数 (実体は .ino や各 .cpp に定義)
// ============================================================
void syncNTP();
void showWiFiScreen();
void showSensorScreen();
void showMenuScreen();
void showDateSetScreen();
void showTestScreen();
void setBacklightBrightness(uint8_t brightness);
void updateBacklightBrightness();

// OTA 制御関数 (実体は ota.cpp)
void otaScheduleFirstCheck();  // WiFi接続完了時に .ino から呼ぶ

// エラーラベル制御用 (Screen_WiFi に存在)
void setWiFiErrorLabel(const char *msg);

// SCD41 センサーデータは AppState 構造体に移行しました

#endif // GLOBALS_H
