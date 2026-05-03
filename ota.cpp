// ota.cpp
//
// OTA Firmware Update for crowPanel_scd41_sensor_v2
//
// April 2026
// Tetsu Nishimura
//
// 実装方針:
//   [定期チェックフェーズ]
//     - otaLoop() が OTA_CHECK_INTERVAL_MS ごとに getFirmwareVersion() を呼ぶ。
//     - WiFi未接続時はスキップ（次のインターバルで再試行）。
//     - サーバ版 != ローカル版 の場合、NotifyCallback でUIに通知。
//
//   [更新実行フェーズ]
//     - UIからユーザ確認後、otaStartUpdate() が呼ばれる。
//     - NVS に "update_pending" フラグを立ててから ESP.restart()。
//     - 次回起動の otaInit() でフラグを検出し、即座に updateFirmware() を実行。
//     - 成功・失敗いずれの場合も NVS フラグをリセットして再起動。

#include "ota.h"
#include "Logger.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include "Screen_OTA.h"
#include <lvgl.h>

// ============================================================
// 内部定数・変数
// ============================================================

// NVS キー
static const char* NVS_NS          = "ota";          // 名前空間
static const char* NVS_KEY_PENDING = "pending";      // 更新待ちフラグ (uint8)
static const char* NVS_KEY_VERSION = "server_ver";   // 更新対象バージョン文字列

// ローカルにビルド埋め込みするバージョン文字列。
// ビルド時に Arduino IDE の「ツール > ボードメニュー」などから渡すか、
// 手動でリリースごとに更新することを想定。
#ifndef OTA_LOCAL_VERSION
  #define OTA_LOCAL_VERSION  "v0.0.5"
#endif

static OtaNotifyCallback  s_notify_cb       = nullptr;
static uint32_t           s_lastCheckMs     = 0;
static bool               s_updateAvailable = false;
static char               s_serverVersion[OTA_VERSION_MAX_LEN] = {0};

static bool               s_startUpdateRequested = false;
static uint32_t           s_updateReqMs          = 0;

extern Preferences prefs;  // .ino で定義されたグローバルオブジェクトを共有

// ============================================================
// 内部ヘルパー: NVS アクセス
// ============================================================

static void nvsSetPending(bool pending, const char* serverVer)
{
    prefs.begin(NVS_NS, false);
    prefs.putUChar(NVS_KEY_PENDING, pending ? 1 : 0);
    if (pending && serverVer) {
        prefs.putString(NVS_KEY_VERSION, serverVer);
    }
    prefs.end();
}

static bool nvsGetPending(char* serverVerOut)
{
    prefs.begin(NVS_NS, true);
    uint8_t pending = prefs.getUChar(NVS_KEY_PENDING, 0);
    if (serverVerOut) {
        String s = prefs.getString(NVS_KEY_VERSION, "");
        strncpy(serverVerOut, s.c_str(), OTA_VERSION_MAX_LEN - 1);
        serverVerOut[OTA_VERSION_MAX_LEN - 1] = '\0';
    }
    prefs.end();
    return (pending == 1);
}

static void nvsClearPending()
{
    prefs.begin(NVS_NS, false);
    prefs.putUChar(NVS_KEY_PENDING, 0);
    prefs.end();
}

// ============================================================
// 内部ヘルパー: サーバからバージョン文字列を取得
// ============================================================

// 成功時: サーバのバージョン文字列を buf に格納して true を返す。
// 失敗時: false を返す (buf は変更しない)。
static bool fetchServerVersion(char* buf, size_t bufLen)
{
    // HTTPS で info.txt を取得する。
    // 証明書検証はスキップ（フィンガープリントの固定が困難なため）。
    // info.txt は公開のバージョン文字列のみなのでリスクは低い。
    WiFiClientSecure secureClient;
    secureClient.setInsecure();  // 証明書検証なし (MitMリスクあり、許容範囲内)
    HTTPClient http;

    if (!http.begin(secureClient, OTA_URL_INFO)) {
        LOG_E("OTA", "http.begin() failed for URL: %s", OTA_URL_INFO);
        return false;
    }

    int httpCode = http.GET();
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        payload.trim();  // 末尾の改行・スペースを除去
        if (payload.length() > 0 && payload.length() < bufLen) {
            strncpy(buf, payload.c_str(), bufLen - 1);
            buf[bufLen - 1] = '\0';
            success = true;
            LOG_I("OTA", "Server version: %s", buf);
        } else {
            LOG_E("OTA", "Server version string invalid (len=%d)", payload.length());
        }
    } else {
        LOG_E("OTA", "HTTP GET failed, code: %d", httpCode);
    }

    http.end();
    return success;
}

