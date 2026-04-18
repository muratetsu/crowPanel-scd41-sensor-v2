#include "SensorChart.h"
#include "HistoryManager.h"
#include <time.h>
#include <math.h>

static lv_obj_t *chart = NULL;
static lv_chart_series_t *ser_co2 = NULL;
static lv_chart_series_t *ser_temp = NULL;
static lv_chart_series_t *ser_humid = NULL;

static int currentChartMode = 0; // 0 = 4H, 1 = 1D
static lv_obj_t *sensor_screen_parent = NULL;

#define GRID_MARKS 9
static lv_obj_t *vline_objs[GRID_MARKS];
static lv_obj_t *xlabel_objs[GRID_MARKS];

// CO2 Y軸: スケール幅固定(1600)、中央をデータに合わせてスライド
#define CO2_YSTEP   200
#define CO2_YSPAN  1600

// Secondary Y軸
#define TEMP_YSTEP      2.0f
#define HUMID_YSTEP     5.0f
#define SEC_GRID_SCALE 125.0f
#define SEC_Y_CENTER   500.0f
#define SEC_YLABEL_N    9

static float tempOffset  = 25.0f;
static float humidOffset = 50.0f;
static float prevTempOffset  = -9999.0f;
static float prevHumidOffset = -9999.0f;

static lv_obj_t *ylabel_temp[SEC_YLABEL_N];
static lv_obj_t *ylabel_humid[SEC_YLABEL_N];

// ============================================================
// Internal Helper Functions
// ============================================================

static void updateCO2YRange() {
    if (chart == NULL) return;

    float vmin = 1e9f, vmax = -1e9f;
    int n_points = (currentChartMode == 0) ? HISTORY_POINTS : HISTORY_DAILY_POINTS;
    const uint16_t *data = (currentChartMode == 0) ? getHistCO2() : getDailyHistCO2();

    for (int i = 0; i < n_points; i++) {
        if (data[i] > 0) {
            if (data[i] < vmin) vmin = data[i];
            if (data[i] > vmax) vmax = data[i];
        }
    }

    if (vmin > vmax) {
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 400, 2000);
        return;
    }

    float offset = (vmin + vmax) / 2.0f;
    float latestVal = -1.0f;
    for (int i = n_points - 1; i >= 0; i--) {
        if (data[i] > 0) { latestVal = data[i]; break; }
    }

    if (latestVal > 0) {
        float halfSpan = CO2_YSPAN / 2.0f;
        float offsetLow  = latestVal - halfSpan;
        float offsetHigh = latestVal + halfSpan;

        if (offset < offsetLow) {
            offset = ceilf(offsetLow / CO2_YSTEP) * CO2_YSTEP;
        } else if (offset > offsetHigh) {
            offset = floorf(offsetHigh / CO2_YSTEP) * CO2_YSTEP;
        } else {
            offset = roundf(offset / CO2_YSTEP) * CO2_YSTEP;
        }
    } else {
        offset = roundf(offset / CO2_YSTEP) * CO2_YSTEP;
    }

    if (offset < CO2_YSPAN / 2.0f) {
        offset = CO2_YSPAN / 2.0f;
    }

    lv_coord_t y_min = (lv_coord_t)(offset - CO2_YSPAN / 2);
    lv_coord_t y_max = (lv_coord_t)(offset + CO2_YSPAN / 2);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
}

static lv_coord_t normTemp(float t) {
    float v = SEC_Y_CENTER + (t - tempOffset) / TEMP_YSTEP * SEC_GRID_SCALE;
    if (v < 0) v = 0; if (v > 1000) v = 1000;
    return (lv_coord_t)v;
}

static lv_coord_t normHumid(float h) {
    float v = SEC_Y_CENTER + (h - humidOffset) / HUMID_YSTEP * SEC_GRID_SCALE;
    if (v < 0) v = 0; if (v > 1000) v = 1000;
    return (lv_coord_t)v;
}

