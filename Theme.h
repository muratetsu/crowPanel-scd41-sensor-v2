#ifndef THEME_H
#define THEME_H

#include <lvgl.h>

// ============================================================
// UI テーマ定義ファイル
// 画面の背景色やフォントカラー、センサーデータの色などを一元管理します
// ============================================================

// --- Background Colors ---
#define THEME_BG_DARK       lv_color_make(10, 15, 35)   // Sensor画面背景
#define THEME_BG_MAIN       lv_color_make(15, 20, 40)   // Menu/設定画面背景
#define THEME_BG_LIGHT      lv_color_make(20, 25, 45)   // WiFi/OTA/チャート背景
#define THEME_BG_PANEL      lv_color_make(25, 30, 50)   // テスト用パネル背景
#define THEME_BG_BTN        lv_color_make(30, 35, 50)   // 汎用ボタンスパン背景

// --- Text Colors ---
#define THEME_TEXT_WHITE    lv_color_make(255, 255, 255)
#define THEME_TEXT_LIGHT    lv_color_make(220, 230, 240)
#define THEME_TEXT_MUTED    lv_color_make(150, 160, 180) // 補足テキスト用
#define THEME_TEXT_DARK     lv_color_make(100, 100, 100) // 無効状態など
#define THEME_TEXT_TITLE    lv_color_make(120, 180, 255) // 各画面タイトル用

// --- Sensor Data Colors ---
#define THEME_COLOR_CO2     lv_color_make(150, 255, 150) // 緑
#define THEME_COLOR_TEMP    lv_color_make(255, 150, 150) // 赤/ピンク
#define THEME_COLOR_HUMID   lv_color_make(150, 150, 255) // 青

// --- Status Colors ---
#define THEME_COLOR_WARNING lv_color_make(255, 180, 50)  // OTA通知など
#define THEME_COLOR_ERROR   lv_color_make(220, 80, 80)
#define THEME_COLOR_GOOD    lv_color_make(80, 220, 80)
#define THEME_COLOR_OK      lv_color_make(220, 200, 60)

// --- Borders and Separators ---
#define THEME_BORDER_DARK   lv_color_make(60, 70, 90)    // チャート枠・グリッド
#define THEME_BORDER_LIGHT  lv_color_make(60, 80, 130)   // メニュー区切り線など

#endif // THEME_H
