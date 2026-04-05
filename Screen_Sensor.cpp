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

static int currentChartMode = 0; // 0 = 1H (過去60分), 1 = 1D (過去24時間)
static lv_obj_t *span_btnm = NULL;

// カスタム罫線・ラベル用オブジェクト（絶対時刻位置に動的配置）
#define GRID_MARKS 6
static lv_obj_t *vline_objs[GRID_MARKS];
static lv_obj_t *xlabel_objs[GRID_MARKS];
static lv_obj_t *sensor_screen = NULL;

// CO2 Y軸: スケール幅固定(1600)、中央をデータに合わせてスライド
#define CO2_YSTEP   200   // グリッド間隔 (旧版と同じ)
#define CO2_YSPAN  1600   // 表示幅 (400〜2000 = 1600)

// ─────────────────────────────────────────────────────────────
// CO2データのmin/maxからY軸 range を計算して適用する
// 旧バージョン graph.cpp の getOffset()/drawline() ロジックを移植
// ─────────────────────────────────────────────────────────────
static void updateCO2YRange() {
    if (chart == NULL) return;

    // --- 1. データのmin/max → 中央値 (getOffset相当) ---
    float vmin = 1e9f, vmax = -1e9f;
    int n_points = (currentChartMode == 0) ? HISTORY_POINTS : HISTORY_DAILY_POINTS;
    uint16_t *data = (currentChartMode == 0) ? histCO2 : dailyHistCO2;

    for (int i = 0; i < n_points; i++) {
        if (data[i] > 0) {
            if (data[i] < vmin) vmin = data[i];
            if (data[i] > vmax) vmax = data[i];
        }
    }

    if (vmin > vmax) {
        // 有効データなし: デフォルトのまま
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 400, 2000);
        return;
    }

    float offset = (vmin + vmax) / 2.0f; // 中央値

    // --- 2. 最新値が必ず見えるよう offset を補正 (drawline相当) ---
    // 最新データ = 配列の最後の有効値
    float latestVal = -1.0f;
    for (int i = n_points - 1; i >= 0; i--) {
        if (data[i] > 0) { latestVal = data[i]; break; }
    }

    if (latestVal > 0) {
        float halfSpan = CO2_YSPAN / 2.0f;
        float offsetLow  = latestVal - halfSpan;
        float offsetHigh = latestVal + halfSpan;

        if (offset < offsetLow) {
            // 最新値が上にはみ出る → offset を上に寄せる
            offset = ceilf(offsetLow / CO2_YSTEP) * CO2_YSTEP;
        } else if (offset > offsetHigh) {
            // 最新値が下にはみ出る → offset を下に寄せる
            offset = floorf(offsetHigh / CO2_YSTEP) * CO2_YSTEP;
        } else {
            // 最新値が収まっている → offset を CO2_YSTEP の倍数にスナップ
            offset = roundf(offset / CO2_YSTEP) * CO2_YSTEP;
        }
    } else {
        offset = roundf(offset / CO2_YSTEP) * CO2_YSTEP;
    }

    // --- 3. offset を中心に CO2_YSPAN の range を設定 ---
    lv_coord_t y_min = (lv_coord_t)(offset - CO2_YSPAN / 2);
    lv_coord_t y_max = (lv_coord_t)(offset + CO2_YSPAN / 2);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);

    Serial.printf("[Chart] CO2 Y range: %d ~ %d (offset=%.0f)\n", y_min, y_max, offset);
}

// ─────────────────────────────────────────────────────────────
// Secondary Y軸 (Temp/Humid): 正規化スケール (0-100) に固定、カスタムラベル
// 1グリッド = 2°C (Temp) = 5% (Humid) — 旧版と同じスケール
// ─────────────────────────────────────────────────────────────
#define TEMP_YSTEP      2.0f  // °C per grid
#define HUMID_YSTEP     5.0f  // % per grid
#define SEC_GRID_SCALE 12.5f  // normalized axis units per grid (100 / 8 intervals)
#define SEC_Y_CENTER   50.0f  // normalized center (0-100 axis)
#define SEC_YLABEL_N    9     // 9 horizontal grid lines = 9 labels per series

static float tempOffset  = 25.0f;
static float humidOffset = 50.0f;
static float prevTempOffset  = -9999.0f;
static float prevHumidOffset = -9999.0f;

static lv_obj_t *ylabel_temp[SEC_YLABEL_N];
static lv_obj_t *ylabel_humid[SEC_YLABEL_N];