static float calcFloatOffset(const float *arr, int n, float ystep) {
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
    float halfSpan = 4.0f * ystep; 
    if (latest > 0) {
        if      (offset < latest - halfSpan) offset = ceilf((latest - halfSpan) / ystep) * ystep;
        else if (offset > latest + halfSpan) offset = floorf((latest + halfSpan) / ystep) * ystep;
        else                                 offset = roundf(offset / ystep) * ystep;
    } else {
        offset = roundf(offset / ystep) * ystep;
    }
    return offset;
}

static void repopulateChart() {
    if (chart == NULL) return;
    int n = (currentChartMode == 0) ? HISTORY_POINTS : HISTORY_DAILY_POINTS;
    const uint16_t *cArr = (currentChartMode == 0) ? getHistCO2()   : getDailyHistCO2();
    const float    *tArr = (currentChartMode == 0) ? getHistTemp()  : getDailyHistTemp();
    const float    *hArr = (currentChartMode == 0) ? getHistHumid() : getDailyHistHumid();
    lv_chart_set_point_count(chart, n);
    for (int i = 0; i < n; i++) {
        lv_chart_set_next_value(chart, ser_co2,   cArr[i] > 0     ? cArr[i]          : LV_CHART_POINT_NONE);
        lv_chart_set_next_value(chart, ser_temp,  tArr[i] > 0.0f  ? normTemp(tArr[i]) : LV_CHART_POINT_NONE);
        lv_chart_set_next_value(chart, ser_humid, hArr[i] > 0.0f  ? normHumid(hArr[i]) : LV_CHART_POINT_NONE);
    }
}

