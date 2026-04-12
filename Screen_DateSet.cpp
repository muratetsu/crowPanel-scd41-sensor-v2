#include "Screen_DateSet.h"
#include "Logger.h"
#include <time.h>
#include <sys/time.h>

static lv_obj_t *roller_year;
static lv_obj_t *roller_month;
static lv_obj_t *roller_day;
static lv_obj_t *roller_hour;
static lv_obj_t *roller_min;

void resetDateSetUI_Fields() {
  roller_year = NULL;
  roller_month = NULL;
  roller_day = NULL;
  roller_hour = NULL;
  roller_min = NULL;
}

static void btn_cancel_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showMenuScreen();
  }
}

static void btn_save_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    // Get values from rollers
    uint16_t y = 2024 + lv_roller_get_selected(roller_year);
    uint8_t m = 1 + lv_roller_get_selected(roller_month);
    uint8_t d = 1 + lv_roller_get_selected(roller_day);
    uint8_t h = lv_roller_get_selected(roller_hour);
    uint8_t min = lv_roller_get_selected(roller_min);

    // Set time
    struct tm t;
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = min;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    time_t timeSinceEpoch = mktime(&t);

    struct timeval tv;
    tv.tv_sec = timeSinceEpoch;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    LOG_I("UI", "Manual time set to: %04d/%02d/%02d %02d:%02d:00", y, m, d, h, min);

    // Go back to Menu
    showMenuScreen();
  }
}

void createDateSetUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_make(15, 20, 40), 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Set Date & Time");
  lv_obj_set_style_text_color(title, lv_color_make(120, 180, 255), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  // Initialize current time or defaults
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 10);
  int cur_y = 2024, cur_m = 1, cur_d = 1, cur_h = 0, cur_min = 0;
  if (timeValid && timeinfo.tm_year > (2016-1900)) {
    cur_y = timeinfo.tm_year + 1900;
    cur_m = timeinfo.tm_mon + 1;
    cur_d = timeinfo.tm_mday;
    cur_h = timeinfo.tm_hour;
    cur_min = timeinfo.tm_min;
  }

  // Common roller style
  static lv_style_t style_roller;
  static bool style_inited = false;
  if (!style_inited) {
    lv_style_init(&style_roller);
    lv_style_set_text_font(&style_roller, &lv_font_montserrat_14);
    lv_style_set_pad_all(&style_roller, 0);
    style_inited = true;
  }

  // Layout calculations (1 row for all rollers)
  int row_y = 60;
  
  // Year Roller
  roller_year = lv_roller_create(scr);
  lv_roller_set_options(roller_year, "2024\n2025\n2026\n2027\n2028\n2029\n2030\n2031\n2032\n2033", LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller_year, 3);
  lv_obj_set_width(roller_year, 66);
  lv_obj_align(roller_year, LV_ALIGN_TOP_LEFT, 10, row_y);
  lv_obj_add_style(roller_year, &style_roller, 0);
  if (cur_y >= 2024 && cur_y <= 2033) lv_roller_set_selected(roller_year, cur_y - 2024, LV_ANIM_OFF);

  // Month Roller
  roller_month = lv_roller_create(scr);
  lv_roller_set_options(roller_month, "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12", LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller_month, 3);
  lv_obj_set_width(roller_month, 42);
  lv_obj_align(roller_month, LV_ALIGN_TOP_LEFT, 84, row_y);
  lv_obj_add_style(roller_month, &style_roller, 0);
  if (cur_m >= 1 && cur_m <= 12) lv_roller_set_selected(roller_month, cur_m - 1, LV_ANIM_OFF);

  // Day Roller
  roller_day = lv_roller_create(scr);
  char day_opts[100] = {0};
  for(int d=1; d<=31; d++){
    char buf[5]; snprintf(buf, sizeof(buf), "%02d", d);
    strcat(day_opts, buf);
    if(d<31) strcat(day_opts, "\n");
  }
  lv_roller_set_options(roller_day, day_opts, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller_day, 3);
  lv_obj_set_width(roller_day, 42);
  lv_obj_align(roller_day, LV_ALIGN_TOP_LEFT, 134, row_y);
  lv_obj_add_style(roller_day, &style_roller, 0);
  if (cur_d >= 1 && cur_d <= 31) lv_roller_set_selected(roller_day, cur_d - 1, LV_ANIM_OFF);

  // Hour Roller
  roller_hour = lv_roller_create(scr);
  char hour_opts[80] = {0};
  for(int h=0; h<=23; h++){
    char buf[5]; snprintf(buf, sizeof(buf), "%02d", h);
    strcat(hour_opts, buf);
    if(h<23) strcat(hour_opts, "\n");
  }
  lv_roller_set_options(roller_hour, hour_opts, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller_hour, 3);
  lv_obj_set_width(roller_hour, 42);
  lv_obj_align(roller_hour, LV_ALIGN_TOP_LEFT, 194, row_y);
  lv_obj_add_style(roller_hour, &style_roller, 0);
  if (cur_h >= 0 && cur_h <= 23) lv_roller_set_selected(roller_hour, cur_h, LV_ANIM_OFF);

  // Minute Roller
  roller_min = lv_roller_create(scr);
  char min_opts[200] = {0};
  for(int m=0; m<=59; m++){
    char buf[5]; snprintf(buf, sizeof(buf), "%02d", m);
    strcat(min_opts, buf);
    if(m<59) strcat(min_opts, "\n");
  }
  lv_roller_set_options(roller_min, min_opts, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller_min, 3);
  lv_obj_set_width(roller_min, 42);
  lv_obj_align(roller_min, LV_ALIGN_TOP_LEFT, 244, row_y);
  lv_obj_add_style(roller_min, &style_roller, 0);
  if (cur_min >= 0 && cur_min <= 59) lv_roller_set_selected(roller_min, cur_min, LV_ANIM_OFF);

  // Layout calculations for buttons
  int btn_y = 175;

  // Cancel Button
  lv_obj_t *btn_cancel = lv_btn_create(scr);
  lv_obj_set_size(btn_cancel, 90, 44);
  lv_obj_align(btn_cancel, LV_ALIGN_TOP_LEFT, 50, btn_y);
  lv_obj_set_style_bg_color(btn_cancel, lv_color_make(100, 100, 100), 0);
  lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
  lv_label_set_text(lbl_cancel, "Cancel");
  lv_obj_center(lbl_cancel);
  lv_obj_add_event_cb(btn_cancel, btn_cancel_cb, LV_EVENT_CLICKED, NULL);

  // Save Button
  lv_obj_t *btn_save = lv_btn_create(scr);
  lv_obj_set_size(btn_save, 90, 44);
  lv_obj_align(btn_save, LV_ALIGN_TOP_RIGHT, -50, btn_y);
  lv_obj_set_style_bg_color(btn_save, lv_color_make(60, 140, 80), 0);
  lv_obj_t *lbl_save = lv_label_create(btn_save);
  lv_label_set_text(lbl_save, "Save");
  lv_obj_center(lbl_save);
  lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);

  LOG_I("UI", "DateSet screen created.");
}