// ============================================================
// 内部ヘルパー: HTTPUpdate コールバック
// ============================================================

static void onUpdateStarted() {
    LOG_I("OTA", "Firmware download started.");
}

static void onUpdateFinished() {
    LOG_I("OTA", "Firmware download finished.");
    // 更新完了 → NVS フラグをクリア (再起動後に通常動作へ戻す)
    nvsClearPending();
}

static void onUpdateProgress(int cur, int total) {
    if (total > 0) {
        int pct = (int)((long)cur * 100 / total);
        
        // 1. シリアル出力はログが埋もれないように10%ごとに行う
        static int lastLogPct = -1;
        if (pct != lastLogPct && pct % 10 == 0) {
            LOG_I("OTA", "Progress: %d%% (%d / %d bytes)", pct, cur, total);
            lastLogPct = pct;
        }

        // 2. 画面更新（LVGL）は通信を阻害しないよう間引きを行う
        // 1%進むのに時間がかかるため、200ms（約5FPS）間隔にして負荷を最小限に抑える
        static uint32_t lastDrawMs = 0;
        if (millis() - lastDrawMs > 200) {
            lastDrawMs = millis();
            
            // 画面に進捗%を表示する
            otaUpdateProgressLabel(pct);

            // LVGLのタスクを回して画面を再描画（これでスピナーが回ります）
            lv_timer_handler(); 
        }
    }
}

static void onUpdateError(int err) {
    LOG_E("OTA", "Update error code: %d", err);
}

// ============================================================
// 内部ヘルパー: ファームウェアダウンロード・書き込み
// ============================================================

// この関数は完了まで制御を返さない。
// 呼び出し前に LVGL タイマーを停止するか、専用画面へ切り替えること。
static void executeFirmwareUpdate()
{
    LOG_I("OTA", "Starting firmware update from: %s", OTA_URL_BIN);

    // WiFi 接続確認 (再接続待ち)
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 30000UL) {
            LOG_E("OTA", "WiFi not connected after 30s. Aborting update.");
            nvsClearPending();
            delay(2000);
            ESP.restart();
            return;
        }
        delay(500);
        LOG_D("OTA", "Waiting for WiFi...");
    }

    WiFiClientSecure secureClient;
    secureClient.setInsecure();  // 証明書検証なし (最小限のリスク: ファームウェアバイナリの中身は検証できない)

    httpUpdate.onStart(onUpdateStarted);
    httpUpdate.onEnd(onUpdateFinished);
    httpUpdate.onProgress(onUpdateProgress);
    httpUpdate.onError(onUpdateError);
    httpUpdate.rebootOnUpdate(false);  // 自前で再起動タイミングを制御する

    t_httpUpdate_return ret = httpUpdate.update(secureClient, OTA_URL_BIN);

    switch (ret) {
        case HTTP_UPDATE_OK:
            LOG_I("OTA", "Update OK. Rebooting...");
            // onUpdateFinished() 内で NVS はクリア済み
            delay(500);
            ESP.restart();
            break;

        case HTTP_UPDATE_FAILED:
            LOG_E("OTA", "Update FAILED: (%d) %s",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
            // 失敗時も NVS をリセットしてループを抜ける (次回起動は通常動作)
            nvsClearPending();
            delay(3000);
            ESP.restart();
            break;

        case HTTP_UPDATE_NO_UPDATES:
            LOG_I("OTA", "No update available (server returned 304).");
            nvsClearPending();
            delay(2000);
            ESP.restart();
            break;
    }
}

// ============================================================
// 内部ヘルパー: 定期バージョンチェック
// ============================================================

static void checkForUpdate()
{
    if (WiFi.status() != WL_CONNECTED) {
        LOG_D("OTA", "WiFi not connected. Skipping check.");
        return;
    }

    LOG_I("OTA", "Checking for firmware update...");
    LOG_D("OTA", "Local version: %s", OTA_LOCAL_VERSION);

    char serverVer[OTA_VERSION_MAX_LEN] = {0};
    if (!fetchServerVersion(serverVer, sizeof(serverVer))) {
        LOG_E("OTA", "Failed to fetch server version. Will retry next interval.");
        return;
    }

    if (strcmp(serverVer, OTA_LOCAL_VERSION) == 0) {
        LOG_I("OTA", "Firmware is up to date.");
        s_updateAvailable = false;
        return;
    }

    // 新バージョン検出
    strncpy(s_serverVersion, serverVer, OTA_VERSION_MAX_LEN - 1);
    s_updateAvailable = true;
    LOG_I("OTA", "New firmware available: %s (local: %s)", serverVer, OTA_LOCAL_VERSION);

    if (s_notify_cb) {
        s_notify_cb(serverVer);
    }
}

