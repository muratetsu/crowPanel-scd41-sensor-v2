#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "Globals.h"
#include "Screen_WiFi.h"
#include "Screen_Sensor.h"
#include "Screen_Menu.h"
#include "Screen_DateSet.h"
#include "Screen_Test.h"
#include "Screen_OTA.h"
#include "HistoryManager.h"
#include "SDManager.h"
#include "SensorManager.h"
#include "SensorChart.h"
#include "Logger.h"
#include "ota.h"

// ============================================================
// グローバルオブジェクト定義
// ============================================================
TFT_eSPI    lcd = TFT_eSPI();
Preferences prefs;

AppScreen currentScreen = SCREEN_NONE;

uint16_t currentCO2 = 0;
float currentTemp = 0.0f;
float currentHumid = 0.0f;
bool sensorDataValid = false;

// 1分間隔チャート記録用バッファ
uint32_t aggSumCO2 = 0;
float aggSumTemp = 0.0f;
float aggSumHumid = 0.0f;
uint16_t aggNumSamples = 0;


// ============================================================
// タッチパネルのキャリブレーション
// ============================================================
void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // NVSからキャリブレーションデータを読み込む
  prefs.begin("touch", true);
  if (prefs.getBytesLength("calData") == sizeof(calData)) {
    prefs.getBytes("calData", calData, sizeof(calData));
    calDataOK = 1;
  }
  prefs.end();

  if (calDataOK) {
    lcd.setTouch(calData);
  } else {
    // データがない場合はキャリブレーション画面を表示
    lcd.fillScreen(TFT_BLACK);
    lcd.setCursor(20, 0);
    lcd.setTextFont(2);
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.println("Touch corners as indicated");

    lcd.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    // 取得したデータをNVSに保存
    prefs.begin("touch", false);
    prefs.putBytes("calData", calData, sizeof(calData));
    prefs.end();
  }
}

// LVGL用バッファ
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[screenWidth * screenHeight / 10];

// --- 起動時の自動接続中フラグ ---
bool bootConnecting = false;

// --- WiFi接続状態 ---
bool     wifiConnecting  = false;
uint32_t wifiStartTime   = 0;
const uint32_t WIFI_TIMEOUT_MS = 15000;

// --- 日時更新タイマ ---
static uint32_t lastDateTimeUpdate = 0;

// --- NTP同期状態 ---
bool     ntpSyncing      = false;
uint32_t ntpStartTime      = 0;
const uint32_t NTP_TIMEOUT_MS = 20000; // 20秒待機

// ============================================================
// LVGL ディスプレイフラッシュコールバック
// ============================================================
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.pushColors((uint16_t *)&color_p->full, w * h, true);
  lcd.endWrite();
  lv_disp_flush_ready(disp);
}

// ============================================================
// LVGL タッチ読み取りコールバック
// ============================================================
static uint16_t touchX, touchY;
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  bool touched = lcd.getTouch(&touchX, &touchY, 600);
  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state   = LV_INDEV_STATE_PR;
    // calibrateTouch によって Rotation に合わせた座標が取得できるため、そのまま使用する
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

// ============================================================
// NTP 時刻同期
// ============================================================
void syncNTP() {
  configTime(9 * 3600, 0, "ntp.nict.jp", "time.google.com");
  LOG_I("NTP", "configTime called (JST, UTC+9).");
}

