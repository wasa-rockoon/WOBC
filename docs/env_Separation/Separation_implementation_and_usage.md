# Separationの実装・検証資料

更新日：2026-09-17。気圧開始条件・開始予約中のアボート競合を修正した作業コピーを対象とし、未コミットの変更を含みます。操作・設定・テレメトリは[ユーザーガイド](Separation_user_guide.md)を参照してください。

## ファイル構成

| ファイル | 役割 |
| --- | --- |
| [main.cpp](../../src/modules/Separation/main.cpp) | 初期化、FlightPin購読、実GPIO確認、開始要求、再挿入割り込みの登録 |
| [NichromeStartGate.h](../../src/components/Nichrome/NichromeStartGate.h) | 挿入履歴、抜去状態、気圧高度・更新停止・代替待ち時間の判定。Mainタスク専用 |
| [Nichrome.h](../../src/components/Nichrome/Nichrome.h) | 公開API、GPIO・要求フラグ・タスク状態 |
| [Nichrome.cpp](../../src/components/Nichrome/Nichrome.cpp) | 出力制御、INA226、アボート、遮断タスク、テレメトリ |
| [NichromeSequence.h](../../src/components/Nichrome/NichromeSequence.h) | 警告・通電・終了の状態機械と時間設定 |
| [FlightPin.cpp](../../src/components/FlightPin/FlightPin.cpp) | ピン状態を1 Hzで取得し、`Fp`・`ET`・`TS`を送信 |
| [nichrome_flight_pin_test.cpp](../../tests/nichrome_flight_pin_test.cpp) | ホスト向け開始判定・シーケンス中止テスト |
| [platformio.ini](../../platformio.ini) | ESP32-S3向け`Separation`ビルド環境 |

## 初期化の順序と失敗時の扱い

1. Serialを115200 baudで起動し、`nichrome.prepareSafeOutputs()`を実行する。
2. 1秒待ち、unit IDを設定して`kernel::begin(module_id, true)`を呼ぶ。
3. Serial0のピン設定、Wire、SerialBus、SPIを初期化する。CANは起動しない。
4. 1秒待ち、インジケータ、Pressure、Logger、LiPoPower、GPSを開始する。
5. `nichrome.begin(false)`に成功した場合だけ、FlightPin割り込みを登録する。
6. 割り込み登録に成功した場合だけ、Main、FlightPinを開始する。
7. ERRORインジケータを通常のエラー数監視へ切り替える。

`prepareSafeOutputs()`はラッチをLOWにしてから出力方向へ設定します。カーネル起動前に実行するため、その後の初期化で停止してもNichrome出力を非通電に保つ構成です。

`begin(false)`は自動開始を予約せず、初期状態を`Disarmed`とします。INA226の接続・校正失敗や遮断タスク生成失敗では出力を停止して失敗を返します。コンポーネントタスクの起動失敗でも出力停止と遮断タスクの削除を行います。すべての失敗で`Fault`の通知・点滅まで動くわけではありません。

Pressure、Logger、LiPoPower、GPS、Main、FlightPinの`begin()`戻り値は現mainでは確認していません。ERRORインジケータの通常表示だけで全機能の正常起動を保証するものではありません。

## 開始判定

MainはFlightPinのtelemetry `F`、component `0x50`、同一unit IDを購読します。キュー長は4です。受信した`Fp`の型と値を調べ、`observeFlightPin()`に渡します。

`NichromeStartGate`は挿入確認を保持します。有効な抜去を確認し、`updateFlightPin()`呼び出し時の実GPIOもLOWならその時点を計測起点にします。挿入中・新たな抜去では気圧回数・更新時計を初期化し、Mainは保留気圧パケットを破棄します。経過時間は`uint32_t`の差で計算し、32 bitの時刻周回をまたぐ待機を扱います。

Mainは同一unitのPressureテレメトリをキュー長4で購読し、`Sm`が自モジュールのものだけを採用します。`PA`・`Va`・`Ts`を検査し、有効な新規測定について20,000 m超の回数と更新時刻を記録します。30回成立、または抜去後1時間かつ有効更新停止60秒で開始可能になります。GPSは購読しません。無効値・閾値以下で回数をリセットしますが、無受信の時間だけではリセットしないIGN方式で、厳密な30秒継続やセンサ固着の検出は対象外です。

Mainは成立時に`separation_start_attempted_`を立ててから`startSequence()`を呼びます。このフラグは起動中に戻さないため、開始拒否・中止・完了後もMainから再要求しません。開始が受理された直後には実GPIOを再確認し、HIGHなら通常タスク用の`abortSequence()`を呼びます。

`NichromeStartGate`自体には一度きりの開始制限や出力停止機能はありません。Mainの要求履歴とNichromeの出力制御が必要です。

## アボートと通電時間の監視

`beginFlightPinAbortInterrupt()`は`ESP_INTR_FLAG_IRAM`でGPIO ISRサービスを生成し、FlightPinの立ち上がりに`onFlightPinInserted()`を登録します。既存ISRサービスがある場合も、IRAM属性を確認できないため登録手順を失敗扱いにし、Mainの開始へ進みません。

コールバックは`nichrome.abortSequenceFromISR()`を呼びます。開始要求後および活動中フェーズで有効な場合、ISR用クリティカルセクション内で開始予約を解除、中止要求をラッチし、LOW側、HIGH側の順にGPIOレジスタで出力を停止します。ISR内ではログ・パケット送信・状態機械の更新をしません。

通常のNichromeループが中止要求を取り込み、状態機械を`Disarmed`へ移します。開始前や終了後など、アボートがアームされていない期間の挿入割り込みは無視します。開始前のタイマーリセットはMain側で行います。

