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
    lv_label_set_text(lbl, LV_SYMBOL_WARNING "\nNo WiFi");
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
  lv_label_set_text(lbl, LV_SYMBOL_REFRESH "\nChecking...");
  lv_timer_handler();  // ラベル更新を描画に反映

  otaCheckNow();  // サーバを即時照会 (新版があれば notify_cb → バナー表示)

  // チェック完了後にラベルを元に戻す
  if (otaIsUpdateAvailable()) {
    // ボタンのテキストは元に戻し、ダイアログを表示する
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH "\nUpdate");
    otaShowNotifyBanner(otaGetServerVersion());
  } else {
    lv_label_set_text(lbl, LV_SYMBOL_OK "\nUp to Date");
  }
}

void createMenuUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, THEME_BG_MAIN, 0);

  // タイトル行 (バージョンを右側に並べて表示)
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Menu");
  lv_obj_set_style_text_color(title, THEME_TEXT_TITLE, 0);
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
  lv_obj_set_style_bg_color(sep, THEME_BORDER_LIGHT, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

  // ボタン共通サイズ・間隔 (2列表示 Flexレイアウト)
  const int16_t PADDING  = 16;  // 画面両端の余白
  const int16_t BTN_GAP  = 12;  // ボタン間の隙間
  const int16_t BTN_W    = (screenWidth - (PADDING * 2) - BTN_GAP) / 2;
  const int16_t BTN_H    = 65;  // 2行テキスト用に高さを確保
  const int16_t CONT_Y   = 40;  // sepの下から開始

  // メニュー用のFlexコンテナを作成
  lv_obj_t *cont = lv_obj_create(scr);
  lv_obj_set_size(cont, screenWidth, screenHeight - CONT_Y);
  lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, CONT_Y);
  
  // コンテナのスタイル設定
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, PADDING, 0);
  lv_obj_set_style_pad_row(cont, BTN_GAP, 0);
  lv_obj_set_style_pad_column(cont, BTN_GAP, 0);
  lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO); // はみ出た場合のみスクロール
  
  // Flexレイアウトの設定 (左上から右へ、端で折り返し)
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  // 1. Sensor Dashboard
  lv_obj_t *btn1 = lv_btn_create(cont);
  lv_obj_set_size(btn1, BTN_W, BTN_H);
  lv_obj_set_style_bg_color(btn1, lv_color_make(25, 100, 75), 0);
  lv_obj_set_style_bg_color(btn1, lv_color_make(35, 140, 105), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn1, 8, 0);
  lv_obj_t *lbl1 = lv_label_create(btn1);
  lv_label_set_text(lbl1, LV_SYMBOL_IMAGE "\nDashboard");
  lv_obj_set_style_text_align(lbl1, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lbl1, lv_color_make(210, 255, 235), 0);
  lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl1);
  lv_obj_add_event_cb(btn1, menu_datetime_cb, LV_EVENT_CLICKED, NULL);

  // 2. Wi-Fi Settings
  lv_obj_t *btn2 = lv_btn_create(cont);
  lv_obj_set_size(btn2, BTN_W, BTN_H);
  lv_obj_set_style_bg_color(btn2, lv_color_make(25, 80, 180), 0);
  lv_obj_set_style_bg_color(btn2, lv_color_make(40, 110, 220), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn2, 8, 0);
  lv_obj_t *lbl2 = lv_label_create(btn2);
  lv_label_set_text(lbl2, LV_SYMBOL_WIFI "\nWi-Fi");
  lv_obj_set_style_text_align(lbl2, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lbl2, lv_color_make(220, 235, 255), 0);
  lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl2);
  lv_obj_add_event_cb(btn2, menu_wifi_cb, LV_EVENT_CLICKED, NULL);

  // 3. Set Date & Time
  lv_obj_t *btn3 = lv_btn_create(cont);
  lv_obj_set_size(btn3, BTN_W, BTN_H);
  lv_obj_set_style_bg_color(btn3, lv_color_make(180, 100, 25), 0);
  lv_obj_set_style_bg_color(btn3, lv_color_make(220, 130, 40), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn3, 8, 0);
  lv_obj_t *lbl3 = lv_label_create(btn3);
  lv_label_set_text(lbl3, LV_SYMBOL_SETTINGS "\nDate/Time");
  lv_obj_set_style_text_align(lbl3, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lbl3, lv_color_make(255, 235, 210), 0);
  lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl3);
  lv_obj_add_event_cb(btn3, menu_dateset_cb, LV_EVENT_CLICKED, NULL);

  // 4. Check for Update (OTA)
  lv_obj_t *btn4 = lv_btn_create(cont);
  lv_obj_set_size(btn4, BTN_W, BTN_H);
  lv_obj_set_style_bg_color(btn4, lv_color_make(20, 100, 100), 0);
  lv_obj_set_style_bg_color(btn4, lv_color_make(30, 140, 140), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn4, 8, 0);
  lv_obj_t *lbl4 = lv_label_create(btn4);
  lv_label_set_text(lbl4, LV_SYMBOL_REFRESH "\nUpdate");
  lv_obj_set_style_text_align(lbl4, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lbl4, lv_color_make(180, 255, 255), 0);
  lv_obj_set_style_text_font(lbl4, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl4);
  lv_obj_add_event_cb(btn4, menu_ota_cb, LV_EVENT_CLICKED, NULL);

  // 5. Test Mode
  lv_obj_t *btn5 = lv_btn_create(cont);
  lv_obj_set_size(btn5, BTN_W, BTN_H);
  lv_obj_set_style_bg_color(btn5, lv_color_make(100, 30, 80), 0);
  lv_obj_set_style_bg_color(btn5, lv_color_make(140, 50, 110), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn5, 8, 0);
  lv_obj_t *lbl5 = lv_label_create(btn5);
  lv_label_set_text(lbl5, LV_SYMBOL_WARNING "\nTest Mode");
  lv_obj_set_style_text_align(lbl5, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lbl5, lv_color_make(255, 200, 220), 0);
  lv_obj_set_style_text_font(lbl5, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl5);
  lv_obj_add_event_cb(btn5, menu_test_cb, LV_EVENT_CLICKED, NULL);

  LOG_I("UI", "Menu screen created.");
}

