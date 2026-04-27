#include "Screen_Menu.h"
#include "Screen_OTA.h"
#include "ota.h"
#include "Logger.h"
#include <WiFi.h>

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

static void menu_test_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    showTestScreen();
  }
}

static void menu_ota_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *lbl = lv_obj_get_child(btn, 0);

  if (WiFi.status() != WL_CONNECTED) {
    lv_label_set_text(lbl, LV_SYMBOL_WARNING "  No WiFi connection");
    LOG_E("OTA", "Manual check failed: WiFi not connected.");
    return;
  }

  if (otaIsUpdateAvailable()) {
    // 既に新バージョン検出済み → バナーを再表示
    otaShowNotifyBanner(otaGetServerVersion());
    return;
  }

  // "Checking..." を表示してから即時チェック実行
  // HTTP通信が含まれるため数秒間タッチ応答が遅くなる
  lv_label_set_text(lbl, LV_SYMBOL_REFRESH "  Checking...");
  lv_timer_handler();  // ラベル更新を描画に反映

  otaCheckNow();  // サーバを即時照会 (新版があれば notify_cb → バナー表示)

  // チェック完了後にラベルを元に戻す
  if (otaIsUpdateAvailable()) {
    lv_label_set_text(lbl, LV_SYMBOL_DOWNLOAD "  Update Found!");
  } else {
    lv_label_set_text(lbl, LV_SYMBOL_OK "  Up to Date");
  }
}

void createMenuUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_make(15, 20, 40), 0);

  // タイトル行 (バージョンを右側に並べて表示)
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Menu");
  lv_obj_set_style_text_color(title, lv_color_make(120, 180, 255), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

  lv_obj_t *lbl_ver = lv_label_create(scr);
  lv_label_set_text_fmt(lbl_ver, "FW: %s", otaGetLocalVersion());
  lv_obj_set_style_text_color(lbl_ver, lv_color_make(80, 100, 140), 0);
  lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_12, 0);
  lv_obj_align(lbl_ver, LV_ALIGN_TOP_RIGHT, -10, 13);

  lv_obj_t *sep = lv_obj_create(scr);
  lv_obj_set_size(sep, screenWidth - 20, 1);
  lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_set_style_bg_color(sep, lv_color_make(60, 80, 130), 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

  // ボタン共通サイズ・間隔
  // 240px 画面: タイトル行38px + ボタン5個×38px + 間隔4px×4 = 38+190+16 = 244px → ぴったり収まる
  const int16_t BTN_W    = screenWidth - 40;
  const int16_t BTN_H    = 38;
  const int16_t BTN_GAP  = 4;
  const int16_t BTN_TOP  = 40;  // sep の下から開始

  // 1. Sensor Dashboard
  lv_obj_t *btn1 = lv_btn_create(scr);
  lv_obj_set_size(btn1, BTN_W, BTN_H);
  lv_obj_align(btn1, LV_ALIGN_TOP_MID, 0, BTN_TOP + (BTN_H + BTN_GAP) * 0);
  lv_obj_set_style_bg_color(btn1, lv_color_make(25, 100, 75), 0);
  lv_obj_set_style_bg_color(btn1, lv_color_make(35, 140, 105), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn1, 8, 0);
  lv_obj_t *lbl1 = lv_label_create(btn1);
  lv_label_set_text(lbl1, LV_SYMBOL_IMAGE "  Sensor Dashboard");
  lv_obj_set_style_text_color(lbl1, lv_color_make(210, 255, 235), 0);
  lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl1);
  lv_obj_add_event_cb(btn1, menu_datetime_cb, LV_EVENT_CLICKED, NULL);

  // 2. Wi-Fi Settings
  lv_obj_t *btn2 = lv_btn_create(scr);
  lv_obj_set_size(btn2, BTN_W, BTN_H);
  lv_obj_align(btn2, LV_ALIGN_TOP_MID, 0, BTN_TOP + (BTN_H + BTN_GAP) * 1);
  lv_obj_set_style_bg_color(btn2, lv_color_make(25, 80, 180), 0);
  lv_obj_set_style_bg_color(btn2, lv_color_make(40, 110, 220), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn2, 8, 0);
  lv_obj_t *lbl2 = lv_label_create(btn2);
  lv_label_set_text(lbl2, LV_SYMBOL_WIFI "  Wi-Fi Settings");
  lv_obj_set_style_text_color(lbl2, lv_color_make(220, 235, 255), 0);
  lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl2);
  lv_obj_add_event_cb(btn2, menu_wifi_cb, LV_EVENT_CLICKED, NULL);

  // 3. Set Date & Time
  lv_obj_t *btn3 = lv_btn_create(scr);
  lv_obj_set_size(btn3, BTN_W, BTN_H);
  lv_obj_align(btn3, LV_ALIGN_TOP_MID, 0, BTN_TOP + (BTN_H + BTN_GAP) * 2);
  lv_obj_set_style_bg_color(btn3, lv_color_make(180, 100, 25), 0);
  lv_obj_set_style_bg_color(btn3, lv_color_make(220, 130, 40), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn3, 8, 0);
  lv_obj_t *lbl3 = lv_label_create(btn3);
  lv_label_set_text(lbl3, LV_SYMBOL_SETTINGS "  Set Date & Time");
  lv_obj_set_style_text_color(lbl3, lv_color_make(255, 235, 210), 0);
  lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl3);
  lv_obj_add_event_cb(btn3, menu_dateset_cb, LV_EVENT_CLICKED, NULL);

  // 4. Test Mode
  lv_obj_t *btn4 = lv_btn_create(scr);
  lv_obj_set_size(btn4, BTN_W, BTN_H);
  lv_obj_align(btn4, LV_ALIGN_TOP_MID, 0, BTN_TOP + (BTN_H + BTN_GAP) * 3);
  lv_obj_set_style_bg_color(btn4, lv_color_make(100, 30, 80), 0);
  lv_obj_set_style_bg_color(btn4, lv_color_make(140, 50, 110), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn4, 8, 0);
  lv_obj_t *lbl4 = lv_label_create(btn4);
  lv_label_set_text(lbl4, LV_SYMBOL_WARNING "  Test Mode");
  lv_obj_set_style_text_color(lbl4, lv_color_make(255, 200, 220), 0);
  lv_obj_set_style_text_font(lbl4, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl4);
  lv_obj_add_event_cb(btn4, menu_test_cb, LV_EVENT_CLICKED, NULL);

  // 5. Check for Update (OTA) — 条件なしで常時表示
  lv_obj_t *btn5 = lv_btn_create(scr);
  lv_obj_set_size(btn5, BTN_W, BTN_H);
  lv_obj_align(btn5, LV_ALIGN_TOP_MID, 0, BTN_TOP + (BTN_H + BTN_GAP) * 4);
  lv_obj_set_style_bg_color(btn5, lv_color_make(30, 80, 60), 0);
  lv_obj_set_style_bg_color(btn5, lv_color_make(45, 110, 85), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn5, 8, 0);
  lv_obj_t *lbl5 = lv_label_create(btn5);
  lv_label_set_text(lbl5, LV_SYMBOL_REFRESH "  Check for Update");
  lv_obj_set_style_text_color(lbl5, lv_color_make(180, 255, 210), 0);
  lv_obj_set_style_text_font(lbl5, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl5);
  lv_obj_add_event_cb(btn5, menu_ota_cb, LV_EVENT_CLICKED, NULL);

  LOG_I("UI", "Menu screen created.");
}

