# crowPanel_wifi_conf_gui

ESP32を搭載したCrowPanelのLCD上にLVGLのソフトウェアキーボードを表示し、タッチ操作でWi-Fiの接続先（SSIDとパスワード）を設定するサンプル実装です。

## 概要

CrowPanelのタッチスクリーンにLVGL（Light and Versatile Graphics Library）のUIウィジェットを表示することで、デバイス単体でWi-Fi設定を完結させます。設定した認証情報はESP32のNVS（フラッシュメモリ）にPreferencesライブラリで保存され、次回起動時は自動接続を試みます。

## デモ画面構成

```
+----------------------------------------+
|  WiFi WiFi Setup              [Connect] |
+----------------------------------------+
| SSID: [Enter WiFi SSID              ]  |
| PASS: [Enter Password               ]  |
| Enter SSID & Password, then press Connect.
+========================================+
|   Q  W  E  R  T  Y  U  I  O  P       |
|   A  S  D  F  G  H  J  K  L          |
|   Z  X  C  V  B  N  M    [⌫]        |
|  [ABC]  [Space]  [Enter]  [Cancel]    |
+========================================+
```

- SSIDまたはPASSフィールドをタップするとキーボードが表示されます
- `[Enter]` または `[Connect]` ボタンで接続を開始します
- 接続成功時はIPアドレスを、失敗時はエラーメッセージを表示します

## 対応ハードウェア

| 機種 | 解像度 | マクロ定数 |
|------|--------|-----------|
| CrowPanel ESP32 2.4インチ | 320×240 | `#define CROWPANEL_24` |
| CrowPanel ESP32 2.8インチ | 320×240 | `#define CROWPANEL_28` |
| CrowPanel ESP32 3.5インチ | 480×320 | `#define CROWPANEL_35` |

`.ino`ファイル冒頭の`#define`で使用する機種を切り替えてください（デフォルトは2.8インチ）。

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

1. `crowPanel_wifi_conf_gui.ino`をArduino IDEで開く
2. `.ino`冒頭の`#define`で使用する機種（2.4 / 2.8 / 3.5インチ）を選択する
3. ボードに「ESP32 Dev Module」またはCrowPanel対応のボードを選択する
4. 書き込む

## 動作フロー

```
起動
 ├─ NVSに保存済みSSID/PASSがある
 │    └─ 自動接続を試みる（タイムアウト: 15秒）
 └─ 保存情報なし / 接続失敗
      └─ UI表示でユーザーが入力・接続ボタン押下
            └─ NVSに保存 → WiFi.begin()
                  ├─ 成功: IPアドレス表示（緑）
                  └─ 失敗: エラーメッセージ表示（赤）
```

## ライセンス

This project is open-sourced software licensed under the [MIT license](https://opensource.org/licenses/MIT).