// Temp/Humid データを 0-100 の正規化座標に変換してプロット
static lv_coord_t normTemp(float t) {
    float v = SEC_Y_CENTER + (t - tempOffset) / TEMP_YSTEP * SEC_GRID_SCALE;
    if (v < 0) v = 0; if (v > 100) v = 100;
    return (lv_coord_t)v;
}
static lv_coord_t normHumid(float h) {
    float v = SEC_Y_CENTER + (h - humidOffset) / HUMID_YSTEP * SEC_GRID_SCALE;
    if (v < 0) v = 0; if (v > 100) v = 100;
    return (lv_coord_t)v;
}

// float配列のオフセットを CO2 と同じアルゴリズムで計算
static float calcFloatOffset(float *arr, int n, float ystep) {
    float vmin = 1e9f, vmax = -1e9f, latest = -1.0f;
    for (int i = 0; i < n; i++) {
        if (arr[i] > 0) {
            if (arr[i] < vmin) vmin = arr[i];
            if (arr[i] > vmax) vmax = arr[i];
            latest = arr[i];
        }
    }
    if (vmin > vmax) return -1.0f;
    float offset = (vmin + vmax) / 2.0f;
    float halfSpan = 4.0f * ystep; // +-4グリッド
    if (latest > 0) {
        if      (offset < latest - halfSpan) offset = ceilf((latest - halfSpan) / ystep) * ystep;
        else if (offset > latest + halfSpan) offset = floorf((latest + halfSpan) / ystep) * ystep;
        else                                 offset = roundf(offset / ystep) * ystep;
    } else {
        offset = roundf(offset / ystep) * ystep;
    }
    return offset;
}

// 全シリーズを履歴配列から再構築（正規化値が変わった時）
static void repopulateChart() {
    if (chart == NULL) return;
    int n = (currentChartMode == 0) ? HISTORY_POINTS : HISTORY_DAILY_POINTS;
    uint16_t *cArr = (currentChartMode == 0) ? histCO2   : dailyHistCO2;
    float    *tArr = (currentChartMode == 0) ? histTemp  : dailyHistTemp;
    float    *hArr = (currentChartMode == 0) ? histHumid : dailyHistHumid;
    lv_chart_set_point_count(chart, n);
    for (int i = 0; i < n; i++) {
        lv_chart_set_next_value(chart, ser_co2,   cArr[i] > 0     ? cArr[i]          : LV_CHART_POINT_NONE);
        lv_chart_set_next_value(chart, ser_temp,  tArr[i] > 0.0f  ? normTemp(tArr[i]) : LV_CHART_POINT_NONE);
        lv_chart_set_next_value(chart, ser_humid, hArr[i] > 0.0f  ? normHumid(hArr[i]) : LV_CHART_POINT_NONE);
    }
}

// Secondary Y軸のカスタムラベルを更新 (チャート右側に配置)
static void updateSecondaryYLabels() {
    if (chart == NULL || sensor_screen == NULL) return;
    lv_obj_update_layout(chart);
    lv_area_t coords;
    lv_obj_get_content_coords(chart, &coords);
    lv_coord_t inner_h = coords.y2 - coords.y1;
    lv_coord_t lx_t = coords.x2 + 4;   // Temp label X
    lv_coord_t lx_h = coords.x2 + 20;  // Humid label X

    for (int k = 0; k < SEC_YLABEL_N; k++) {
        // k=0 → 軸value=0(下端), k=8 → 軸value=100(上端)
        lv_coord_t y = coords.y2 - (lv_coord_t)(k * 12.5f) * inner_h / 100 - 7;
        int n = k - 4; // -4, -3, -2, -1, 0, +1, +2, +3, +4
        if (ylabel_temp[k]) {
            lv_label_set_text_fmt(ylabel_temp[k], "%d", (int)roundf(tempOffset + n * TEMP_YSTEP));
            lv_obj_set_pos(ylabel_temp[k], lx_t, y);
        }
        if (ylabel_humid[k]) {
            lv_label_set_text_fmt(ylabel_humid[k], "%d", (int)roundf(humidOffset + n * HUMID_YSTEP));
            lv_obj_set_pos(ylabel_humid[k], lx_h, y);
        }
    }
}

