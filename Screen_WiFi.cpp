#include "Screen_WiFi.h"
#include <WiFi.h>

static lv_obj_t *ta_ssid      = NULL;
static lv_obj_t *ta_pass      = NULL;
static lv_obj_t *kb           = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *scan_modal   = NULL;
static bool      scanRequested = false;

// 前方宣言
static void closeScanModal();

// エラーラベル変更用
void setWiFiErrorLabel(const char *msg) {
  if (label_status) {
    lv_label_set_text(label_status, msg);
    lv_obj_set_style_text_color(label_status, lv_color_make(255, 80, 80), 0);
  }
}

// GUIフィールドのリセット (表示切り替え時に呼ばれる)
void resetWiFiUI_Fields() {
  ta_ssid = ta_pass = kb = label_status = scan_modal = NULL;
  scanRequested = false;
}

static void ta_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    if (!kb) return;
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
  }
}

static void kb_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (kb) lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}

static void btn_connect_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  const char *ssid = lv_textarea_get_text(ta_ssid);
  const char *pass = lv_textarea_get_text(ta_pass);

  if (strlen(ssid) == 0) {
    setWiFiErrorLabel("[!] Please enter the SSID");
    return;
  }

  if (kb) lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

  // NVSに保存
  prefs.begin("wifi_cfg", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  if (label_status) {
    lv_label_set_text(label_status, "Connecting...");
    lv_obj_set_style_text_color(label_status, lv_color_make(255, 220, 0), 0);
  }
  lv_timer_handler();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  wifiConnecting = true;
  wifiStartTime  = millis();
  bootConnecting = false; // 手動接続

  Serial.printf("[WiFi] Connecting to SSID: %s\n", ssid);
}


static void closeScanModal() {
  if (scan_modal) {
    lv_obj_del(scan_modal);
    scan_modal = NULL;
  }
}

static void scan_item_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (ta_ssid) lv_textarea_set_text(ta_ssid, (const char *)lv_event_get_user_data(e));
  closeScanModal();
  if (kb && ta_pass) {
    lv_keyboard_set_textarea(kb, ta_pass);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
  }
}

static void scan_close_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    closeScanModal();
    WiFi.scanDelete();
  }
}

