// ota_version.h
//
// OTA ローカルファームウェアバージョン定義
// crowPanel_scd41_sensor_v2
//
// このファイルがバージョン文字列の唯一の定義場所です。
// リリース時にここだけ更新してください。
//
// 参照先:
//   - src/ota.cpp          : ファームウェアの自己バージョン文字列として使用
//   - otaImageUploader.py  : アップロード時のバージョンとして自動取得

#ifndef OTA_VERSION_H
#define OTA_VERSION_H

#define OTA_LOCAL_VERSION  "v0.0.17"

#endif // OTA_VERSION_H
