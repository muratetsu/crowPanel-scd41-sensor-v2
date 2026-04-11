#ifndef SENSOR_CHART_H
#define SENSOR_CHART_H

#include "Globals.h"
#include <lvgl.h>

// チャートの初期化
// parent: チャートやカスタムグリッドを配置する親オブジェクト（スクリーン）
void SensorChart_Init(lv_obj_t *parent);

// チャートリソースのリセット（画面遷移時など）
void SensorChart_Reset();

// チャートモードの取得と設定 (0 = 4H, 1 = 1D)
int SensorChart_GetMode();
void SensorChart_SetMode(int mode);

// 1分ごとの新しいデータを受け取ってチャートを更新
void SensorChart_UpdateData(uint16_t co2, float temp, float humid);

// 現在の履歴データからチャート全体を再描画
void SensorChart_RefreshAll();

#endif // SENSOR_CHART_H