// ============================================================
// 画面遷移定義
// ============================================================
void showWiFiScreen() {
  resetWiFiUI_Fields();
  currentScreen = SCREEN_WIFI;

  lv_obj_t *scr = lv_obj_create(NULL);
  createWiFiUI(scr);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showSensorScreen() {
  resetSensorUI_Fields();
  currentScreen = SCREEN_SENSOR;

  lv_obj_t *scr = lv_obj_create(NULL);
  createSensorUI(scr);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showMenuScreen() {
  currentScreen = SCREEN_MENU;

  lv_obj_t *scr = lv_obj_create(NULL);
  createMenuUI(scr);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showDateSetScreen() {
  resetDateSetUI_Fields();
  currentScreen = SCREEN_DATESET;

  lv_obj_t *scr = lv_obj_create(NULL);
  createDateSetUI(scr);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showTestScreen() {
  currentScreen = SCREEN_TEST;

  lv_obj_t *scr = lv_obj_create(NULL);
  createTestUI(scr);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

// ============================================================
// WiFi接続状態チェック (loop() から毎サイクル呼ぶ)
// ============================================================
void checkWiFiStatus() {
  if (!wifiConnecting) return;

  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    wifiConnecting = false;
    LOG_I("WiFi", "Connected. IP: %s", WiFi.localIP().toString().c_str());
    
    syncNTP();
    ntpSyncing = true;
    ntpStartTime = millis();

    // OTA初期化: WiFi接続完了後に呼ぶ。
    // pendingフラグがある場合はここでファームウェア更新を実行して再起動するため、
    // 以降の showSensorScreen() などは実行されない。
    otaInit();
    otaScheduleFirstCheck();

    showSensorScreen(); // ひとまず画面2へ遷移（時刻同期はバックグラウンドで継続）

  } else if (millis() - wifiStartTime > WIFI_TIMEOUT_MS) {
    wifiConnecting = false;
    WiFi.disconnect();
    LOG_E("WiFi", "Connection timed out.");

    if (bootConnecting) {
      bootConnecting = false;
      showMenuScreen(); // 起動時失敗 → 画面3
    } else {
      // WiFi設定画面から接続 → 画面1に留まってエラー表示
      setWiFiErrorLabel("[!] Connection failed. Check SSID/Password.");
    }
  }
}

// ============================================================
// NTP同期状態チェック (loop() から呼ぶ)
// ============================================================
void checkNTPStatus() {
  if (!ntpSyncing) return;

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) { // タイムアウト0で非ブロッキング
    ntpSyncing = false;
    LOG_I("NTP", "Time synchronized successfully.");

    // 時刻が取れたので履歴をロード
    loadHistoryFromSD(&timeinfo);
    loadDailyHistoryFromSD(&timeinfo);

    // チャートに履歴データを反映
    SensorChart_RefreshAll();

    // 輝度も再計算
    updateBacklightBrightness();
  } 
  else if (millis() - ntpStartTime > NTP_TIMEOUT_MS) {
    ntpSyncing = false;
    LOG_E("NTP", "Failed to obtain time (Timeout). History load skipped.");
    
    // 失敗しても再試行などはせず、次の自動集計タイミング等で getLocalTime されるのを待つ
  }
}


// ============================================================
// 起動時: 接続中画面を出してNVS認証情報で自動接続
// ============================================================
void bootConnectWithSavedCredentials(const String &ssid, const String &pass) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_make(15, 20, 40), 0);

  lv_obj_t *lbl = lv_label_create(scr);
  lv_label_set_text_fmt(lbl, LV_SYMBOL_WIFI "  Connecting to\n%s ...", ssid.c_str());
  lv_obj_set_style_text_color(lbl, lv_color_make(255, 220, 80), 0);
  lv_obj_center(lbl);
  lv_timer_handler();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  wifiConnecting = true;
  wifiStartTime  = millis();
  bootConnecting = true;

  LOG_I("Boot", "Auto-connecting to SSID: %s", ssid.c_str());
}

// ============================================================
// センサーからのデータ取得
// ============================================================
void processSensorData() {
  uint16_t co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;

  int status = SensorManager::readData(co2, temperature, humidity);
  
  if (status == 1) { // 取得成功
    currentCO2 = co2;
    currentTemp = temperature;
    currentHumid = humidity;
    sensorDataValid = true;
    
    // 集計用に追加
    aggSumCO2 += co2;
    aggSumTemp += temperature;
    aggSumHumid += humidity;
    aggNumSamples++;
  } else if (status == -1) { // 取得エラー（Not Readyではなく明確なエラー）
    sensorDataValid = false;
  }
  // status == 0 (Not Ready) の場合は何もしない（前回値を保持）
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  LOG_I("Boot", "CrowPanel WiFi Config (LVGL) - 3-Screen");

  // ============================================================
  // ★ NVS削除用 (動作確認後は必ずこのブロックを削除すること) ★
#if 0
  prefs.begin("wifi_cfg", false);
  prefs.clear();
  prefs.end();
  Serial.println("[NVS] WiFi credentials cleared.");
#endif
  // ============================================================

  // WiFiハードウェア初期化 (LCD初期化前にSPI競合を回避)
  WiFi.mode(WIFI_STA);
  WiFi.begin("", "", 0, NULL, false);

  // LCD初期化
  lcd.begin();
  lcd.setRotation(3);
  lcd.fillScreen(TFT_BLACK);
  
  // タッチパネルのキャリブレーション（初回のみ画面表示）
  touch_calibrate();
  
  delay(100);

  // バックライト設定
  ledcAttach(BACKLIGHT_PIN, PWM_FREQ, PWM_RESOLUTION);
  updateBacklightBrightness(); // 初期輝度設定

  // LVGL初期化
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, screenWidth * screenHeight / 10);

  // ディスプレイドライバ登録
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = screenWidth;
  disp_drv.ver_res  = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // タッチドライバ登録
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // センサー初期化 (ダミー切替などはSensorManager内で完結)
  SensorManager::init();

  // SDカード初期化
  initSD();

  // NVS に保存済み認証情報があるか確認
  prefs.begin("wifi_cfg", true);
  String savedSSID = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();

  if (savedSSID.length() == 0) {
    // 認証情報なし → メニュー画面へ
    LOG_I("Boot", "No saved credentials. -> Menu screen.");
    showMenuScreen();
  } else {
    // 認証情報あり → 自動接続 (結果は loop() -> checkWiFiStatus() で処理)
    LOG_I("Boot", "Found saved SSID: %s -> Auto-connecting.", savedSSID.c_str());
    bootConnectWithSavedCredentials(savedSSID, savedPass);
  }

  LOG_I("Boot", "Setup done.");
}

