#include "Screen_Sensor.h"
#include "HistoryManager.h"
#include "Logger.h"
#include <time.h>
#include <WiFi.h>
#include "ota.h"

static lv_obj_t *label_datetime = NULL;
static lv_obj_t *label_wifi = NULL;
static lv_obj_t *label_ota_icon = NULL;
static lv_obj_t *label_co2 = NULL;
static lv_obj_t *label_co2_unit = NULL;
static lv_obj_t *label_temp = NULL;
static lv_obj_t *label_temp_unit = NULL;
static lv_obj_t *label_humid = NULL;
static lv_obj_t *label_humid_unit = NULL;

#include "SensorChart.h"

// ─────────────────────────────────────────────────────────────
static lv_obj_t *span_btn = NULL;
static lv_obj_t *span_label = NULL;

// ─────────────────────────────────────────────────────────────
static void span_btn_event_cb(lv_event_t * e) {
    int new_mode = (SensorChart_GetMode() == 0) ? 1 : 0;
    SensorChart_SetMode(new_mode);
    if (span_label) {
        lv_label_set_text(span_label, (new_mode == 0) ? "4H" : "24H");
    }
    LOG_I("UI", "Switched to %s mode", (new_mode == 0) ? "4H" : "24H");
}

void resetSensorUI_Fields() {
  label_datetime = NULL;
  label_wifi = NULL;
  label_ota_icon = NULL;
  label_co2 = NULL;
  label_co2_unit = NULL;
  label_temp = NULL;
  label_temp_unit = NULL;
  label_humid = NULL;
  label_humid_unit = NULL;
  SensorChart_Reset();
}

static void datetime_touch_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showMenuScreen();
  }
}

