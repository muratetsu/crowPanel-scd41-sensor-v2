// ota.h
//
// OTA Firmware Update for crowPanel_scd41_sensor_v2
//
// April 2026
// Tetsu Nishimura
//
// 概要:
//   - WiFi接続後、定期的にサーバのファームウェアバージョンをチェックする。
//   - 新しいバージョンを検出したらユーザに通知する (NotifyCallback)。
//   - ユーザが更新を承認したら、専用OTA画面を表示してダウンロード・更新を行う。
//   - 成功・失敗いずれの場合もNVSフラグを正しくリセットしてから再起動する。

#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

// ============================================================
// サーバ設定 (URLはプライベート設定ファイルに分離)
// ============================================================
// サーバURLは ota_server_config.h に記載してください。
// そのファイルは .gitignore で除外されています。
// 初回セットアップ時: ota_server_config.h.example をコピーして発展してください。
#include "ota_server_config.h"

// info.txt フォーマット: ファームウェアのバージョン文字列 (英数字、≤31文字)
// 例: "v1.2.3"  または  "20260421"
#define OTA_VERSION_MAX_LEN  32

// ============================================================
// 定期チェック間隔
// ============================================================
#define OTA_CHECK_INTERVAL_MS  (6UL * 3600UL * 1000UL)  // 6時間ごと

// ============================================================
// 新バージョン検出時に呼ばれるコールバック型
// server_version: サーバにある新バージョン文字列
// ============================================================
typedef void (*OtaNotifyCallback)(const char *server_version);

// ============================================================
// 公開API
// ============================================================

// 初期化。setup()で一度だけ呼ぶ。
// notify_cb: 新バージョン検出時に呼ばれるコールバック (UIにバナーを表示するなど)
void otaInit(OtaNotifyCallback notify_cb);

// loop() から毎サイクル呼ぶ。
// 定期チェックタイミングの管理を行う。
void otaLoop();

// WiFi接続完了時に .ino から呼ぶ。最初の定期チェックをスケジュールする。
void otaScheduleFirstCheck();

// サーバへの即時チェックを実行する (メニューの手動ボタンから呼ぶ)。
// WiFi未接続の場合は何もしない。
// 新バージョンが見つかれば otaInit() に渡した notify_cb が呼ばれる。
void otaCheckNow();

// OTA更新を開始する。ユーザが確認ボタンを押したときにUIから呼ぶ。
// この関数は完了まで制御を返さない (LVGL描画は停止)。
// 完了後は自動で ESP.restart() する。
void otaStartUpdate();

// 現在のローカルファームウェアバージョン文字列を返す。
const char* otaGetLocalVersion();

// 最後に検出したサーババージョン文字列を返す。
const char* otaGetServerVersion();

// 新バージョンが検出済みか (ユーザ通知待ち状態か) を返す。
bool otaIsUpdateAvailable();

#endif // OTA_H