static void updateSecondaryYLabels() {
    if (chart == NULL || sensor_screen_parent == NULL) return;
    lv_obj_update_layout(chart);
    lv_area_t coords;
    lv_obj_get_content_coords(chart, &coords);
    lv_coord_t inner_h = coords.y2 - coords.y1;
    lv_coord_t lx_t = coords.x2 + 8;
    lv_coord_t lx_h = coords.x2 + 24;

    for (int k = 0; k < SEC_YLABEL_N; k++) {
        lv_coord_t y = coords.y2 - (lv_coord_t)(k * inner_h / 8.0f) - 7;
        int n = k - 4; 
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

static bool updateSecondaryRange() {
    int n = (currentChartMode == 0) ? HISTORY_POINTS : HISTORY_DAILY_POINTS;
    const float *tArr = (currentChartMode == 0) ? getHistTemp()  : getDailyHistTemp();
    const float *hArr = (currentChartMode == 0) ? getHistHumid() : getDailyHistHumid();
    float newT = calcFloatOffset(tArr, n, TEMP_YSTEP);
    float newH = calcFloatOffset(hArr, n, HUMID_YSTEP);
    if (newT < 0) newT = 25.0f;
    if (newH < 0) newH = 50.0f;

    float humidHalfSpan = 4.0f * HUMID_YSTEP;
    if (newH < humidHalfSpan) newH = humidHalfSpan; 
    if (newH > 100.0f - humidHalfSpan) newH = 100.0f - humidHalfSpan; 

    bool changed = (newT != prevTempOffset) || (newH != prevHumidOffset);
    tempOffset = newT;   prevTempOffset  = newT;
    humidOffset = newH;  prevHumidOffset = newH;
    return changed;
}

static void updateChartGridUI() {
    if (chart == NULL || sensor_screen_parent == NULL) return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return;

    lv_obj_update_layout(chart);
    lv_area_t coords;
    lv_obj_get_content_coords(chart, &coords);
    lv_coord_t inner_w = coords.x2 - coords.x1;
    lv_coord_t inner_h = coords.y2 - coords.y1;
    lv_coord_t label_y = coords.y2 + 2;

    if (currentChartMode == 0) {
        int cur_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;
        int T = (cur_min - (HISTORY_POINTS - 1) % 60 + 60) % 60;
        int idx_start = (60 - T % 60) % 60;

        for (int k = 0; k < GRID_MARKS; k++) {
            int data_idx = idx_start + k * 60;
            if (vline_objs[k]) {
                if (data_idx < HISTORY_POINTS) {
                    lv_coord_t x = coords.x1 + (lv_coord_t)data_idx * inner_w / (HISTORY_POINTS - 1);
                    lv_obj_set_pos(vline_objs[k], x, coords.y1);
                    lv_obj_set_size(vline_objs[k], 1, inner_h);
                } else {
                    lv_obj_set_size(vline_objs[k], 0, 0);
                }
            }
            if (xlabel_objs[k]) {
                if (data_idx < HISTORY_POINTS) {
                    lv_coord_t x = coords.x1 + (lv_coord_t)data_idx * inner_w / (HISTORY_POINTS - 1);
                    int abs_min = cur_min - (HISTORY_POINTS - 1) + data_idx;
                    int label_hour = (((abs_min % 1440) + 1440) % 1440) / 60;
                    lv_label_set_text_fmt(xlabel_objs[k], "%d:00", label_hour);
                    lv_obj_set_pos(xlabel_objs[k], x - 12, label_y);
                } else {
                    lv_label_set_text(xlabel_objs[k], "");
                }
            }
        }
    } else {
        int cur_bkt = (timeinfo.tm_hour * 60 + timeinfo.tm_min) / 6;
        int T = (cur_bkt - (HISTORY_DAILY_POINTS - 1) % 40 + 40) % 40;
        int idx_start = (40 - T % 40) % 40;

        for (int k = 0; k < GRID_MARKS; k++) {
            int data_idx = idx_start + k * 40;
            if (vline_objs[k]) {
                if (data_idx < HISTORY_DAILY_POINTS) {
                    lv_coord_t x = coords.x1 + (lv_coord_t)data_idx * inner_w / (HISTORY_DAILY_POINTS - 1);
                    lv_obj_set_pos(vline_objs[k], x, coords.y1);
                    lv_obj_set_size(vline_objs[k], 1, inner_h);
                } else {
                    lv_obj_set_size(vline_objs[k], 0, 0);
                }
            }
            if (xlabel_objs[k]) {
                if (data_idx < HISTORY_DAILY_POINTS) {
                    lv_coord_t x = coords.x1 + (lv_coord_t)data_idx * inner_w / (HISTORY_DAILY_POINTS - 1);
                    int abs_bkt = cur_bkt - (HISTORY_DAILY_POINTS - 1) + data_idx;
                    int label_hour = (((abs_bkt * 6 % 1440) + 1440) % 1440) / 60;
                    lv_label_set_text_fmt(xlabel_objs[k], "%d:00", label_hour);
                    lv_obj_set_pos(xlabel_objs[k], x - 12, label_y);
                } else {
                    lv_label_set_text(xlabel_objs[k], "");
                }
            }
        }
    }
}

// ============================================================
// Public API
// ============================================================

int SensorChart_GetMode() {
    return currentChartMode;
}

void SensorChart_SetMode(int mode) {
    if (chart == NULL || currentChartMode == mode) return;
    currentChartMode = mode;
    
    int n = (mode == 0) ? HISTORY_POINTS : HISTORY_DAILY_POINTS;
    lv_chart_set_point_count(chart, n);
    
    SensorChart_RefreshAll();
}

void SensorChart_RefreshAll() {
    if (chart == NULL) return;
    
    updateCO2YRange();
    updateSecondaryRange();
    repopulateChart();
    lv_chart_refresh(chart);
    updateSecondaryYLabels();
    updateChartGridUI();
}

void SensorChart_UpdateData(uint16_t co2, float temp, float humid) {
    if (chart == NULL) return;

    if (currentChartMode == 0) {
        updateCO2YRange(); 
        bool secChanged = updateSecondaryRange();
        if (secChanged) {
            repopulateChart();
        } else {
            lv_chart_set_next_value(chart, ser_co2,   co2   > 0     ? co2            : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_temp,  temp  > 0.0f  ? normTemp(temp)  : LV_CHART_POINT_NONE);
            lv_chart_set_next_value(chart, ser_humid, humid > 0.0f  ? normHumid(humid) : LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(chart);
        updateSecondaryYLabels();
        updateChartGridUI();
    } else {
        updateCO2YRange();
        updateSecondaryRange();
        repopulateChart();
        lv_chart_refresh(chart);
        updateSecondaryYLabels();
        updateChartGridUI();
    }
}

void SensorChart_Reset() {
    chart = NULL;
    ser_co2 = NULL;
    ser_temp = NULL;
    ser_humid = NULL;
    sensor_screen_parent = NULL;
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

void SensorChart_Init(lv_obj_t *parent) {
    sensor_screen_parent = parent;
    
    // Create chart background behind everything
    lv_obj_t *chart_bg = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_bg);
    lv_obj_set_size(chart_bg, screenWidth - 80, 150);
    lv_obj_align(chart_bg, LV_ALIGN_TOP_RIGHT, -40, 45);
    lv_obj_set_style_bg_opa(chart_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(chart_bg, lv_color_make(20, 25, 45), 0);
    lv_obj_set_style_border_color(chart_bg, lv_color_make(60, 70, 90), 0);
    lv_obj_set_style_border_width(chart_bg, 1, 0);

    for (int k = 0; k < GRID_MARKS; k++) {
        lv_obj_t *vl = lv_obj_create(parent);
        lv_obj_remove_style_all(vl);
        lv_obj_set_style_bg_opa(vl, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(vl, lv_color_make(60, 70, 90), 0);
        lv_obj_set_style_radius(vl, 0, 0);
        lv_obj_clear_flag(vl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(vl, 1, 1); 
        vline_objs[k] = vl;
    }

    chart = lv_chart_create(parent);
    
    lv_obj_set_size(chart, screenWidth - 80, 150);
    lv_obj_align(chart, LV_ALIGN_TOP_RIGHT, -40, 45);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_CLICKABLE); 

    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, lv_color_make(60, 70, 90), LV_PART_MAIN);

    lv_obj_set_style_pad_left(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(chart, 0, LV_PART_MAIN);

    lv_chart_set_point_count(chart, currentChartMode == 0 ? HISTORY_POINTS : HISTORY_DAILY_POINTS);

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 400, 2000);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 1000);

    lv_chart_set_div_line_count(chart, 9, 0);

    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 15);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 9, 1, true, 40);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 5, 2, 9, 1, false, 30);
    
    lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);
    lv_obj_set_style_text_color(chart, lv_color_make(150, 255, 150), LV_PART_TICKS); 

    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(chart, true, LV_PART_ITEMS);

    ser_co2 = lv_chart_add_series(chart, lv_color_make(150, 255, 150), LV_CHART_AXIS_PRIMARY_Y);
    ser_temp = lv_chart_add_series(chart, lv_color_make(255, 150, 150), LV_CHART_AXIS_SECONDARY_Y);
    ser_humid = lv_chart_add_series(chart, lv_color_make(150, 150, 255), LV_CHART_AXIS_SECONDARY_Y);

    for (int k = 0; k < GRID_MARKS; k++) {
        lv_obj_t *xl = lv_label_create(parent);
        lv_obj_set_style_text_font(xl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(xl, lv_color_make(180, 190, 210), 0);
        lv_label_set_text(xl, "");
        lv_obj_clear_flag(xl, LV_OBJ_FLAG_CLICKABLE);
        xlabel_objs[k] = xl;
    }

    for (int k = 0; k < SEC_YLABEL_N; k++) {
        lv_obj_t *yl_t = lv_label_create(parent);
        lv_obj_set_style_text_font(yl_t, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(yl_t, lv_color_make(255, 150, 150), 0); 
        lv_label_set_text(yl_t, "");
        lv_obj_clear_flag(yl_t, LV_OBJ_FLAG_CLICKABLE);
        ylabel_temp[k] = yl_t;

        lv_obj_t *yl_h = lv_label_create(parent);
        lv_obj_set_style_text_font(yl_h, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(yl_h, lv_color_make(150, 150, 255), 0); 
        lv_label_set_text(yl_h, "");
        lv_obj_clear_flag(yl_h, LV_OBJ_FLAG_CLICKABLE);
        ylabel_humid[k] = yl_h;
    }

    // Load initial data and update view
    SensorChart_RefreshAll();
}
