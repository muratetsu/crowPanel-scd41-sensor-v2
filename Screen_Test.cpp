#include "Screen_Test.h"
#include "Logger.h"
#include <WiFi.h>
#include <esp_system.h>
#include "SensorManager.h"

static lv_obj_t *lbl_heap;
static lv_obj_t *lbl_rssi;
static lv_obj_t *lbl_uptime;
static lv_obj_t *lbl_sdk;
static lv_obj_t *lbl_ntp;
static lv_obj_t *lbl_i2c;
static lv_obj_t *lbl_asc;
static lv_timer_t *test_timer = NULL;

static void btn_back_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (test_timer != NULL) {
      lv_timer_del(test_timer);
      test_timer = NULL;
    }
    showMenuScreen();
  }
}

static void btn_clear_nvs_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    LOG_I("Test", "Clearing WiFi Credentials via NVS...");
    WiFi.disconnect(true, true);
    
    prefs.begin("wifi_cfg", false);
    prefs.clear();
    prefs.end();
    
    LOG_I("Test", "Credentials cleared. Reboot to take effect.");
    
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    lv_label_set_text(lbl, "Cleared! Rebooting...");
    
    // Reboot after short delay using LVGL timer or async
    delay(1000); 
    ESP.restart();
  }
}

static void test_timer_cb(lv_timer_t *timer) {
  if (!lbl_heap || !lbl_rssi || !lbl_uptime) return;

  // Free Heap
  String heapStr = "Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB";
  lv_label_set_text(lbl_heap, heapStr.c_str());

  // WiFi RSSI
  String rssiStr = "WiFi RSSI: ";
  if (WiFi.status() == WL_CONNECTED) {
    rssiStr += String(WiFi.RSSI()) + " dBm";
  } else {
    rssiStr += "Disconnected";
  }
  lv_label_set_text(lbl_rssi, rssiStr.c_str());

  // Uptime
  uint32_t ms = millis();
  uint32_t s = ms / 1000;
  uint32_t m = s / 60;
  uint32_t h = m / 60;
  String uptimeStr = "Uptime: " + String(h) + "h " + String(m % 60) + "m " + String(s % 60) + "s";
  lv_label_set_text(lbl_uptime, uptimeStr.c_str());

  // NTP Status
  if (lbl_ntp) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0) && timeinfo.tm_year > (2020 - 1900)) {
      char timeStringBuff[32];
      strftime(timeStringBuff, sizeof(timeStringBuff), "NTP: Synced (%m/%d %H:%M)", &timeinfo);
      lv_label_set_text(lbl_ntp, timeStringBuff);
    } else {
      lv_label_set_text(lbl_ntp, "NTP: Not Synced");
    }
  }

  // I2C / ASC Status
  if (lbl_i2c && lbl_asc) {
    uint16_t err = SensorManager::getLastError();
    if (err == 0) {
      lv_label_set_text(lbl_i2c, "I2C Error: 0 (OK)");
    } else {
      lv_label_set_text_fmt(lbl_i2c, "I2C Error: 0x%04X", err);
    }
    
    uint16_t asc = SensorManager::getAscStatus();
    lv_label_set_text_fmt(lbl_asc, "ASC (Auto Calib): %s", (asc ? "Enabled" : "Disabled"));
  }
}

void createTestUI(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, THEME_BG_MAIN, 0);

  // --- Title & Back Button ---
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Test / Debug Mode");
  lv_obj_set_style_text_color(title, THEME_TEXT_TITLE, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *btn_back = lv_btn_create(scr);
  lv_obj_set_size(btn_back, 60, 30);
  lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 5, 5);
  lv_obj_set_style_bg_color(btn_back, THEME_BORDER_LIGHT, 0);
  lv_obj_set_style_bg_color(btn_back, lv_color_make(80, 100, 160), LV_STATE_PRESSED);
  lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_back = lv_label_create(btn_back);
  lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Back");
  lv_obj_center(lbl_back);

  lv_obj_t *sep = lv_obj_create(scr);
  lv_obj_set_size(sep, screenWidth - 40, 1);
  lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_style_bg_color(sep, THEME_BORDER_LIGHT, 0);
  lv_obj_set_style_border_width(sep, 0, 0);

  // --- System Info Panel ---
  lv_obj_t *panel = lv_obj_create(scr);
  lv_obj_set_size(panel, screenWidth - 20, 180);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 45);
  lv_obj_set_style_bg_color(panel, THEME_BG_PANEL, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, THEME_BORDER_LIGHT, 0);
  // Optional: flex layout
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(panel, 10, 0);
  lv_obj_set_style_pad_row(panel, 5, 0);

  // Labels for status
  lbl_heap = lv_label_create(panel);
  lv_obj_set_style_text_color(lbl_heap, lv_color_make(220, 220, 220), 0);
  lv_label_set_text(lbl_heap, "Free Heap: ---");

  lbl_rssi = lv_label_create(panel);
  lv_obj_set_style_text_color(lbl_rssi, lv_color_make(220, 220, 220), 0);
  lv_label_set_text(lbl_rssi, "WiFi RSSI: ---");

  lbl_ntp = lv_label_create(panel);
  lv_obj_set_style_text_color(lbl_ntp, lv_color_make(220, 220, 220), 0);
  lv_label_set_text(lbl_ntp, "NTP: ---");

  lbl_i2c = lv_label_create(panel);
  lv_obj_set_style_text_color(lbl_i2c, lv_color_make(220, 220, 220), 0);
  lv_label_set_text(lbl_i2c, "I2C Error: ---");

  lbl_asc = lv_label_create(panel);
  lv_obj_set_style_text_color(lbl_asc, lv_color_make(220, 220, 220), 0);
  lv_label_set_text(lbl_asc, "ASC: ---");

  lbl_uptime = lv_label_create(panel);
  lv_obj_set_style_text_color(lbl_uptime, lv_color_make(220, 220, 220), 0);
  lv_label_set_text(lbl_uptime, "Uptime: ---");

  lbl_sdk = lv_label_create(panel);
  lv_obj_set_style_text_color(lbl_sdk, lv_color_make(150, 150, 150), 0);
  lv_label_set_text_fmt(lbl_sdk, "SDK: %s", ESP.getSdkVersion());

  // --- Clear NVS Button ---
  lv_obj_t *btn_clear = lv_btn_create(scr);
  lv_obj_set_size(btn_clear, screenWidth - 60, 40);
  lv_obj_align(btn_clear, LV_ALIGN_TOP_MID, 0, 235);
  lv_obj_set_style_bg_color(btn_clear, lv_color_make(180, 40, 40), 0);
  lv_obj_set_style_bg_color(btn_clear, lv_color_make(220, 60, 60), LV_STATE_PRESSED);
  lv_obj_add_event_cb(btn_clear, btn_clear_nvs_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *lbl_clear = lv_label_create(btn_clear);
  lv_label_set_text(lbl_clear, LV_SYMBOL_TRASH "  Clear WiFi Creds (NVS)");
  lv_obj_center(lbl_clear);

  // Initialize values immediately
  test_timer_cb(NULL);

  // Create timer
  test_timer = lv_timer_create(test_timer_cb, 1000, NULL);
  if (test_timer == NULL) {
    LOG_E("Test", "Failed to create timer for Test UI");
  }

  LOG_I("UI", "Test screen created.");
}
