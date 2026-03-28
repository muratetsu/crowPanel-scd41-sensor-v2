#include "Screen_DateTime.h"
#include <time.h>

static lv_obj_t *label_datetime = NULL;

void resetDateTimeUI_Fields() {
  label_datetime = NULL;
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
  lv_obj_set_width(label_datetime, screenWidth - 20);
  lv_obj_align(label_datetime, LV_ALIGN_CENTER, 0, -10);

  lv_obj_t *hint = lv_label_create(scr);
  lv_label_set_text(hint, "Tap anywhere to go to Menu");
  lv_obj_set_style_text_color(hint, lv_color_make(70, 90, 130), 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

  Serial.println("[UI] DateTime screen created.");
}