static void buildScanModal(int16_t n) {
  lv_obj_t *scr = lv_scr_act();

  scan_modal = lv_obj_create(scr);
  lv_obj_set_size(scan_modal, screenWidth, screenHeight);
  lv_obj_set_pos(scan_modal, 0, 0);
  lv_obj_set_style_bg_color(scan_modal, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(scan_modal, LV_OPA_70, 0);
  lv_obj_set_style_border_width(scan_modal, 0, 0);
  lv_obj_clear_flag(scan_modal, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *dialog = lv_obj_create(scan_modal);
  lv_obj_set_size(dialog, screenWidth - 20, screenHeight - 20);
  lv_obj_center(dialog);
  lv_obj_set_style_bg_color(dialog, lv_color_make(18, 24, 48), 0);
  lv_obj_set_style_border_color(dialog, lv_color_make(60, 100, 200), 0);
  lv_obj_set_style_border_width(dialog, 2, 0);
  lv_obj_set_style_radius(dialog, 8, 0);
  lv_obj_set_style_pad_all(dialog, 6, 0);
  lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_row = lv_obj_create(dialog);
  lv_obj_set_size(title_row, lv_pct(100), 28);
  lv_obj_set_pos(title_row, 0, 0);
  lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(title_row, 0, 0);
  lv_obj_set_style_pad_all(title_row, 0, 0);
  lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_lbl = lv_label_create(title_row);
  lv_label_set_text_fmt(title_lbl, LV_SYMBOL_WIFI "  WiFi Networks (%d)", n < 0 ? 0 : n);
  lv_obj_set_style_text_color(title_lbl, lv_color_make(100, 180, 255), 0);
  lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *close_btn = lv_btn_create(title_row);
  lv_obj_set_size(close_btn, 26, 22);
  lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_make(180, 40, 40), 0);
  lv_obj_set_style_radius(close_btn, 4, 0);
  lv_obj_t *close_lbl = lv_label_create(close_btn);
  lv_label_set_text(close_lbl, LV_SYMBOL_CLOSE);
  lv_obj_center(close_lbl);
  lv_obj_add_event_cb(close_btn, scan_close_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *list_area = lv_obj_create(dialog);
  int list_h = screenHeight - 20 - 28 - 12;
  lv_obj_set_size(list_area, lv_pct(100), list_h);
  lv_obj_set_pos(list_area, 0, 32);
  lv_obj_set_style_bg_color(list_area, lv_color_make(12, 16, 36), 0);
  lv_obj_set_style_border_width(list_area, 0, 0);
  lv_obj_set_style_pad_all(list_area, 2, 0);
  lv_obj_set_flex_flow(list_area, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  if (n <= 0) {
    lv_obj_t *no_result = lv_label_create(list_area);
    lv_label_set_text(no_result, "No networks found.");
    lv_obj_set_style_text_color(no_result, lv_color_make(160, 160, 180), 0);
    lv_obj_center(no_result);
    return;
  }

  for (int i = 0; i < n; i++) {
    String ssid_str = WiFi.SSID(i);
    int32_t rssi    = WiFi.RSSI(i);
    bool    isEnc   = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    if (ssid_str.length() == 0) continue;

    char *ssid_copy = (char *)malloc(ssid_str.length() + 1);
    if (!ssid_copy) continue;
    ssid_str.toCharArray(ssid_copy, ssid_str.length() + 1);

    lv_obj_t *item_btn = lv_btn_create(list_area);
    lv_obj_set_width(item_btn, lv_pct(100));
    lv_obj_set_height(item_btn, 32);
    lv_obj_set_style_bg_color(item_btn, lv_color_make(30, 40, 75), 0);
    lv_obj_set_style_bg_color(item_btn, lv_color_make(50, 80, 160), LV_STATE_PRESSED);
    lv_obj_set_style_radius(item_btn, 4, 0);
    lv_obj_set_style_pad_left(item_btn, 6, 0);
    lv_obj_set_style_pad_right(item_btn, 4, 0);
    lv_obj_set_style_pad_top(item_btn, 0, 0);
    lv_obj_set_style_pad_bottom(item_btn, 0, 0);
    lv_obj_clear_flag(item_btn, LV_OBJ_FLAG_SCROLLABLE);

    const char *lock_sym = isEnc ? LV_SYMBOL_EYE_CLOSE " " : "";
    lv_obj_t *item_lbl = lv_label_create(item_btn);
    lv_label_set_text_fmt(item_lbl, "%s%s", lock_sym, ssid_copy);
    lv_obj_set_style_text_color(item_lbl, lv_color_make(220, 230, 255), 0);
    lv_obj_align(item_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *rssi_lbl = lv_label_create(item_btn);
    lv_label_set_text_fmt(rssi_lbl, "%ddBm", rssi);
    lv_obj_set_style_text_color(rssi_lbl,
      rssi >= -60 ? lv_color_make(80, 220, 80) :
      rssi >= -75 ? lv_color_make(220, 200, 60) :
                    lv_color_make(220, 80, 80), 0);
    lv_obj_set_style_text_font(rssi_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(rssi_lbl, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_add_event_cb(item_btn, scan_item_cb, LV_EVENT_CLICKED, (void *)ssid_copy);
  }
}

static void btn_scan_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (kb) lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  closeScanModal();
  if (label_status) {
    lv_label_set_text(label_status, LV_SYMBOL_REFRESH " Scanning...");
    lv_obj_set_style_text_color(label_status, lv_color_make(100, 200, 255), 0);
  }
  lv_timer_handler();
  WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
  scanRequested = true;
  Serial.println("[Scan] WiFi scan started (async).");
}

void checkScanStatus() {
  if (!scanRequested) return;
  
  if (currentScreen != SCREEN_WIFI) {
    scanRequested = false;
    return;
  }

  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scanRequested = false;

  if (n == WIFI_SCAN_FAILED) {
    setWiFiErrorLabel("[!] Scan failed.");
    Serial.println("[Scan] WiFi scan failed.");
    return;
  }

  Serial.printf("[Scan] Found %d network(s).\n", n);
  if (label_status) {
    lv_label_set_text(label_status, "Select a network below.");
    lv_obj_set_style_text_color(label_status, lv_color_make(150, 160, 180), 0);
  }
  buildScanModal(n);
}

void createWiFiUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_make(20, 25, 45), 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi Setup");
  lv_obj_set_style_text_color(title, lv_color_make(100, 180, 255), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 5, 7);

  lv_obj_t *btn_connect = lv_btn_create(scr);
  lv_obj_set_size(btn_connect, 80, 26);
  lv_obj_align(btn_connect, LV_ALIGN_TOP_RIGHT, -4, 2);
  lv_obj_set_style_bg_color(btn_connect, lv_color_make(30, 140, 255), 0);
  lv_obj_t *btn_connect_lbl = lv_label_create(btn_connect);
  lv_label_set_text(btn_connect_lbl, LV_SYMBOL_OK " Connect");
  lv_obj_center(btn_connect_lbl);
  lv_obj_add_event_cb(btn_connect, btn_connect_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btn_scan = lv_btn_create(scr);
  lv_obj_set_size(btn_scan, 66, 26);
  lv_obj_align_to(btn_scan, btn_connect, LV_ALIGN_OUT_LEFT_MID, -4, 0);
  lv_obj_set_style_bg_color(btn_scan, lv_color_make(20, 120, 80), 0);
  lv_obj_t *btn_scan_lbl = lv_label_create(btn_scan);
  lv_label_set_text(btn_scan_lbl, LV_SYMBOL_REFRESH " Scan");
  lv_obj_center(btn_scan_lbl);
  lv_obj_add_event_cb(btn_scan, btn_scan_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *line_top = lv_obj_create(scr);
  lv_obj_set_size(line_top, screenWidth, 2);
  lv_obj_set_pos(line_top, 0, 30);
  lv_obj_set_style_bg_color(line_top, lv_color_make(60, 80, 120), 0);
  lv_obj_set_style_border_width(line_top, 0, 0);
  lv_obj_clear_flag(line_top, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl_ssid = lv_label_create(scr);
  lv_label_set_text(lbl_ssid, "SSID:");
  lv_obj_set_style_text_color(lbl_ssid, lv_color_make(180, 200, 230), 0);
  lv_obj_align(lbl_ssid, LV_ALIGN_TOP_LEFT, 5, 38);

  ta_ssid = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta_ssid, true);
  lv_textarea_set_placeholder_text(ta_ssid, "Tap Scan or type SSID");
  lv_obj_set_size(ta_ssid, 253, 28);
  lv_obj_align(ta_ssid, LV_ALIGN_TOP_RIGHT, -4, 32);
  lv_obj_set_style_bg_color(ta_ssid, lv_color_make(40, 50, 80), 0);
  lv_obj_set_style_text_color(ta_ssid, lv_color_make(230, 230, 230), 0);
  lv_obj_set_style_border_color(ta_ssid, lv_color_make(80, 120, 200), 0);
  lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *lbl_pass = lv_label_create(scr);
  lv_label_set_text(lbl_pass, "PASS:");
  lv_obj_set_style_text_color(lbl_pass, lv_color_make(180, 200, 230), 0);
  lv_obj_align(lbl_pass, LV_ALIGN_TOP_LEFT, 5, 70);

  ta_pass = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta_pass, true);
  lv_textarea_set_password_mode(ta_pass, true);
  lv_textarea_set_placeholder_text(ta_pass, "Enter Password");
  lv_obj_set_size(ta_pass, 253, 28);
  lv_obj_align(ta_pass, LV_ALIGN_TOP_RIGHT, -4, 64);
  lv_obj_set_style_bg_color(ta_pass, lv_color_make(40, 50, 80), 0);
  lv_obj_set_style_text_color(ta_pass, lv_color_make(230, 230, 230), 0);
  lv_obj_set_style_border_color(ta_pass, lv_color_make(80, 120, 200), 0);
  lv_obj_add_event_cb(ta_pass, ta_event_cb, LV_EVENT_ALL, NULL);

  label_status = lv_label_create(scr);
  lv_label_set_long_mode(label_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_status, screenWidth - 10);
  lv_label_set_text(label_status, "Press Scan to find networks, or type SSID manually.");
  lv_obj_set_style_text_color(label_status, lv_color_make(150, 160, 180), 0);
  lv_obj_align(label_status, LV_ALIGN_TOP_LEFT, 5, 98);

  kb = lv_keyboard_create(scr);
  lv_obj_set_size(kb, screenWidth, screenHeight - 100);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, ta_ssid);
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

  Serial.println("[UI] WiFi setup screen created.");

  prefs.begin("wifi_cfg", true);
  String s = prefs.getString("ssid", "");
  String p = prefs.getString("pass", "");
  prefs.end();
  if (s.length() > 0 && ta_ssid && ta_pass) {
    lv_textarea_set_text(ta_ssid, s.c_str());
    lv_textarea_set_text(ta_pass, p.c_str());
  }
}
