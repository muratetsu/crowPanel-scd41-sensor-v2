#include "Screen_Menu.h"

static void menu_wifi_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showWiFiScreen();
  }
}

static void menu_dateset_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showDateSetScreen();
  }
}

static void menu_datetime_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showSensorScreen();
  }
}

void createMenuUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_make(15, 20, 40), 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Menu");
  lv_obj_set_style_text_color(title, lv_color_make(120, 180, 255), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *sep = lv_obj_create(scr);
  lv_obj_set_size(sep, screenWidth - 40, 1);
  lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_set_style_bg_color(sep, lv_color_make(60, 80, 130), 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

  // 1. Wi-Fi Settings
  lv_obj_t *btn_wifi = lv_btn_create(scr);
  lv_obj_set_size(btn_wifi, screenWidth - 60, 46);
  lv_obj_align(btn_wifi, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_set_style_bg_color(btn_wifi, lv_color_make(25, 80, 180), 0);
  lv_obj_set_style_bg_color(btn_wifi, lv_color_make(40, 110, 220), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn_wifi, 10, 0);
  lv_obj_t *lbl_wifi = lv_label_create(btn_wifi);
  lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI "  Wi-Fi Settings");
  lv_obj_set_style_text_color(lbl_wifi, lv_color_make(220, 235, 255), 0);
  lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_wifi);
  lv_obj_add_event_cb(btn_wifi, menu_wifi_cb, LV_EVENT_CLICKED, NULL);

  // 2. Set Date & Time
  lv_obj_t *btn_dateset = lv_btn_create(scr);
  lv_obj_set_size(btn_dateset, screenWidth - 60, 46);
  lv_obj_align(btn_dateset, LV_ALIGN_TOP_MID, 0, 106);
  lv_obj_set_style_bg_color(btn_dateset, lv_color_make(180, 100, 25), 0);
  lv_obj_set_style_bg_color(btn_dateset, lv_color_make(220, 130, 40), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn_dateset, 10, 0);
  lv_obj_t *lbl_dateset = lv_label_create(btn_dateset);
  lv_label_set_text(lbl_dateset, LV_SYMBOL_SETTINGS "  Set Date & Time");
  lv_obj_set_style_text_color(lbl_dateset, lv_color_make(255, 235, 210), 0);
  lv_obj_set_style_text_font(lbl_dateset, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_dateset);
  lv_obj_add_event_cb(btn_dateset, menu_dateset_cb, LV_EVENT_CLICKED, NULL);

  // 3. Display Date & Time
  lv_obj_t *btn_dt = lv_btn_create(scr);
  lv_obj_set_size(btn_dt, screenWidth - 60, 46);
  lv_obj_align(btn_dt, LV_ALIGN_TOP_MID, 0, 164);
  lv_obj_set_style_bg_color(btn_dt, lv_color_make(25, 100, 75), 0);
  lv_obj_set_style_bg_color(btn_dt, lv_color_make(35, 140, 105), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn_dt, 10, 0);
  lv_obj_t *lbl_dt = lv_label_create(btn_dt);
  lv_label_set_text(lbl_dt, LV_SYMBOL_LOOP "  Display Date & Time");
  lv_obj_set_style_text_color(lbl_dt, lv_color_make(210, 255, 235), 0);
  lv_obj_set_style_text_font(lbl_dt, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_dt);
  lv_obj_add_event_cb(btn_dt, menu_datetime_cb, LV_EVENT_CLICKED, NULL);

  Serial.println("[UI] Menu screen created.");
}