2026-09-17の修正では、アボートの有効状態を更新するときに、活動中フェーズだけでなく`start_requested_`も同じクリティカルセクション内で確認します。これにより、Mainの開始予約後に古い`Disarmed`のSnapshotでアボートを無効化する競合を防ぎます。中止要求が既にあれば再アームしません。

`Ignition`への遷移時には、出力を有効にする前に高優先度の遮断タスクへ通知します。通知に失敗すれば`Fault`へ移行します。タスクは`NichromeSequence::ignition_ms`（120,000 ms）だけ待ち、監視が有効なら出力を強制停止します。状態機械も同じ定数で終了を判定します。

出力制御には、中止・遮断後に古いSnapshotで再通電しないための要求フラグ確認、監視未作動時の通電拒否、LOW側だけを有効にする要求の拒否があります。実際のISR応答時間や電気的な遮断時間はホストテストの対象外です。

## APIを使用するとき

| API | 用途 |
| --- | --- |
| `prepareSafeOutputs()` | カーネル起動前にも呼べる出力LOW初期化 |
| `begin(false)` | センサ・監視タスク・コンポーネントを初期化して待機 |
| `startSequence()` | 初期化後の開始予約。FlightPinやタイマー条件そのものは検査しない |
| `abortSequence()` | 通常タスクからの出力停止と中止要求 |
| `abortSequenceFromISR()` | 有効期間中のISRからの出力停止と中止要求 |
| `phase()`、`healthy()` | 状態機械の段階、初期化成功状態の参照 |

`begin(true)`や`startSequence()`の直接呼び出しでは、MainのFlightPin条件を通りません。通常のSeparationでは`begin(false)`とMainの開始経路を使用します。シーケンスが実際に開始した後は状態機械側の履歴でも再開始を拒否しますが、開始予約直後の中止を含めた通常モジュールの一度きり制限はMainのフラグが担います。

## IGNとの共通点・維持している相違点

| 項目 | Separationの現状 |
| --- | --- |
| 起動直後の出力初期化・開始時の挿入確認 | IGNと同様の方式 |
| 再挿入によるISRアボート・開始直後のGPIO再確認 | IGNと同様の方式 |
| 開始条件 | 気圧高度20 km超を連続30回、または抜去後1時間かつ有効な気圧更新停止60秒。GPS条件なし |
| 警告時間 | 合計36秒（1＋30＋5）。IGNは66秒 |
| 通電設定 | 120秒。IGNは60秒 |
| GPS・LiPoPower | Separationの直接取得・電源計測を維持 |
| CAN・Telemeter・Heater | CANとTelemeterは無効、Heaterコンポーネントは未搭載 |

初回のFlightPin対応に加えて、2026-09-17に開始ゲートとmainの気圧購読を更新し、`Nichrome.cpp`のアボート有効状態更新を修正しました。`Nichrome.h`・`NichromeSequence.h`、IGNの実装、センサ処理は今回変更していません。

## 検証

2026-09-17の修正後に以下が成功しました。実機試験は行っていません。

- `nichrome_flight_pin_test.cpp`のg++によるコンパイルと実行：成功。
- `platformio run -e Separation`：成功。RAM 41,148 bytes、Flash 451,177 bytes。
- [nichrome_abort_race_test.ps1](../../tests/nichrome_abort_race_test.ps1)：成功。実装から開始要求・ISR・アボート状態更新部分を取り出し、GPIO／RTOSをホスト用に置き換えて、古い待機Snapshotの取得後に開始予約と再挿入が起きる順序を検証。
- 変更差分の空白検査：成功。

ビルドには既存箇所の`ARDUINO_RUNNING_CORE`再定義や型変換の警告があり、警告ゼロではありません。

ホストテストを再実行するPowerShellの例（プロジェクトルートから）：

```powershell
g++ -std=c++14 -Wall -Wextra -Werror -I . tests/nichrome_flight_pin_test.cpp -o nichrome_flight_pin_test.exe
if ($LASTEXITCODE -ne 0) { throw "Test compilation failed" }
.\nichrome_flight_pin_test.exe
if ($LASTEXITCODE -ne 0) { throw "Test failed" }
Remove-Item -LiteralPath .\nichrome_flight_pin_test.exe
```

このテストは`tests/`に置いた単独の実行ファイルです。PlatformIOの`test_dir`は別の`src/library/test`なので、`pio test -e native`では自動実行されません。

| テストで確認したこと | 範囲 |
| --- | --- |
| 起動時から抜去状態 | 挿入確認がなければ開始しない |
| 高度・時間境界 | 20 kmちょうどは不成立、超過30回で成立。代替待ち1時間と気圧停止60秒の両方が必要 |
| 気圧更新の継続・復旧 | 正常な低高度測定が続く間は時間だけで開始せず、復旧すれば代替条件も不成立 |
| 古い測定・重複 | 抜去前の測定や同一測定の再送で回数を進めない |
| 同一処理回での再挿入と抜去 | 待ち時間をリセット |
| 実GPIO HIGH、無効な`Fp` | 開始判定のリセット |
| 時刻周回 | `UINT32_MAX`をまたいでも経過時間を計算 |
| 各活動フェーズのアボート | 状態機械の出力要求停止と再開始拒否 |
| 既存時間設定 | カウントダウン30秒・通電120秒を維持 |

GPIO割り込み、FreeRTOSの競合・監視タスク、実パケット配送、センサ実測、SD保存、実機への書き込み、ニクロム線による分離はこのテストでは検証していません。開始直前の再挿入、チャタリング、タスク遅延時の遮断などは実機確認項目です。
