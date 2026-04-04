#include "Screen_Sensor.h"
#include "HistoryManager.h"
#include <time.h>

static lv_obj_t *label_datetime = NULL;
static lv_obj_t *label_co2 = NULL;
static lv_obj_t *label_co2_unit = NULL;
static lv_obj_t *label_temp = NULL;
static lv_obj_t *label_temp_unit = NULL;
static lv_obj_t *label_humid = NULL;
static lv_obj_t *label_humid_unit = NULL;

static lv_obj_t *chart = NULL;
static lv_chart_series_t *ser_co2 = NULL;
static lv_chart_series_t *ser_temp = NULL;
static lv_chart_series_t *ser_humid = NULL;

static void chart_draw_event_cb(lv_event_t * e);

static int currentChartMode = 0; // 0 = 1H (過去60分), 1 = 1D (過去24時間)
static lv_obj_t *span_btnm = NULL;

static void span_btnm_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    if(id == 0 && currentChartMode != 0) {
        currentChartMode = 0;
        Serial.println("[UI] Switched to 1H mode");
        lv_chart_set_point_count(chart, HISTORY_POINTS);
        
        for (int i = 0; i < HISTORY_POINTS; i++) {
            lv_chart_set_next_value(chart, ser_co2, histCO2[i] > 0 ? histCO2[i] : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_temp, histTemp[i] > 0.0f ? (lv_coord_t)histTemp[i] : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_humid, histHumid[i] > 0.0f ? (lv_coord_t)histHumid[i] : LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(chart);
        
    } else if(id == 1 && currentChartMode != 1) {
        currentChartMode = 1;
        Serial.println("[UI] Switched to 1D mode");

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 100)) {
            loadDailyHistoryFromSD(&timeinfo);
        }
        
        lv_chart_set_point_count(chart, HISTORY_DAILY_POINTS);

        for (int i = 0; i < HISTORY_DAILY_POINTS; i++) {
            lv_chart_set_next_value(chart, ser_co2, dailyHistCO2[i] > 0 ? dailyHistCO2[i] : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_temp, dailyHistTemp[i] > 0.0f ? (lv_coord_t)dailyHistTemp[i] : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_humid, dailyHistHumid[i] > 0.0f ? (lv_coord_t)dailyHistHumid[i] : LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(chart);
    }
}

static void chart_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL)) return;
    
    if (dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text) {
        if (currentChartMode == 0) {
            // 1H: dsc->value (0〜6) を 0, 10, 20...60 にする
            lv_snprintf(dsc->text, dsc->text_length, "%d", dsc->value * 10);
        } else {
            // 1D: dsc->value (0〜6) を 0, 4, 8, 12, 16, 20, 24 にする
            lv_snprintf(dsc->text, dsc->text_length, "%d", dsc->value * 4);
        }
    }
}

void resetSensorUI_Fields() {
  label_datetime = NULL;
  label_co2 = NULL;
  label_co2_unit = NULL;
  label_temp = NULL;
  label_temp_unit = NULL;
  label_humid = NULL;
  label_humid_unit = NULL;
  chart = NULL;
  span_btnm = NULL;
  ser_co2 = NULL;
  ser_temp = NULL;
  ser_humid = NULL;
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
    lv_label_set_text(label_datetime, "----/--/--\n  --:--:--");
    return;
  }

  char buf[40];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d",
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );
  lv_label_set_text(label_datetime, buf);

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

void addChartData(uint16_t co2, float temp, float humid) {
  // グローバルなメモリバッファにも保存
  addHistoryData(co2, temp, humid);

  if (chart == NULL || currentScreen != SCREEN_SENSOR) return;
  
  // 1H表示の時のみ1分ごとの更新をチャートにリアルタイムに流し込む
  if (currentChartMode == 0) {
      lv_chart_set_next_value(chart, ser_co2, co2 > 0 ? co2 : LV_CHART_POINT_NONE);
      lv_chart_set_next_value(chart, ser_temp, temp > 0.0f ? (lv_coord_t)temp : LV_CHART_POINT_NONE);
      lv_chart_set_next_value(chart, ser_humid, humid > 0.0f ? (lv_coord_t)humid : LV_CHART_POINT_NONE);
      lv_chart_refresh(chart);
  }
}

void createSensorUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_make(10, 15, 35), 0);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, datetime_touch_cb, LV_EVENT_CLICKED, NULL);

  label_datetime = lv_label_create(scr);
  lv_label_set_text(label_datetime, "----/--/-- --:--:--");
  lv_obj_set_style_text_color(label_datetime, lv_color_make(200, 220, 255), 0);
  lv_obj_set_style_text_font(label_datetime, &lv_font_montserrat_16, 0); 
  lv_obj_align(label_datetime, LV_ALIGN_TOP_LEFT, 5, 5);

  // --- 期間切り替え用ボタングループ (1H / 1D) ---
  span_btnm = lv_btnmatrix_create(scr);
  static const char * span_map[] = {"1H", "1D", ""};
  lv_btnmatrix_set_map(span_btnm, span_map);
  lv_obj_set_size(span_btnm, 80, 25);
  lv_obj_align(span_btnm, LV_ALIGN_TOP_RIGHT, -5, 5); // 右上に配置
  lv_btnmatrix_set_btn_ctrl_all(span_btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(span_btnm, true);
  // 初期状態を同期
  lv_btnmatrix_set_btn_ctrl(span_btnm, currentChartMode, LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_add_event_cb(span_btnm, span_btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  // スタイルを画面に少し馴染ませる（ダークっぽく）
  lv_obj_set_style_bg_color(span_btnm, lv_color_make(30, 35, 50), 0);
  lv_obj_set_style_border_width(span_btnm, 0, 0);
  lv_obj_set_style_pad_all(span_btnm, 2, 0);
  lv_obj_set_style_text_font(span_btnm, &lv_font_montserrat_12, 0);

  label_co2 = lv_label_create(scr);
  lv_label_set_text(label_co2, "--");
  lv_obj_set_style_text_color(label_co2, lv_color_make(150, 255, 150), 0);
  lv_obj_set_style_text_font(label_co2, &lv_font_montserrat_24, 0); // 大きいフォント
  lv_obj_set_width(label_co2, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_co2, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_co2, LV_ALIGN_TOP_LEFT, 5, 25);
  
  label_co2_unit = lv_label_create(scr);
  lv_label_set_text(label_co2_unit, "ppm");
  lv_obj_set_style_text_color(label_co2_unit, lv_color_make(150, 255, 150), 0);
  lv_obj_set_style_text_font(label_co2_unit, &lv_font_montserrat_12, 0); // 小さいフォント
  lv_obj_align_to(label_co2_unit, label_co2, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  
  // --- Temperature ---
  label_temp = lv_label_create(scr);
  lv_label_set_text(label_temp, "--");
  lv_obj_set_style_text_color(label_temp, lv_color_make(255, 150, 150), 0);
  lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_24, 0);
  lv_obj_set_width(label_temp, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_temp, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_temp, LV_ALIGN_TOP_MID, -15, 25);

  label_temp_unit = lv_label_create(scr);
  lv_label_set_text(label_temp_unit, "°C");
  lv_obj_set_style_text_color(label_temp_unit, lv_color_make(255, 150, 150), 0);
  lv_obj_set_style_text_font(label_temp_unit, &lv_font_montserrat_12, 0);
  lv_obj_align_to(label_temp_unit, label_temp, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  
  // --- Humidity ---
  label_humid = lv_label_create(scr);
  lv_label_set_text(label_humid, "--");
  lv_obj_set_style_text_color(label_humid, lv_color_make(150, 150, 255), 0);
  lv_obj_set_style_text_font(label_humid, &lv_font_montserrat_24, 0);
  lv_obj_set_width(label_humid, 65); // 固定幅にする
  lv_obj_set_style_text_align(label_humid, LV_TEXT_ALIGN_RIGHT, 0); // 右揃え
  lv_obj_align(label_humid, LV_ALIGN_TOP_RIGHT, -50, 25);

  label_humid_unit = lv_label_create(scr);
  lv_label_set_text(label_humid_unit, "%");
  lv_obj_set_style_text_color(label_humid_unit, lv_color_make(150, 150, 255), 0);
  lv_obj_set_style_text_font(label_humid_unit, &lv_font_montserrat_12, 0);
  lv_obj_align_to(label_humid_unit, label_humid, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);

  // Setup LVGL chart for graph
  chart = lv_chart_create(scr);
  
  // Y軸の目盛り（テキスト）はチャート領域の外側に描画されます。
  // そのため、画面の左右にはみ出ないように全体の幅を小さくします。
  // 数値とチャートの間隔を狭めるため、チャートの高さを 160 に増やして上に広げます。
  lv_obj_set_size(chart, screenWidth - 80, 160);
  // 少し右に寄せて、桁数の多いCO2（左軸）側の余裕を多め（約45px）に取ります
  lv_obj_align(chart, LV_ALIGN_BOTTOM_RIGHT, -30, -20);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_obj_clear_flag(chart, LV_OBJ_FLAG_CLICKABLE); // Pass clicks to screen

  // --- Dark Modeの設定（背景色とグリッド線） ---
  lv_obj_set_style_bg_color(chart, lv_color_make(20, 25, 45), LV_PART_MAIN);
  lv_obj_set_style_border_color(chart, lv_color_make(60, 70, 90), LV_PART_MAIN);
  lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN);
  lv_obj_set_style_line_color(chart, lv_color_make(60, 70, 90), LV_PART_MAIN); // グリッド線の色

  // 左右をギリギリまで使うため、チャート内のパディング（余白）を 0 に設定します
  lv_obj_set_style_pad_left(chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(chart, 5, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(chart, 0, LV_PART_MAIN);

  // 初回生成時は現在のモードのポイント数をセット
  lv_chart_set_point_count(chart, currentChartMode == 0 ? HISTORY_POINTS : HISTORY_DAILY_POINTS);

  // Axis ranges (Primary for CO2, Secondary for Temp/Humid)
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 400, 2000);
  lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);

  // Axis Ticks
  // X axis: 7 major ticks (0, 10, 20...60), draw size: 15px for text area
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 5, 2, 7, 2, true, 15);
  
  // Left Y axis (CO2): 5 major ticks (400, 800, 1200, 1600, 2000), draw size: 40px
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 5, 2, true, 40);
  
  // Right Y axis (Temp/Humid): 6 major ticks (0, 20, 40, 60, 80, 100), draw size: 30px
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 5, 2, 6, 2, true, 30);
  
  // チャート目盛り（Tick）のフォントサイズを小さくし、色をライトグレーにする
  lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);
  lv_obj_set_style_text_color(chart, lv_color_make(180, 190, 210), LV_PART_TICKS);

  // X軸の数値をカスタム表示（0,10..60）にするためのイベントコールバック
  lv_obj_add_event_cb(chart, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

  // Add series
  ser_co2 = lv_chart_add_series(chart, lv_color_make(150, 255, 150), LV_CHART_AXIS_PRIMARY_Y);
  ser_temp = lv_chart_add_series(chart, lv_color_make(255, 150, 150), LV_CHART_AXIS_SECONDARY_Y);
  ser_humid = lv_chart_add_series(chart, lv_color_make(150, 150, 255), LV_CHART_AXIS_SECONDARY_Y);

  // メモリから過去の履歴をプロットする
  if (currentChartMode == 0) {
      for (int i = 0; i < HISTORY_POINTS; i++) {
          lv_chart_set_next_value(chart, ser_co2, histCO2[i] > 0 ? histCO2[i] : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_temp, histTemp[i] > 0.0f ? (lv_coord_t)histTemp[i] : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_humid, histHumid[i] > 0.0f ? (lv_coord_t)histHumid[i] : LV_CHART_POINT_NONE);
      }
  } else {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 100)) {
          loadDailyHistoryFromSD(&timeinfo);
      }
      for (int i = 0; i < HISTORY_DAILY_POINTS; i++) {
          lv_chart_set_next_value(chart, ser_co2, dailyHistCO2[i] > 0 ? dailyHistCO2[i] : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_temp, dailyHistTemp[i] > 0.0f ? (lv_coord_t)dailyHistTemp[i] : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_humid, dailyHistHumid[i] > 0.0f ? (lv_coord_t)dailyHistHumid[i] : LV_CHART_POINT_NONE);
      }
  }
  lv_chart_refresh(chart);

  Serial.println("[UI] Sensor screen created with LVGL chart and axes.");
}
