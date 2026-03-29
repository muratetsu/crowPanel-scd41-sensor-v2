#include "Screen_DateTime.h"
#include <time.h>

static lv_obj_t *label_datetime = NULL;
static lv_obj_t *label_co2 = NULL;
static lv_obj_t *label_temp = NULL;
static lv_obj_t *label_humid = NULL;

void resetDateTimeUI_Fields() {
  label_datetime = NULL;
  label_co2 = NULL;
  label_temp = NULL;
  label_humid = NULL;
}

static void datetime_touch_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showMenuScreen();
  }
}

void updateDateTimeLabel() {
  if (currentScreen != SCREEN_DATETIME || label_datetime == NULL) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    lv_label_set_text(label_datetime, "----/--/--\n  --:--:--");
    return;
  }

  char buf[40];
  snprintf(buf, sizeof(buf), "%04d/%d/%d\n  %02d:%02d:%02d",
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );
  lv_label_set_text(label_datetime, buf);

  if (sensorDataValid) {
    if (label_co2) lv_label_set_text_fmt(label_co2, "CO2:   %d ppm", currentCO2);
    if (label_temp) lv_label_set_text_fmt(label_temp, "Temp:  %s °C", String(currentTemp, 1).c_str());
    if (label_humid) lv_label_set_text_fmt(label_humid, "Humid: %s %%", String(currentHumid, 1).c_str());
  } else {
    if (label_co2) lv_label_set_text(label_co2, "CO2:   -- ppm");
    if (label_temp) lv_label_set_text(label_temp, "Temp:  -- °C");
    if (label_humid) lv_label_set_text(label_humid, "Humid: -- %%");
  }
}

void createDateTimeUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_make(10, 15, 35), 0);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, datetime_touch_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *icon = lv_label_create(scr);
  lv_label_set_text(icon, LV_SYMBOL_LOOP);
  lv_obj_set_style_text_color(icon, lv_color_make(60, 100, 180), 0);
  lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
  lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 8, 8);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Date & Time");
  lv_obj_set_style_text_color(title, lv_color_make(100, 150, 220), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 34, 11);

  label_datetime = lv_label_create(scr);
  lv_label_set_text(label_datetime, "----/--/--\n  --:--:--");
  lv_obj_set_style_text_color(label_datetime, lv_color_make(200, 220, 255), 0);
  lv_obj_set_style_text_font(label_datetime, &lv_font_montserrat_20, 0);
  lv_label_set_long_mode(label_datetime, LV_LABEL_LONG_WRAP);
  lv_obj_align(label_datetime, LV_ALIGN_TOP_MID, 0, 45);

  label_co2 = lv_label_create(scr);
  lv_label_set_text(label_co2, "CO2:   -- ppm");
  lv_obj_set_style_text_color(label_co2, lv_color_make(150, 255, 150), 0);
  lv_obj_set_style_text_font(label_co2, &lv_font_montserrat_20, 0);
  lv_obj_align(label_co2, LV_ALIGN_TOP_LEFT, 50, 100);
  
  label_temp = lv_label_create(scr);
  lv_label_set_text(label_temp, "Temp:  -- °C");
  lv_obj_set_style_text_color(label_temp, lv_color_make(255, 150, 150), 0);
  lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_20, 0);
  lv_obj_align(label_temp, LV_ALIGN_TOP_LEFT, 50, 130);
  
  label_humid = lv_label_create(scr);
  lv_label_set_text(label_humid, "Humid: -- %%");
  lv_obj_set_style_text_color(label_humid, lv_color_make(150, 150, 255), 0);
  lv_obj_set_style_text_font(label_humid, &lv_font_montserrat_20, 0);
  lv_obj_align(label_humid, LV_ALIGN_TOP_LEFT, 50, 160);

  lv_obj_t *hint = lv_label_create(scr);
  lv_label_set_text(hint, "Tap anywhere to go to Menu");
  lv_obj_set_style_text_color(hint, lv_color_make(70, 90, 130), 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

  Serial.println("[UI] DateTime screen created.");
}