// ============================================================
// センサーデータの集計と履歴・UI更新処理
// ============================================================
void processMinuteAggregation() {
  struct tm timeinfo;
  bool gotTime = getLocalTime(&timeinfo, 0);

  // 時間が切り替わったか（あるいは高速テストモードか）を判定
  if (SensorManager::isAggregationTime(&timeinfo, gotTime)) {
    if (aggNumSamples > 0) {
      // 平均を求める
      uint16_t avgCO2 = aggSumCO2 / aggNumSamples;
      float avgTemp = aggSumTemp / aggNumSamples;
      float avgHumid = aggSumHumid / aggNumSamples;
      
      // 1. HistoryManagerに保存 (モデルの更新)
      addHistoryData(avgCO2, avgTemp, avgHumid);
      updateDailyHistoryInRealTime(avgCO2, avgTemp, avgHumid);
      
      // 2. センサー画面のチャートを更新 (Viewの更新)
      updateSensorChartData(avgCO2, avgTemp, avgHumid);
      
      // 3. SDカードへ保存 (永続化)
      // センサー安定のため、電源投入から3分(180000ms)経過してからSDへログを書き込む
      if (millis() >= 180000) {
          writeLogToSD(&timeinfo, avgCO2, avgTemp, avgHumid);
      } else {
          LOG_D("SD", "Skip writing log to SD (warming up: < 3 mins)");
      }

      LOG_D("Graph", "Aggregated -> CO2: %d, Temp: %.1f, Humid: %.1f (Samples: %d)", 
                    avgCO2, avgTemp, avgHumid, aggNumSamples);

      // メモリ残量などのシステムヘルスを1分間隔（ダミー時は毎秒）で出力
      LOG_SYS_HEALTH();

      // クリア
      aggSumCO2 = 0;
      aggSumTemp = 0.0f;
      aggSumHumid = 0.0f;
      aggNumSamples = 0;

      // バックライト輝度更新
      updateBacklightBrightness();
    }
  }
}

// ============================================================
// バックライト制御
// ============================================================
void setBacklightBrightness(uint8_t brightness) {
  ledcWrite(BACKLIGHT_PIN, brightness);
}

void updateBacklightBrightness() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    int hour = timeinfo.tm_hour;
    if (hour >= BACKLIGHT_HOUR_DUSK || hour < BACKLIGHT_HOUR_DAWN) {
      setBacklightBrightness(BRIGHTNESS_NIGHT);
      LOG_D("Backlight", "Night mode (Brightness: %d)", BRIGHTNESS_NIGHT);
    } else {
      setBacklightBrightness(BRIGHTNESS_DAY);
      LOG_D("Backlight", "Day mode (Brightness: %d)", BRIGHTNESS_DAY);
    }
  } else {
    // 時刻未取得時はとりあえず昼間輝度にする
    setBacklightBrightness(BRIGHTNESS_DAY);
  }
}

// ============================================================
// Loop
// ============================================================
void loop() {
  lv_timer_handler();   // LVGLタイマー処理 (描画・イベント)
  checkWiFiStatus();    // WiFi接続状態チェック
  checkNTPStatus();     // NTP同期状態チェック
  checkScanStatus();    // WiFiスキャン完了チェック

  otaLoop();            // OTA定期チェック

  processSensorData();  // センサーデータ取得

  if (currentScreen == SCREEN_SENSOR) {
    if (millis() - lastDateTimeUpdate >= 1000) {
      lastDateTimeUpdate = millis();
      updateSensorLabel();
    }
  }

  processMinuteAggregation();

  delay(5);
}
