# CrowPanel SCD41 Sensor Dashboard (v2)

ESP32を搭載したCrowPanelのLCD上にLVGLのUIを展開し、SCD41 CO2センサーによる空気質（CO2・温度・湿度）のリアルタイム監視とグラフ表示を提供するスマートモニターの実装です。
Wi-Fi設定、環境センサーのダッシュボード、SDカードへのデータロギング、そしてメニュー画面といった複数画面機能を含みます。

## 概要

CrowPanelのタッチスクリーンとLVGL（Light and Versatile Graphics Library）を活用したセンサー監視ダッシュボードです。
デバイス単体で周辺のWi-Fiアクセスポイントをスキャンしてインターネット経由で時刻同期を行い、Sensirion SCD41 CO2センサーの計測データをリアルタイムに画面へプロットします。また、SDカードを用いたCSVデータのバックグラウンド記録により、過去のトレンド（4時間/24時間）の切り替え表示を可能にしています。設定した認証情報はESP32のNVSに保存され、次回起動時は自動接続・復帰します。

## 画面構成と機能

4つのメイン画面（Screen）で構成されています。

### 1. Wi-Fi設定画面 (`Screen_WiFi`)
- **Wi-Fiスキャン機能**: `[Scan]`ボタンをタップすると周辺のアクセスポイントを検索し、リストから接続したいSSIDを選択できます。
- **ソフトウェアキーボード**: スクリーンのキーボードで直感的にパスワードを入力可能です。
- `[Connect]`ボタンで接続を開始し、成功時はセンサーダッシュボードへ遷移します。

### 2. センサーダッシュボード (`Screen_Sensor`)
- **環境データ表示**: SCD41センサーから取得した 現在のCO2濃度 (ppm)、温度 (℃)、湿度 (%)、および現在日時を表示します。
- **トレンドチャート (LVGL Chart)**: 右上のトグルボタンで2つのモードを即座にシームレスに切り替え可能です。
  - **4Hモード**: 過去4時間分の詳細な推移を1分刻み（240ポイント）で高速描画します。
  - **1Dモード**: 過去24時間分の推移を6分平均（240ポイント）で高速描画します（毎分配列をバックグラウンドで補完計算しロード遅延ゼロを実現）。
- **データ永続化と復元**: SDカードに日毎のログを自動記録し、起動時には過去の履歴を自動的に読み込んでグラフを復元します。
- 画面上の「日時表示エリア」をタップするとメニュー画面へ遷移します。

### 3. 手動日時設定画面 (`Screen_DateSet`)
- **日時マニュアル設定**: Wi-Fiがない環境下でも、ドラムロールUIから手動で「年・月・日・時・分」を設定し、内部時計とSDへの記録時間を補正できます。

### 4. メニュー画面 (`Screen_Menu`)
- 各種画面へ遷移するためのナビゲーションメニューです。
  - `[Wi-Fi Settings]` (WiFi設定画面へ)
  - `[Set Date & Time]` (手動日時設定画面へ)
  - `[Sensor Dashboard]` (センサーダッシュボードへ)

## 対応ハードウェア

| 機種 | 解像度 | マクロ定数 |
|------|--------|-----------|
| CrowPanel ESP32 2.4インチ | 320×240 | `#define CROWPANEL_24` |
| CrowPanel ESP32 2.8インチ | 320×240 | `#define CROWPANEL_28` |
| CrowPanel ESP32 3.5インチ | 480×320 | `#define CROWPANEL_35` |

`Globals.h` ファイル冒頭の `#define` 部分で使用する機種のコメントアウトを外して切り替えてください（デフォルトは2.8インチ）。

- 拡張モジュール: **Sensirion SCD41 CO2センサー** (I2C)
- ストレージ: **MicroSDカード** (SPI通信)
- タッチコントローラ: **XPT2046**

## 依存ライブラリ

Arduino IDEのライブラリマネージャーから以下をインストールしてください。

| ライブラリ | バージョン | 用途 |
|-----------|-----------|------|
| **[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)** (Bodmer) | 最新 | LCD描画・タッチ読み取り |
| **[lvgl](https://github.com/lvgl/lvgl)** | **v8.x** | GUI・ソフトウェアキーボード・チャート描画 |
| **[Sensirion I2C SCD4x](https://github.com/Sensirion/arduino-i2c-scd4x)**| 最新 | SCD41の制御・データ取得 |

※ `WiFi.h`、`Preferences.h`、`SPI.h`、`Wire.h`、`SD.h`、`FS.h` はESP32 Arduino Core標準のため追加インストール不要です。

## セットアップ

### 1. TFT_eSPIの設定
`TFT_eSPI`ライブラリフォルダ内の`User_Setup.h`を、お使いのCrowPanelに合わせて設定してください。CrowPanel向けの設定ファイルはElecrowのGitHubリポジトリで公開されています。
- [Elecrow CrowPanel GitHub](https://github.com/Elecrow-RD/CrowPanel-ESP32-Display-Course-File)

### 2. LVGLの設定
`lvgl`ライブラリフォルダ内の`lv_conf_template.h`を`lv_conf.h`にリネームし、先頭の`#if 0`を`#if 1`に変更して有効化します。

### 3. 書き込み
1. `crowPanel_scd41_sensor_v2.ino`をArduino IDEで開く
2. `Globals.h`を開き、使用する機種（2.4 / 2.8 / 3.5インチ）の定数定義を有効化する
3. ボードに「ESP32 Dev Module」またはCrowPanel対応のボードを選択する
4. スケッチを書き込む

## 動作フロー

```text
起動
 ├─ SDカード・SCD41初期化処理
 ├─ NVSに保存済みSSID/PASSがあるか
 │    └─ 自動接続を試みる（タイムアウト: 15秒）
 │          ├─ 成功: NTP同期後、SDから過去履歴(4H・1D)を一発ロードして【センサーダッシュボード】へ遷移
 │          └─ 失敗: 自動で【メニュー画面】へ遷移
 └─ 保存情報なし
      └─ 自動接続をスキップし、【メニュー画面】へ移行
```

※ メインループやSD読み書き・グラフ集計処理はバックグラウンドで継続的に実行されるため、メニュー画面や設定画面に遷移している最中もデータ損失なく記録が行われます。

## ライセンス

This project is open-sourced software licensed under the [MIT license](https://opensource.org/licenses/MIT).
