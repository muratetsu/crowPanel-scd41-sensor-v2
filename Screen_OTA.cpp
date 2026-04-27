// Screen_OTA.cpp
//
// OTA通知バナー・更新進捗画面
//
// April 2026
// Tetsu Nishimura
//
// UI動作フロー:
//
//   [バナー表示]
//     otaShowNotifyBanner(ver)
//       → 画面下部にスライドアップするバナーをオーバーレイ
//         ┌─────────────────────────────┐
//         │ 🔔 New firmware: v1.2.3     │
//         │  [Update Now]   [Later]     │
//         └─────────────────────────────┘
//     "Update Now" → otaShowProgressScreen() → otaStartUpdate()
//                    → executeFirmwareUpdate() → 書き込み完了 → ESP.restart()
//     "Later"      → バナーを閉じる (次回チェックまで通知なし)
//
//   [進捗画面]
//     otaShowProgressScreen(ver)
//       → フルスクリーンの進捗画面を表示したまま
//       → otaStartUpdate() → executeFirmwareUpdate() がダウンロード・書き込みを実行。
//       → 完了後に ESP.restart() するためスピナーが止まることはない。
//          LVGLのタイマーループは停止するが画面は維持される。

#include "Screen_OTA.h"
#include "ota.h"
#include "Logger.h"
#include <lvgl.h>

// ============================================================
// バナー関連
// ============================================================

static lv_obj_t* s_banner = nullptr;

static void btn_update_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    // ユーザが「今すぐ更新」を選択
    const char* serverVer = (const char*)lv_event_get_user_data(e);
    LOG_I("OTA", "User confirmed update.");

    // 進捗画面に切り替え、描画を確定させてからダウンロード開始
    otaShowProgressScreen(serverVer);
    lv_timer_handler();  // 進捗画面を描画に反映

    // WiFi 接続済みのまま直接ダウンロード・書き込みを開始する。
    // executeFirmwareUpdate() は完了後に ESP.restart() するため、この関数から制御は戻らない。
    // 進捗画面は restart() まで表示されたままになる。
    otaStartUpdate();
}

static void btn_later_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    LOG_I("OTA", "User deferred update.");
    otaHideNotifyBanner();
}