// ============================================================
// 公開API の実装
// ============================================================

void otaInit(OtaNotifyCallback notify_cb)
{
    s_notify_cb   = notify_cb;
    s_lastCheckMs = 0;  // 起動直後は即チェックさせない (WiFi接続完了を待つ)
    s_updateAvailable = false;
    memset(s_serverVersion, 0, sizeof(s_serverVersion));

    // NVS に "更新保留" フラグがあれば、更新実行フェーズ
    char pendingVer[OTA_VERSION_MAX_LEN] = {0};
    if (nvsGetPending(pendingVer)) {
        LOG_I("OTA", "Update pending flag found. Starting firmware update...");
        strncpy(s_serverVersion, pendingVer, OTA_VERSION_MAX_LEN - 1);
        // executeFirmwareUpdate() は完了後に ESP.restart() するため、ここから先は実行されない
        executeFirmwareUpdate();
        // ここには到達しない
    }

    LOG_I("OTA", "OTA initialized. Local version: %s", OTA_LOCAL_VERSION);
}

void otaLoop()
{
    // 更新実行フェーズ中は otaInit() で止まるので通常ここには来ない。
    // 念のため、pending フラグが残っている場合は何もしない。
    // ただし、otaStartUpdate() でフラグが立った直後は、画面描画を待ってから実行する。
    char pendingVer[OTA_VERSION_MAX_LEN];
    if (nvsGetPending(pendingVer)) {
        if (s_startUpdateRequested) {
            // UI描画を確実に行わせるため、少し待機してからブロック処理に入る
            if (millis() - s_updateReqMs > 200) {
                s_startUpdateRequested = false;
                executeFirmwareUpdate();
            }
        }
        return;
    }

    // 定期チェック
    uint32_t now = millis();
    // s_lastCheckMs == 0 のときは WiFi 接続完了後に外部から
    // otaScheduleCheck() で起動させるため、ここでは何もしない。
    if (s_lastCheckMs == 0) return;

    if (now - s_lastCheckMs >= OTA_CHECK_INTERVAL_MS) {
        s_lastCheckMs = now;
        checkForUpdate();
    }
}

// WiFi 接続完了時に .ino から呼ぶ。最初のチェックをスケジュールする。
// (宣言は Globals.h に追加する)
void otaScheduleFirstCheck()
{
    // タイマをリセット。次の otaLoop() 呼び出しから OTA_CHECK_INTERVAL_MS 後に初回チェック。
    // ただし、起動直後はすぐチェックしたい場合は以下を変更:
    //   s_lastCheckMs = millis() - OTA_CHECK_INTERVAL_MS;  // 即チェック
    s_lastCheckMs = millis();
    LOG_I("OTA", "OTA check scheduled (interval: %lu ms).", OTA_CHECK_INTERVAL_MS);
}

void otaCheckNow()
{
    // メニューの「手動チェック」ボタンから呼ばれる。
    // HTTP通信が発生するためタッチ応答が数秒遅くなる点に注意。
    LOG_I("OTA", "Manual check requested.");
    checkForUpdate();
    // 次の定期チェックタイマもリセットしておく (直後に二重チェックしないよう)
    s_lastCheckMs = millis();
}

void otaStartUpdate()
{
    LOG_I("OTA", "User confirmed update. Starting firmware download...");

    // NVS に pending フラグを立てておく。
    // これはダウンロード中に電源断が起きた場合の起動時リカバリ用。
    // (次回起動の otaInit() で検出し、executeFirmwareUpdate() を再実行する)
    nvsSetPending(true, s_serverVersion);

    // UI(LVGL)の描画ループに制御を一旦戻し、進捗画面を確実に描画させるため、
    // ここでは直接 executeFirmwareUpdate() を呼ばずにフラグを立てる。
    // 実際のダウンロード処理は otaLoop() 内で遅延実行される。
    s_startUpdateRequested = true;
    s_updateReqMs = millis();
}

const char* otaGetLocalVersion()
{
    return OTA_LOCAL_VERSION;
}

const char* otaGetServerVersion()
{
    return s_serverVersion;
}

bool otaIsUpdateAvailable()
{
    return s_updateAvailable;
}
