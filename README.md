# CrowPanel Multi-Screen Base (SCD41 Monitor v2)

ESP32を搭載したCrowPanelのLCD上にLVGLのUIを展開し、Wi-Fi設定、日時表示（NTP同期）、メニュー画面といった複数画面を自由に遷移して操作できる基本実装です。ファイル分割による構造化を取り入れており、将来的にはSCD41 CO2センサー等との連携開発を行うためのベースとなります。

## 概要

CrowPanelのタッチスクリーンとLVGL（Light and Versatile Graphics Library）を活用したUIです。
デバイス単体で周辺のWi-Fiアクセスポイントをスキャン、ソフトウェアキーボードでパスワードを入力して接続し、インターネット経由で時刻同期を行うまでの基本機能を提供します。設定した認証情報はESP32のNVS（フラッシュメモリ）にPreferencesライブラリで保存され、次回起動時は自動接続を開始します。

## 画面構成と機能

4つの画面（Screen）で構成されています。

### 1. Wi-Fi設定画面 (`Screen_WiFi`)
- **Wi-Fiスキャン機能**: `[Scan]`ボタンをタップすると周辺のアクセスポイントを検索し、ダイアログリストから接続したいSSIDを選択できます。
- **ソフトウェアキーボード**: SSIDまたはPASSフィールドをタップするとLVGLのキーボードが表示され、パスワードを入力できます。
- `[Connect]`ボタンで接続を開始し、成功時は日時表示画面へ遷移します。

### 2. 日時表示画面 (`Screen_DateTime`)
- **NTP時刻同期**: Wi-Fi接続完了後にNTPサーバーと同期し、現在の日時を1秒ごとに更新して表示します。
- 画面上の任意の場所をタップするとメニュー画面へ遷移します。

### 3. 手動日時設定画面 (`Screen_DateSet`)
- **日時マニュアル設定**: Wi-Fiが使用できない環境でも、ドラムロールUI（ローラー部品）から直感的に「年・月・日・時・分」をマニュアル設定できます。
- 設定した日時は内部時計（RTC）に反映され、直後からカウントアップされます。

### 4. メニュー画面 (`Screen_Menu`)
- **ナビゲーション機能**: メニュー上の各ボタンから対象の画面へ切り替えられます。
  - `[Wi-Fi Settings]` (WiFi設定画面へ)
  - `[Set Date & Time]` (手動日時設定画面へ)
  - `[Display Date & Time]` (日時表示画面へ)

## 対応ハードウェア

| 機種 | 解像度 | マクロ定数 |
|------|--------|-----------|
| CrowPanel ESP32 2.4インチ | 320×240 | `#define CROWPANEL_24` |
| CrowPanel ESP32 2.8インチ | 320×240 | `#define CROWPANEL_28` |
| CrowPanel ESP32 3.5インチ | 480×320 | `#define CROWPANEL_35` |

`Globals.h` ファイル冒頭の `#define` 部分で使用する機種のコメントアウトを外して切り替えてください（デフォルトは2.8インチ）。

- タッチコントローラ: **XPT2046**（`TFT_eSPI`の`getTouch()`を使用）

## 依存ライブラリ

Arduino IDEのライブラリマネージャーから以下をインストールしてください。

| ライブラリ | バージョン | 用途 |
|-----------|-----------|------|
| **[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)** (Bodmer) | 最新 | LCD描画・タッチ読み取り |
| **[lvgl](https://github.com/lvgl/lvgl)** | **v8.x** | GUI・ソフトウェアキーボード |

※ `WiFi.h`、`Preferences.h`、`SPI.h`、`Wire.h` はESP32 Arduino Core標準のため追加インストール不要です。

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
 ├─ NVSに保存済みSSID/PASSがある
 │    └─ 自動接続を試みる（タイムアウト: 15秒）
 │          ├─ 成功: NTP時刻同期後、自動で【日時表示画面】へ遷移
 │          └─ 失敗: 自動で【メニュー画面】へ遷移
 └─ 保存情報なし
      └─ 自動接続をスキップし、【メニュー画面】へ移行
```

各画面からいつでも他の画面へ遷移することができます（例: 日時表示画面をタップ→メニュー画面→WiFi設定画面）。

## ライセンス

This project is open-sourced software licensed under the [MIT license](https://opensource.org/licenses/MIT).
