#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <SensirionI2cScd4x.h>

#include "Globals.h"
#include "Screen_WiFi.h"
#include "Screen_Sensor.h"
#include "Screen_Menu.h"
#include "Screen_DateSet.h"
#include "HistoryManager.h"

// ============================================================
// グローバルオブジェクト定義
// ============================================================
TFT_eSPI    lcd = TFT_eSPI();
Preferences prefs;

AppScreen currentScreen = SCREEN_NONE;

SensirionI2cScd4x scd4x;
uint16_t currentCO2 = 0;
float currentTemp = 0.0f;
float currentHumid = 0.0f;
bool sensorDataValid = false;

// 1分間隔チャート記録用バッファ
uint32_t aggSumCO2 = 0;
float aggSumTemp = 0.0f;
float aggSumHumid = 0.0f;
uint16_t aggNumSamples = 0;

#define BACKLIGHT_PIN 27

#if defined(CROWPANEL_35)
  uint16_t calData[5] = { 353, 3568, 269, 3491, 7 };
#elif defined(CROWPANEL_24)
  uint16_t calData[5] = { 557, 3263, 369, 3493, 3 };
#elif defined(CROWPANEL_28)
  uint16_t calData[5] = { 189, 3416, 359, 3439, 1 };
#endif

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
    // Rotation(3)に合わせてタッチ座標を180度反転する
    data->point.x = screenWidth - touchX;
    data->point.y = screenHeight - touchY;
  }
}

// ============================================================
// NTP 時刻同期
// ============================================================
void syncNTP() {
  configTime(9 * 3600, 0, "ntp.nict.jp", "time.google.com");
  Serial.println("[NTP] configTime called (JST, UTC+9).");
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

// ============================================================
// WiFi接続状態チェック (loop() から毎サイクル呼ぶ)
// ============================================================
void checkWiFiStatus() {
  if (!wifiConnecting) return;

  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    wifiConnecting = false;
    Serial.println("[WiFi] Connected. IP: " + WiFi.localIP().toString());
    syncNTP();
    
    // 過去のログをSDからロード
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 2000)) {
        loadHistoryFromSD(&timeinfo);
    } else {
        Serial.println("[NTP] Failed to obtain time for SD history load");
    }

    showSensorScreen(); // 成功 → 常に画面2

  } else if (millis() - wifiStartTime > WIFI_TIMEOUT_MS) {
    wifiConnecting = false;
    WiFi.disconnect();
    Serial.println("[WiFi] Connection timed out.");

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

  Serial.printf("[Boot] Auto-connecting to SSID: %s\n", ssid.c_str());
}

// ============================================================
// SCD41 センサー初期化・データ取得
// ============================================================
void scd4xInit() {
  Wire.begin();
  scd4x.begin(Wire, SCD41_I2C_ADDR_62);

  if (scd4x.wakeUp()) { Serial.println("[SCD41] wakeUp error"); }
  if (scd4x.stopPeriodicMeasurement()) { Serial.println("[SCD41] stop error"); }
  if (scd4x.reinit()) { Serial.println("[SCD41] reinit error"); }
  
  uint64_t serial0;
  if (!scd4x.getSerialNumber(serial0)) {
    Serial.printf("[SCD41] Serial: 0x%04x%08x\n", (uint32_t)(serial0 >> 32), (uint32_t)(serial0 & 0xFFFFFFFF));
  }
  
  if (scd4x.startPeriodicMeasurement()) { Serial.println("[SCD41] start error"); }
  Serial.println("[SCD41] Initialized.");
}

void processSensorData() {
  static uint32_t lastRead = 0;
  // 5秒間隔で取得
  if (millis() - lastRead < 5000) return;
  
  uint16_t error;
  uint16_t co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;

  bool isDataReady = false;
  error = scd4x.getDataReadyStatus(isDataReady);
  // isDataReady indicates if new data is available
  if (error || !isDataReady) return;

  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (!error && co2 != 0) {
    currentCO2 = co2;
    currentTemp = temperature;
    currentHumid = humidity;
    sensorDataValid = true;
    lastRead = millis();
    
    // 集計用に追加
    aggSumCO2 += co2;
    aggSumTemp += temperature;
    aggSumHumid += humidity;
    aggNumSamples++;
  } else {
    sensorDataValid = false;
  }
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Boot] CrowPanel WiFi Config (LVGL) - 3-Screen");

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
  lcd.setTouch(calData);
  delay(100);

  // バックライト ON
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

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

  // SCD41初期化
  scd4xInit();

  // SDカード初期化
  initSD();

  // NVS に保存済み認証情報があるか確認
  prefs.begin("wifi_cfg", true);
  String savedSSID = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();

  if (savedSSID.length() == 0) {
    // 認証情報なし → メニュー画面へ
    Serial.println("[Boot] No saved credentials. → Menu screen.");
    showMenuScreen();
  } else {
    // 認証情報あり → 自動接続 (結果は loop() -> checkWiFiStatus() で処理)
    Serial.printf("[Boot] Found saved SSID: %s → Auto-connecting.\n", savedSSID.c_str());
    bootConnectWithSavedCredentials(savedSSID, savedPass);
  }

  Serial.println("[Boot] Setup done.");
}

// ============================================================
// Loop
// ============================================================
void loop() {
  lv_timer_handler();   // LVGLタイマー処理 (描画・イベント)
  checkWiFiStatus();    // WiFi接続状態チェック
  checkScanStatus();    // WiFiスキャン完了チェック

  processSensorData();  // センサーデータ取得

  if (currentScreen == SCREEN_SENSOR) {
    if (millis() - lastDateTimeUpdate >= 1000) {
      lastDateTimeUpdate = millis();
      updateSensorLabel();
    }
  }

  // 1分ごとの集計・チャートプロット処理
  static int prevMinute = -1;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    if (prevMinute != -1 && prevMinute != timeinfo.tm_min) {
      if (aggNumSamples > 0) {
        // 平均を求めてチャートにプロット
        uint16_t avgCO2 = aggSumCO2 / aggNumSamples;
        float avgTemp = aggSumTemp / aggNumSamples;
        float avgHumid = aggSumHumid / aggNumSamples;
        
        addChartData(avgCO2, avgTemp, avgHumid);
        
        // SDへログを書き込む
        writeLogToSD(&timeinfo, avgCO2, avgTemp, avgHumid);

        Serial.printf("[Graph] Plot -> CO2: %d, Temp: %.1f, Humid: %.1f (Samples: %d)\n", 
                      avgCO2, avgTemp, avgHumid, aggNumSamples);

        // クリア
        aggSumCO2 = 0;
        aggSumTemp = 0.0f;
        aggSumHumid = 0.0f;
        aggNumSamples = 0;
      }
    }
    prevMinute = timeinfo.tm_min;
  }

  delay(5);
}