void updateSensorLabel() {
  if (state.currentScreen != SCREEN_SENSOR || label_datetime == NULL) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    lv_label_set_text(label_datetime, "--- - --:--");
    return;
  }

  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char buf[40];
  snprintf(buf, sizeof(buf), "%s %d  %02d:%02d",
    months[timeinfo.tm_mon],
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min
  );
  lv_label_set_text(label_datetime, buf);

  if (label_wifi) {
    if (WiFi.status() == WL_CONNECTED) {
      lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
      lv_obj_set_style_text_color(label_wifi, THEME_TEXT_WHITE, 0); // White
    } else {
      lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
      lv_obj_set_style_text_color(label_wifi, THEME_TEXT_DARK, 0); // Dark Gray
    }
  }

  if (label_ota_icon) {
    if (otaIsUpdateAvailable()) {
      lv_obj_clear_flag(label_ota_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(label_ota_icon, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (state.sensorDataValid) {
    if (label_co2) lv_label_set_text_fmt(label_co2, "%d", state.currentCO2);
    if (label_temp) lv_label_set_text_fmt(label_temp, "%s", String(state.currentTemp, 1).c_str());
    if (label_humid) lv_label_set_text_fmt(label_humid, "%s", String(state.currentHumid, 1).c_str());
  } else {
    if (label_co2) lv_label_set_text(label_co2, "--");
    if (label_temp) lv_label_set_text(label_temp, "--");
    if (label_humid) lv_label_set_text(label_humid, "--");
  }
}

void updateSensorChartData(uint16_t co2, float temp, float humid) {
  if (state.currentScreen != SCREEN_SENSOR) return;
  SensorChart_UpdateData(co2, temp, humid);
}

void createSensorUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, THEME_BG_BLACK, 0);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, datetime_touch_cb, LV_EVENT_CLICKED, NULL);

  label_wifi = lv_label_create(scr);
  lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(label_wifi, THEME_TEXT_WHITE, 0); // initial white
  lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_16, 0);
  lv_obj_align(label_wifi, LV_ALIGN_TOP_LEFT, 0, 0);

  label_datetime = lv_label_create(scr);
  lv_label_set_text(label_datetime, "--- - --:--");
  lv_obj_set_style_text_color(label_datetime, THEME_TEXT_WHITE, 0);
  lv_obj_set_style_text_font(label_datetime, &lv_font_montserrat_20, 0); 
  lv_obj_align_to(label_datetime, label_wifi, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  // --- 期間切り替え用トグルボタン (4H / 24H) ---
  // タッチエリア用の透明な親コンテナ（大きめ）
  lv_obj_t *span_touch_area = lv_obj_create(scr);
  lv_obj_remove_style_all(span_touch_area);
  lv_obj_set_size(span_touch_area, 70, 50);
  lv_obj_align(span_touch_area, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(span_touch_area, span_btn_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_clear_flag(span_touch_area, LV_OBJ_FLAG_SCROLLABLE);

  // 見た目のボタン（小さめ）
  span_btn = lv_btn_create(span_touch_area);
  lv_obj_set_size(span_btn, 50, 25);
  lv_obj_align(span_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_clear_flag(span_btn, LV_OBJ_FLAG_CLICKABLE); // 親がタッチを受け取る
  
  // スタイル：白背景に黒文字
  lv_obj_set_style_bg_color(span_btn, THEME_TEXT_WHITE, 0);
  lv_obj_set_style_border_width(span_btn, 0, 0);
  lv_obj_set_style_pad_all(span_btn, 2, 0);

  // ボタン上に現在のモードを表示するラベル
  span_label = lv_label_create(span_btn);
  lv_label_set_text(span_label, (SensorChart_GetMode() == 0) ? "4H" : "24H");
  lv_obj_set_style_text_font(span_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(span_label, THEME_BG_BLACK, 0);
  lv_obj_center(span_label);

  // --- OTA Notification Icon ---
  label_ota_icon = lv_label_create(scr);
  lv_label_set_text(label_ota_icon, LV_SYMBOL_BELL);
  lv_obj_set_style_text_color(label_ota_icon, THEME_COLOR_WARNING, 0); // Orange/Yellow
  lv_obj_set_style_text_font(label_ota_icon, &lv_font_montserrat_16, 0); // same as wifi
  // 4H/24Hボタン(span_btn)の左隣に配置
  lv_obj_align_to(label_ota_icon, span_btn, LV_ALIGN_OUT_LEFT_MID, -10, 0);
  lv_obj_add_flag(label_ota_icon, LV_OBJ_FLAG_HIDDEN); // Hidden by default

  // --- CO2 ---
  label_co2 = lv_label_create(scr);
  lv_label_set_text(label_co2, "--");
  lv_obj_set_style_text_color(label_co2, THEME_COLOR_CO2, 0);
  lv_obj_set_style_text_font(label_co2, &lv_font_montserrat_24, 0); // 大きいフォント
  lv_obj_set_width(label_co2, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_co2, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_co2, LV_ALIGN_BOTTOM_LEFT, 5, 0);
  
  label_co2_unit = lv_label_create(scr);
  lv_label_set_text(label_co2_unit, "ppm");
  lv_obj_set_style_text_color(label_co2_unit, THEME_COLOR_CO2, 0);
  lv_obj_set_style_text_font(label_co2_unit, &lv_font_montserrat_12, 0); // 小さいフォント
  lv_obj_align_to(label_co2_unit, label_co2, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -3);
  
  // --- Temperature ---
  label_temp = lv_label_create(scr);
  lv_label_set_text(label_temp, "--");
  lv_obj_set_style_text_color(label_temp, THEME_COLOR_TEMP, 0);
  lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_24, 0);
  lv_obj_set_width(label_temp, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_temp, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_temp, LV_ALIGN_BOTTOM_MID, -15, 0);

  label_temp_unit = lv_label_create(scr);
  lv_label_set_text(label_temp_unit, "°C");
  lv_obj_set_style_text_color(label_temp_unit, THEME_COLOR_TEMP, 0);
  lv_obj_set_style_text_font(label_temp_unit, &lv_font_montserrat_12, 0);
  lv_obj_align_to(label_temp_unit, label_temp, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -3);
  
  // --- Humidity ---
  label_humid = lv_label_create(scr);
  lv_label_set_text(label_humid, "--");
  lv_obj_set_style_text_color(label_humid, THEME_COLOR_HUMID, 0);
  lv_obj_set_style_text_font(label_humid, &lv_font_montserrat_24, 0);
  lv_obj_set_width(label_humid, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_humid, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_humid, LV_ALIGN_BOTTOM_RIGHT, -50, 0);

  label_humid_unit = lv_label_create(scr);
  lv_label_set_text(label_humid_unit, "%");
  lv_obj_set_style_text_color(label_humid_unit, THEME_COLOR_HUMID, 0);
  lv_obj_set_style_text_font(label_humid_unit, &lv_font_montserrat_12, 0);
  lv_obj_align_to(label_humid_unit, label_humid, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -3);

  // Setup LVGL chart for graph using the extracted component
  SensorChart_Init(scr);

  LOG_I("UI", "Sensor screen created with LVGL chart and axes.");
}
