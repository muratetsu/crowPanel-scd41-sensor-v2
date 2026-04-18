#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <time.h>

// ============================================================
// テスト／デバッグ用フラグ
// ============================================================
// 下記のコメントアウトを外すと、SCD41のダミーデータを使用し、
// チャート更新などのUIと履歴ロジックのテストを素早く行うことができます。
// #define USE_DUMMY_SENSOR

namespace SensorManager {

    // センサーの初期化処理
    void init();
    
    // センサーデータの取得
    // 戻り値:
    //   1 : 取得成功（co2, temperature, humidityに値がセットされる）
    //   0 : データ未準備（エラーではないが、まだ取得タイミングではない）
    //  -1 : 取得エラー（通信エラー等）
    int readData(uint16_t &co2, float &temperature, float &humidity);

    // 過去ログ集計などのトリガー時間をチェックする
    // 本番環境時は1分ごとの変化をNTP時刻から検出し、
    // テスト時はテストを素早く回すため短時間（毎秒）でtrueを返すように切り替えます。
    bool isAggregationTime(struct tm* timeinfo, bool gotTime);

    // テスト画面・デバッグ用
    uint16_t getLastError();
    uint16_t getAscStatus();
}

#endif // SENSOR_MANAGER_H