void otaShowNotifyBanner(const char* serverVersion)
{
    // 既に表示中なら再生成しない
    if (s_banner != nullptr) return;

    lv_obj_t* scr = lv_scr_act();

    // バナー本体 (画面下部に固定)
    s_banner = lv_obj_create(scr);
    lv_obj_set_size(s_banner, screenWidth, 80);
    lv_obj_align(s_banner, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_banner, lv_color_make(20, 60, 120), 0);
    lv_obj_set_style_bg_opa(s_banner, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_banner, 0, 0);
    lv_obj_set_style_radius(s_banner, 0, 0);
    lv_obj_set_style_pad_all(s_banner, 8, 0);
    lv_obj_clear_flag(s_banner, LV_OBJ_FLAG_SCROLLABLE);

    // メッセージラベル
    char msg[64];
    snprintf(msg, sizeof(msg), LV_SYMBOL_DOWNLOAD "  New firmware: %s", serverVersion);
    lv_obj_t* lbl = lv_label_create(s_banner);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_color(lbl, lv_color_make(200, 230, 255), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 2);

    // 「今すぐ更新」ボタン
    // ユーザデータとしてバージョン文字列ポインタを渡す
    // (serverVersionはota.cppのstatic変数s_serverVersionなので寿命は問題なし)
    lv_obj_t* btn_now = lv_btn_create(s_banner);
    lv_obj_set_size(btn_now, (screenWidth / 2) - 20, 34);
    lv_obj_align(btn_now, LV_ALIGN_BOTTOM_LEFT, 4, 0);
    lv_obj_set_style_bg_color(btn_now, lv_color_make(30, 140, 80), 0);
    lv_obj_set_style_bg_color(btn_now, lv_color_make(50, 180, 110), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_now, 8, 0);
    lv_obj_add_event_cb(btn_now, btn_update_cb, LV_EVENT_CLICKED, (void*)serverVersion);
    lv_obj_t* lbl_now = lv_label_create(btn_now);
    lv_label_set_text(lbl_now, LV_SYMBOL_OK "  Update Now");
    lv_obj_set_style_text_font(lbl_now, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_now);

    // 「後で」ボタン
    lv_obj_t* btn_later = lv_btn_create(s_banner);
    lv_obj_set_size(btn_later, (screenWidth / 2) - 20, 34);
    lv_obj_align(btn_later, LV_ALIGN_BOTTOM_RIGHT, -4, 0);
    lv_obj_set_style_bg_color(btn_later, lv_color_make(70, 70, 90), 0);
    lv_obj_set_style_bg_color(btn_later, lv_color_make(100, 100, 120), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_later, 8, 0);
    lv_obj_add_event_cb(btn_later, btn_later_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_later = lv_label_create(btn_later);
    lv_label_set_text(lbl_later, LV_SYMBOL_CLOSE "  Later");
    lv_obj_set_style_text_font(lbl_later, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_later);

    LOG_I("OTA", "Notify banner shown (server: %s).", serverVersion);
}

void otaHideNotifyBanner()
{
    if (s_banner == nullptr) return;
    lv_obj_del(s_banner);
    s_banner = nullptr;
    LOG_D("OTA", "Notify banner hidden.");
}

// ============================================================
// 進捗画面 (フルスクリーン)
// ============================================================

void otaShowProgressScreen(const char* serverVersion)
{
    // バナーが出ていれば閉じる
    if (s_banner) {
        lv_obj_del(s_banner);
        s_banner = nullptr;
    }

    // 新しいフルスクリーンを作成して即時切替
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_make(10, 15, 35), 0);

    // タイトル
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, LV_SYMBOL_DOWNLOAD "  Firmware Update");
    lv_obj_set_style_text_color(title, lv_color_make(100, 180, 255), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // バージョン表示
    char verMsg[64];
    snprintf(verMsg, sizeof(verMsg), "%s  →  %s",
             otaGetLocalVersion(), serverVersion);
    lv_obj_t* lbl_ver = lv_label_create(scr);
    lv_label_set_text(lbl_ver, verMsg);
    lv_obj_set_style_text_color(lbl_ver, lv_color_make(180, 200, 220), 0);
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_ver, LV_ALIGN_TOP_MID, 0, 55);

    // 区切り線
    lv_obj_t* sep = lv_obj_create(scr);
    lv_obj_set_size(sep, screenWidth - 40, 1);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_style_bg_color(sep, lv_color_make(50, 70, 120), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    // ステータスラベル
    lv_obj_t* lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, "Downloading firmware...\nDo not power off.");
    lv_obj_set_style_text_color(lbl_status, lv_color_make(255, 210, 80), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, -20);

    // スピナー (ダウンロード中を視覚的に示す)
    lv_obj_t* spinner = lv_spinner_create(scr, 1500, 60);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 50);
    lv_obj_set_style_arc_color(spinner, lv_color_make(50, 140, 255), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_make(30, 40, 70), LV_PART_MAIN);

    // 警告ラベル
    lv_obj_t* lbl_warn = lv_label_create(scr);
    lv_label_set_text(lbl_warn, LV_SYMBOL_WARNING "  Device will reboot after update.");
    lv_obj_set_style_text_color(lbl_warn, lv_color_make(160, 160, 160), 0);
    lv_obj_set_style_text_font(lbl_warn, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_warn, LV_ALIGN_BOTTOM_MID, 0, -15);

    // 画面をロードして、LVGLに描画させる
    lv_scr_load(scr);
    lv_timer_handler();

    LOG_I("OTA", "Progress screen displayed.");
}
