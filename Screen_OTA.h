// Screen_OTA.h
//
// OTA通知バナー・更新進捗画面
//
// April 2026
// Tetsu Nishimura

#ifndef SCREEN_OTA_H
#define SCREEN_OTA_H

#include "Globals.h"

// 新ファームウェア検出時に呼ぶ。
// 現在の画面上に「更新あり」バナーをオーバーレイ表示する。
// serverVersion: サーバにある新バージョン文字列
void otaShowNotifyBanner(const char* serverVersion);

// バナーを非表示にする (キャンセルボタン押下時などに呼ぶ)。
void otaHideNotifyBanner();

// OTA更新実行中の専用フルスクリーンを表示する。
// この関数を呼んだ後に otaStartUpdate() を呼ぶこと。
void otaShowProgressScreen(const char* serverVersion);
// OTA更新の進捗状況を画面のラベルに反映する。
void otaUpdateProgressLabel(int pct);

#endif // SCREEN_OTA_H