// Temp/Humidのオフセットを計算。変化した場合 true を返す
static bool updateSecondaryRange() {
    int n = (currentChartMode == 0) ? HISTORY_POINTS : HISTORY_DAILY_POINTS;
    float *tArr = (currentChartMode == 0) ? histTemp  : dailyHistTemp;
    float *hArr = (currentChartMode == 0) ? histHumid : dailyHistHumid;
    float newT = calcFloatOffset(tArr, n, TEMP_YSTEP);
    float newH = calcFloatOffset(hArr, n, HUMID_YSTEP);
    if (newT < 0) newT = 25.0f;
    if (newH < 0) newH = 50.0f;
    bool changed = (newT != prevTempOffset) || (newH != prevHumidOffset);
    tempOffset = newT;   prevTempOffset  = newT;
    humidOffset = newH;  prevHumidOffset = newH;
    Serial.printf("[Chart] Sec Y: temp_off=%.0f, humid_off=%.0f%s\n", newT, newH, changed ? " [CHANGED]" : "");
    return changed;
}


// ─────────────────────────────────────────────────────────────
// カスタム罫線・ラベルを絶対時刻位置に更新する
// ─────────────────────────────────────────────────────────────
static void updateChartGridUI() {
    if (chart == NULL || sensor_screen == NULL) return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return;

    // レイアウト確定後に座標取得
    lv_obj_update_layout(chart);
    lv_area_t coords;
    lv_obj_get_content_coords(chart, &coords);
    lv_coord_t inner_w = coords.x2 - coords.x1;
    lv_coord_t inner_h = coords.y2 - coords.y1;
    lv_coord_t label_y = coords.y2 + 2; // チャート直下

    if (currentChartMode == 0) {
        // ─── 1H: 絶対10分マーク (0,10,20,30,40,50分) ───
        // data[59]=現在分, data[0]=59分前
        // idx_start: 左端に最も近い絶対10分マークのdata index
        int cur_min = timeinfo.tm_min;
        int idx_start = (10 - (cur_min + 1) % 10) % 10;

        for (int k = 0; k < GRID_MARKS; k++) {
            int data_idx = idx_start + k * 10;
            lv_coord_t x = coords.x1 + (lv_coord_t)data_idx * inner_w / (HISTORY_POINTS - 1);

            // 罫線の位置とサイズを更新
            if (vline_objs[k]) {
                lv_obj_set_pos(vline_objs[k], x, coords.y1);
                lv_obj_set_size(vline_objs[k], 1, inner_h);
            }

            // ラベルの位置とテキストを更新
            if (xlabel_objs[k]) {
                int label_min = (cur_min - 59 + data_idx + 60) % 60;
                lv_label_set_text_fmt(xlabel_objs[k], "%d", label_min);
                lv_obj_set_pos(xlabel_objs[k], x - 8, label_y);
            }
        }
    } else {
        // ─── 1D: 絶対4時間マーク (0,4,8,12,16,20時) ───
        // data[47]=最新30minバケツ, data[0]=47バケツ前(≈23.5h前)
        // 4h = 8バケツ。idx_startは左端に近い4h境界のdata index
        int cur_hh = timeinfo.tm_hour * 2 + (timeinfo.tm_min >= 30 ? 1 : 0);
        int idx_start = (8 - (cur_hh + 1) % 8) % 8;

        for (int k = 0; k < GRID_MARKS; k++) {
            int data_idx = idx_start + k * 8;
            lv_coord_t x = coords.x1 + (lv_coord_t)data_idx * inner_w / (HISTORY_DAILY_POINTS - 1);

            if (vline_objs[k]) {
                lv_obj_set_pos(vline_objs[k], x, coords.y1);
                lv_obj_set_size(vline_objs[k], 1, inner_h);
            }

            if (xlabel_objs[k]) {
                // 絶対時: data_idxは cur_hh から (47-data_idx) バケツ前 = (47-data_idx)/2h前
                int label_hour = ((cur_hh - 47 + data_idx + 96) / 2) % 24;
                lv_label_set_text_fmt(xlabel_objs[k], "%d", label_hour);
                lv_obj_set_pos(xlabel_objs[k], x - 8, label_y);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
static void span_btnm_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    if(id == 0 && currentChartMode != 0) {
        currentChartMode = 0;
        Serial.println("[UI] Switched to 1H mode");
        lv_chart_set_point_count(chart, HISTORY_POINTS);
        updateSecondaryRange();  // オフセットを先に計算
        for (int i = 0; i < HISTORY_POINTS; i++) {
            lv_chart_set_next_value(chart, ser_co2,   histCO2[i]   > 0     ? histCO2[i]           : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_temp,  histTemp[i]  > 0.0f  ? normTemp(histTemp[i])  : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_humid, histHumid[i] > 0.0f  ? normHumid(histHumid[i]) : LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(chart);
        updateCO2YRange();
        updateSecondaryYLabels();
        updateChartGridUI();
        
    } else if(id == 1 && currentChartMode != 1) {
        currentChartMode = 1;
        Serial.println("[UI] Switched to 1D mode");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 100)) { loadDailyHistoryFromSD(&timeinfo); }
        lv_chart_set_point_count(chart, HISTORY_DAILY_POINTS);
        updateSecondaryRange();  // オフセットを先に計算
        for (int i = 0; i < HISTORY_DAILY_POINTS; i++) {
            lv_chart_set_next_value(chart, ser_co2,   dailyHistCO2[i]   > 0     ? dailyHistCO2[i]              : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_temp,  dailyHistTemp[i]  > 0.0f  ? normTemp(dailyHistTemp[i])  : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_humid, dailyHistHumid[i] > 0.0f  ? normHumid(dailyHistHumid[i]) : LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(chart);
        updateCO2YRange();
        updateSecondaryYLabels();
        updateChartGridUI();
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
  sensor_screen = NULL;
  for (int k = 0; k < GRID_MARKS; k++) {
    vline_objs[k] = NULL;
    xlabel_objs[k] = NULL;
  }
  for (int k = 0; k < SEC_YLABEL_N; k++) {
    ylabel_temp[k]  = NULL;
    ylabel_humid[k] = NULL;
  }
  prevTempOffset  = -9999.0f;
  prevHumidOffset = -9999.0f;
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
      updateCO2YRange();  // CO2 Y軸レンジ更新
      bool secChanged = updateSecondaryRange();
      if (secChanged) {
          // Temp/Humid の正規化値が変わった→全データ再構築
          repopulateChart();
      } else {
          // 正規化値は変わらない→最新の1点だけ追加
          lv_chart_set_next_value(chart, ser_co2,   co2   > 0     ? co2            : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_temp,  temp  > 0.0f  ? normTemp(temp)  : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_humid, humid > 0.0f  ? normHumid(humid) : LV_CHART_POINT_NONE);
      }
      lv_chart_refresh(chart);
      updateSecondaryYLabels();
      updateChartGridUI();
  }
}

void createSensorUI(lv_obj_t *scr) {
  sensor_screen = scr;

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

  // Setup LVGL chart for graph
  chart = lv_chart_create(scr);
  
  // Y軸の目盛り（テキスト）はチャート領域の外側に描画されます。
  // そのため、画面の左右にはみ出ないように全体の幅を小さくします。
  // チャートを上部に、数値を下部に配置します。
  lv_obj_set_size(chart, screenWidth - 80, 150);
  // 少し右に寄せて、桁数の多いCO2（左軸）側の余裕を多めに取ります
  lv_obj_align(chart, LV_ALIGN_TOP_RIGHT, -40, 45);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_obj_clear_flag(chart, LV_OBJ_FLAG_CLICKABLE); // Pass clicks to screen

  // --- Dark Modeの設定（背景色とグリッド線） ---
  lv_obj_set_style_bg_color(chart, lv_color_make(20, 25, 45), LV_PART_MAIN);
  lv_obj_set_style_border_color(chart, lv_color_make(60, 70, 90), LV_PART_MAIN);
  lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN);
  lv_obj_set_style_line_color(chart, lv_color_make(60, 70, 90), LV_PART_MAIN);

  // 左右をギリギリまで使うため、チャート内のパディング（余白）を 0 に設定します
  lv_obj_set_style_pad_left(chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(chart, 0, LV_PART_MAIN);

  // 初回生成時は現在のモードのポイント数をセット
  lv_chart_set_point_count(chart, currentChartMode == 0 ? HISTORY_POINTS : HISTORY_DAILY_POINTS);

  // Axis ranges (Primary for CO2, Secondary for Temp/Humid)
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 400, 2000);
  lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);

  // 水平グリッド線: hdiv=9 → 9本合計(内部7本+枠2本) = Y軸9ラベル位置と一致
  // 縦グリッド線: 0 (カスタム vline_objs で代替)
  lv_chart_set_div_line_count(chart, 9, 0);

  // X軸: 目盛り線・ラベルなし（カスタム xlabel_objs で代替）
  // draw_size=15 でチャート下の空間は確保しつつ、ラベル非表示
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 15);
  
  // Left Y axis (CO2): 9 major ticks, draw size: 40px
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 9, 1, true, 40);
  
  // Right Y axis (Temp/Humid): 標準ラベル無効、draw_sizeだけ確保（カスタムラベル使用）
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 5, 2, 9, 1, false, 30);
  
  // チャートの標準目盛り（Tick）のフォント。現在有効なのは左Y軸(CO2)のラベルのみ
  lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);
  lv_obj_set_style_text_color(chart, lv_color_make(150, 255, 150), LV_PART_TICKS); // CO2 color

  // チャートのデータポイント（マーカー）のサイズを3pxに設定
  lv_obj_set_style_size(chart, 3, LV_PART_INDICATOR);
  
  // 線を非表示にしてマーカーのみを描画する（線の太さを0にする）
  lv_obj_set_style_line_width(chart, 0, LV_PART_ITEMS);

  // Add series
  ser_co2 = lv_chart_add_series(chart, lv_color_make(150, 255, 150), LV_CHART_AXIS_PRIMARY_Y);
  ser_temp = lv_chart_add_series(chart, lv_color_make(255, 150, 150), LV_CHART_AXIS_SECONDARY_Y);
  ser_humid = lv_chart_add_series(chart, lv_color_make(150, 150, 255), LV_CHART_AXIS_SECONDARY_Y);

  // メモリから過去の履歴をプロットする
  // まずオフセットを計算してから正規化値でためる
  if (currentChartMode == 0) {
      updateSecondaryRange();
      for (int i = 0; i < HISTORY_POINTS; i++) {
          lv_chart_set_next_value(chart, ser_co2,   histCO2[i]   > 0     ? histCO2[i]            : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_temp,  histTemp[i]  > 0.0f  ? normTemp(histTemp[i])  : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_humid, histHumid[i] > 0.0f  ? normHumid(histHumid[i]) : LV_CHART_POINT_NONE);
      }
  } else {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 100)) { loadDailyHistoryFromSD(&timeinfo); }
      updateSecondaryRange();
      for (int i = 0; i < HISTORY_DAILY_POINTS; i++) {
          lv_chart_set_next_value(chart, ser_co2,   dailyHistCO2[i]   > 0     ? dailyHistCO2[i]              : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_temp,  dailyHistTemp[i]  > 0.0f  ? normTemp(dailyHistTemp[i])  : LV_CHART_POINT_NONE);
          lv_chart_set_next_value(chart, ser_humid, dailyHistHumid[i] > 0.0f  ? normHumid(dailyHistHumid[i]) : LV_CHART_POINT_NONE);
      }
  }
  updateCO2YRange();
  lv_chart_refresh(chart);

  // ─── カスタム罫線・ラベルを画面上に重ねて配置 ───
  // チャートの後に生成することでZ順でチャートの上に描画される
  for (int k = 0; k < GRID_MARKS; k++) {
    // 縦線 (1px幅の矩形)
    lv_obj_t *vl = lv_obj_create(scr);
    lv_obj_remove_style_all(vl);
    lv_obj_set_style_bg_opa(vl, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(vl, lv_color_make(60, 70, 90), 0);
    lv_obj_set_style_radius(vl, 0, 0);
    lv_obj_clear_flag(vl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(vl, 1, 1); // 初期サイズ（updateChartGridUIで更新）
    vline_objs[k] = vl;

    // X軸ラベル
    lv_obj_t *xl = lv_label_create(scr);
    lv_obj_set_style_text_font(xl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(xl, lv_color_make(180, 190, 210), 0);
    lv_label_set_text(xl, "");
    lv_obj_clear_flag(xl, LV_OBJ_FLAG_CLICKABLE);
    xlabel_objs[k] = xl;
  }

  // レイアウト確定後にグリッドUIを初期配置
  updateChartGridUI();

  // Secondary Y軸（右側）のカスタムラベルを作成
  for (int k = 0; k < SEC_YLABEL_N; k++) {
    lv_obj_t *yl_t = lv_label_create(scr);
    lv_obj_set_style_text_font(yl_t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(yl_t, lv_color_make(255, 150, 150), 0); // Temp color
    lv_label_set_text(yl_t, "");
    lv_obj_clear_flag(yl_t, LV_OBJ_FLAG_CLICKABLE);
    ylabel_temp[k] = yl_t;

    lv_obj_t *yl_h = lv_label_create(scr);
    lv_obj_set_style_text_font(yl_h, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(yl_h, lv_color_make(150, 150, 255), 0); // Humid color
    lv_label_set_text(yl_h, "");
    lv_obj_clear_flag(yl_h, LV_OBJ_FLAG_CLICKABLE);
    ylabel_humid[k] = yl_h;
  }
  updateSecondaryYLabels();

  Serial.println("[UI] Sensor screen created with LVGL chart and axes.");
}
