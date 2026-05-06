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
static lv_obj_t *span_btnm = NULL;

// ─────────────────────────────────────────────────────────────
static void span_btnm_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    if(id != SensorChart_GetMode()) {
        SensorChart_SetMode(id);
        LOG_I("UI", "Switched to %s mode", (id == 0) ? "4H" : "1D");
    }
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
  if (currentScreen != SCREEN_SENSOR || label_datetime == NULL) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    lv_label_set_text(label_datetime, "----/--/-- --:--");
    return;
  }

  char buf[40];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d",
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min
  );
  lv_label_set_text(label_datetime, buf);

  if (label_wifi) {
    if (WiFi.status() == WL_CONNECTED) {
      lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
      lv_obj_set_style_text_color(label_wifi, lv_color_make(255, 255, 255), 0); // White
    } else {
      lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
      lv_obj_set_style_text_color(label_wifi, lv_color_make(100, 100, 100), 0); // Dark Gray
    }
  }

  if (label_ota_icon) {
    if (otaIsUpdateAvailable()) {
      lv_obj_clear_flag(label_ota_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(label_ota_icon, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (sensorDataValid) {
    if (label_co2) lv_label_set_text_fmt(label_co2, "%d", currentCO2);
    if (label_temp) lv_label_set_text_fmt(label_temp, "%s", String(currentTemp, 1).c_str());
    if (label_humid) lv_label_set_text_fmt(label_humid, "%s", String(currentHumid, 1).c_str());
  } else {
    if (label_co2) lv_label_set_text(label_co2, "--");
    if (label_temp) lv_label_set_text(label_temp, "--");
    if (label_humid) lv_label_set_text(label_humid, "--");
  }
}

void updateSensorChartData(uint16_t co2, float temp, float humid) {
  if (currentScreen != SCREEN_SENSOR) return;
  SensorChart_UpdateData(co2, temp, humid);
}

void createSensorUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_make(10, 15, 35), 0);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, datetime_touch_cb, LV_EVENT_CLICKED, NULL);

  label_wifi = lv_label_create(scr);
  lv_label_set_text(label_wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(label_wifi, lv_color_make(255, 255, 255), 0); // initial white
  lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_16, 0);
  lv_obj_align(label_wifi, LV_ALIGN_TOP_LEFT, 5, 7); // slightly lower to align with date text baseline

  label_datetime = lv_label_create(scr);
  lv_label_set_text(label_datetime, "----/--/-- --:--");
  lv_obj_set_style_text_color(label_datetime, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_text_font(label_datetime, &lv_font_montserrat_20, 0); 
  lv_obj_align_to(label_datetime, label_wifi, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  // --- 期間切り替え用ボタングループ (4H / 1D) ---
  span_btnm = lv_btnmatrix_create(scr);
  static const char * span_map[] = {"4H", "1D", ""};
  lv_btnmatrix_set_map(span_btnm, span_map);
  lv_obj_set_size(span_btnm, 80, 25);
  lv_obj_align(span_btnm, LV_ALIGN_TOP_RIGHT, -5, 5); // 右上に配置
  lv_btnmatrix_set_btn_ctrl_all(span_btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(span_btnm, true);
  // 初期状態を同期
  lv_btnmatrix_set_btn_ctrl(span_btnm, SensorChart_GetMode(), LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_add_event_cb(span_btnm, span_btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  // スタイルを画面に少し馴染ませる（ダークっぽく）
  lv_obj_set_style_bg_color(span_btnm, lv_color_make(30, 35, 50), 0);
  lv_obj_set_style_border_width(span_btnm, 0, 0);
  lv_obj_set_style_pad_all(span_btnm, 2, 0);
  lv_obj_set_style_text_font(span_btnm, &lv_font_montserrat_12, 0);

  // --- OTA Notification Icon ---
  label_ota_icon = lv_label_create(scr);
  lv_label_set_text(label_ota_icon, LV_SYMBOL_BELL);
  lv_obj_set_style_text_color(label_ota_icon, lv_color_make(255, 180, 50), 0); // Orange/Yellow
  lv_obj_set_style_text_font(label_ota_icon, &lv_font_montserrat_16, 0); // same as wifi
  // 4H/1Dボタン(span_btnm)の左隣に配置
  lv_obj_align_to(label_ota_icon, span_btnm, LV_ALIGN_OUT_LEFT_MID, -10, 0);
  lv_obj_add_flag(label_ota_icon, LV_OBJ_FLAG_HIDDEN); // Hidden by default

  // --- CO2 ---
  label_co2 = lv_label_create(scr);
  lv_label_set_text(label_co2, "--");
  lv_obj_set_style_text_color(label_co2, lv_color_make(150, 255, 150), 0);
  lv_obj_set_style_text_font(label_co2, &lv_font_montserrat_24, 0); // 大きいフォント
  lv_obj_set_width(label_co2, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_co2, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_co2, LV_ALIGN_BOTTOM_LEFT, 5, 0);
  
  label_co2_unit = lv_label_create(scr);
  lv_label_set_text(label_co2_unit, "ppm");
  lv_obj_set_style_text_color(label_co2_unit, lv_color_make(150, 255, 150), 0);
  lv_obj_set_style_text_font(label_co2_unit, &lv_font_montserrat_12, 0); // 小さいフォント
  lv_obj_align_to(label_co2_unit, label_co2, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -3);
  
  // --- Temperature ---
  label_temp = lv_label_create(scr);
  lv_label_set_text(label_temp, "--");
  lv_obj_set_style_text_color(label_temp, lv_color_make(255, 150, 150), 0);
  lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_24, 0);
  lv_obj_set_width(label_temp, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_temp, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_temp, LV_ALIGN_BOTTOM_MID, -15, 0);

  label_temp_unit = lv_label_create(scr);
  lv_label_set_text(label_temp_unit, "°C");
  lv_obj_set_style_text_color(label_temp_unit, lv_color_make(255, 150, 150), 0);
  lv_obj_set_style_text_font(label_temp_unit, &lv_font_montserrat_12, 0);
  lv_obj_align_to(label_temp_unit, label_temp, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -3);
  
  // --- Humidity ---
  label_humid = lv_label_create(scr);
  lv_label_set_text(label_humid, "--");
  lv_obj_set_style_text_color(label_humid, lv_color_make(150, 150, 255), 0);
  lv_obj_set_style_text_font(label_humid, &lv_font_montserrat_24, 0);
  lv_obj_set_width(label_humid, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_humid, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_humid, LV_ALIGN_BOTTOM_RIGHT, -50, 0);

  label_humid_unit = lv_label_create(scr);
  lv_label_set_text(label_humid_unit, "%");
  lv_obj_set_style_text_color(label_humid_unit, lv_color_make(150, 150, 255), 0);
  lv_obj_set_style_text_font(label_humid_unit, &lv_font_montserrat_12, 0);
  lv_obj_align_to(label_humid_unit, label_humid, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -3);

  // Setup LVGL chart for graph using the extracted component
  SensorChart_Init(scr);

  LOG_I("UI", "Sensor screen created with LVGL chart and axes.");
}
