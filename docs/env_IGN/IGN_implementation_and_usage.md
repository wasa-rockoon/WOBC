# IGN環境の実装と使い方

## 概要

PlatformIO環境`IGN`（IGNモジュール）の実装内容と使用手順をまとめる．
本モジュールは，電源投入から36秒の警告シーケンスを実行したのちモデルロケットのイグナイタへ通電し，
通電開始から3秒後に自動遮断する．シーケンス状態と点火回路の電圧・電流はテレメトリとして送信し，SDカードへ保存する．

- 作成日: 2026-08-29
- 対象環境: `IGN`
- 対象モジュール: `IGN`
- 対象基板: ESP32-S3（`esp32-s3-devkitc-1`）
- モジュールID: `I` (`0x49`)
- ユニットID: `0x40`
- IGNコンポーネントID: `36` (`0x24`)

シーケンスの修正経緯は`docs/env_IGN/IGN_ignition_sequence_fix.md`に，
テレメトリの詳細仕様は`docs/components.md`の`Ignite (36)`に記載する．

## 実装

### 1. 全体構成

点火の判断（いつ・どの出力を有効にするか）と，GPIOの操作を分離している．
判断部はハードウェアに依存しないため，nativeテストで時間境界と異常系を検証できる．

| ファイル | 役割 |
| --- | --- |
| `src/components/IGN/IGNSequence.h` | 点火シーケンスのステートマシン．時間管理と出力要求の決定 |
| `src/components/IGN/IGN.h` | IGNコンポーネントの公開API，状態変数および安全機構の定義 |
| `src/components/IGN/IGN.cpp` | GPIO制御，INA226の初期化，自動遮断タスク，テレメトリ送信 |
| `src/modules/IGN/main.cpp` | モジュールの初期化順序と自動開始 |
| `src/library/test/test_kernel_unit.cpp` | `IGNSequence`のnativeテスト |

IGNモジュールには，IGNのほかにPressure，Heater，Logger，Mainのコンポーネントを搭載する．

### 2. 点火シーケンス（IGNSequence）

`update(now_ms)`を呼ぶと，フェーズを進めたうえで，そのときの出力要求を`Snapshot`として返す．
`Snapshot`は`phase`，`phase_elapsed_ms`，`sequence_elapsed_ms`，`remaining_ms`，`high`，`low`，`status_led`，`phase_changed`を持つ．

| フェーズ | 値 | 時間 | HIGH | LOW | ブザー | NORMAL LED |
| --- | ---: | ---: | --- | --- | --- | --- |
| Startup | 0 | 1秒 | H | L | 連続 | 点灯 |
| Countdown | 1 | 30秒 | 断続 | L | 1秒周期で0.2秒 | ブザーと同期して点滅 |
| Final | 2 | 5秒 | H | L | 連続 | 5 Hzで点滅 |
| Ignition | 3 | 3秒 | H | H | — | 点灯 |
| Done | 4 | 無期限 | L | L | — | 2秒周期で0.1秒点灯 |
| Disarmed | 5 | 無期限 | L | L | — | 消灯 |
| Fault | 6 | 無期限 | L | L | — | 2 Hzで点滅 |

開始から点火までは最短36秒である．

一度の`update()`で進めるフェーズは1つだけとする．
タスクが遅延した場合も各警告フェーズは所定時間実行され，遅延によって点火が早まることはない．
また，`Ignition`では期限を判定してから出力要求を返すため，更新が遅れた場合に期限後の通電要求を返すことはない．

シーケンスの開始は1回のbootにつき1回だけ許可する．中止後の再開始も拒否する．

### 3. GPIO制御（IGN）

| ピン名 | 既定GPIO | 種別 | 内容 |
| --- | ---: | --- | --- |
| NORMAL | 5 | Digital Out | 状態表示LED．点火回路には接続されていない |
| HIGH | 6 | Digital Out | HIGHのみでブザーが鳴る |
| LOW | 4 | Digital Out | HIGHとLOWの両方がHIGHで点火する |

出力の切り替えには次の規則を設けている．

- 停止時はLOW側を先にLOWへ戻し，その後HIGH側をLOWへ戻す．
- 点火時はHIGH側を先に有効化し，LOW側を最後に有効化する．
- LOWだけを有効にする要求は不正とみなし，HIGH・LOWの両方を停止する．
- 自動遮断タスクがアームされていない状態でのLOW有効化要求は拒否する．
- 中止要求または自動遮断の発生後は，古い`Snapshot`が届いても出力を再有効化しない．
- GPIOの更新は`portENTER_CRITICAL()`で保護し，自動遮断タスクと通常ループの競合を防ぐ．

### 4. 起動時の安全化

`prepareSafeOutputs()`は`kernel::begin()`より前に単独で呼び出せる．
出力ラッチをLOWへ設定してから`pinMode()`で出力方向へ変更するため，起動時に通電を要求する状態を経由しない．

カーネルやコンポーネントの初期化に失敗して`setup()`が途中で終了した場合も，点火出力はLOWのままとなる．

### 5. 独立した3秒自動遮断

`Ignition`へ入る直前に，専用の高優先度FreeRTOSタスクへ通知する．
このタスクは通電開始から3秒後にHIGH・LOWをLOWへ戻す．
通常のIGNループ，ログ送信およびテレメトリ送信とは独立して動作するため，
これらが停止しても通電時間は3秒に制限される．

タスクの起動または通知に失敗した場合は，点火せず`Fault`へ移行する．
遮断待ち時間には`IGNSequence::ignition_ms`を使用するため，シーケンス側の通電時間と常に一致する．

### 6. 初期化時の検証とFault

`begin()`は次を検証し，いずれかに失敗した場合は`Fault`へ移行して出力をLOWへ固定し，`false`を返す．

- ピン番号が負値または`no_pin`でないこと，および3本が相互に重複しないこと
- サンプリング周波数が`1〜1000 Hz`の範囲にあること
- INA226（アドレス`0x4D`）の`begin()`が成功すること
- INA226の`setMaxCurrentShunt(4, 0.020)`（最大4 A，シャント20 mΩ）が成功すること
- 自動遮断タスクの生成が成功すること

### 7. テレメトリ

電力テレメトリ`I`はサンプリング周期ごとに，シーケンス状態テレメトリ`S`は状態が変化した時点および1秒周期で送信する．

| パケット | 主なエントリ |
| --- | --- |
| `I` | `Vi`(mV)，`Ii`(mA)，`Pi`(mW)，`Ts`(ms) |
| `S` | `Ph`，`Et`，`St`，`Rt`，`Bz`，`Ig`，`Hi`，`Lo`，`Nl`，`Ok`，`Ts` |

`Rt`は点火までの残り時間(ms)であり，地上側でカウントダウンをそのまま表示できる．
`Hi`，`Lo`および`Nl`はGPIOへ要求したソフトウェア上の状態であり，電気的なreadbackではない．
全エントリの定義は`docs/components.md`を参照する．

## 使い方

### 1. 配線

| 接続先 | GPIO |
| --- | --- |
| I2C（SDA, SCL） | 17, 16 |
| CAN（TX, RX） | 44, 43 |
| Serial0（TX, RX） | 2, 1 |
| SDカード（SCK, MISO, MOSI, CS） | 12, 13, 11, 10 |
| SDカード挿入検出 | 9 |
| STATインジケータ | 42 |
| ERRORインジケータ | 41 |
| NORMAL, HIGH, LOW | 5, 6, 4 |

点火回路の電圧・電流監視にはINA226（アドレス`0x4D`）をI2Cに接続する．

### 2. ビルドと書き込み

```sh
# ビルド
pio run -e IGN

# 書き込み
pio run -e IGN -t upload

# シリアルモニタ（115200 baud）
pio device monitor -b 115200
```

`default_envs`に`IGN`を含めているため，環境を指定しない`pio run`でもIGNがビルドされる．

Windowsで`pio`にPATHが通っていない場合は`%USERPROFILE%\.platformio\penv\Scripts\pio.exe`を使用する．

### 3. 初回起動時のモジュールID

`main.cpp`では`kernel::begin(module_id, true)`としてモジュールID検査を有効にしている．
以前に別のモジュールのファームウェアを書き込んだ基板では，NVSに残る旧IDと一致せず`assert`で起動が停止し，
ハートビートを含むパケットが一切出力されない．

この場合は`docs/nvs_module_id_troubleshooting.md`の復旧手順に従い，
一度`kernel::begin(module_id, false)`で起動して保存済みIDを削除してから`true`へ戻す．

### 4. 点火試験の手順

1. イグナイタと点火用電源を切り離した状態でファームウェアを書き込む．
2. 電源を投入し，ブザーとNORMAL LEDが第5節の表のとおりに変化することを確認する．
3. オシロスコープまたはロジックアナライザで，HIGH・LOWの立ち上がり順序，36秒の警告時間，
   および通電開始から3秒後の遮断を確認する．
4. テレメトリで`Ok`が真であること，`Ph`が0→1→2→3→4と遷移することを確認する．
5. 以上を確認したのち，実イグナイタを接続した試験へ移る．

電源投入またはリセットのたびにシーケンスが自動的に開始する．
イグナイタを接続したままの再起動は，そのまま点火動作となる．

### 5. ブザーとLEDによる状態確認

| 電源投入からの経過時間 | フェーズ | ブザー | NORMAL LED |
| --- | --- | --- | --- |
| 0〜1秒 | Startup | 連続 | 点灯 |
| 1〜31秒 | Countdown | 1秒周期で0.2秒 | ブザーと同期して点滅 |
| 31〜36秒 | Final | 連続 | 5 Hzで点滅 |
| 36〜39秒 | Ignition | — | 点灯 |
| 39秒以降 | Done | — | 2秒周期で0.1秒点灯 |

NORMAL LEDが2 Hzで点滅している場合は`Fault`であり，点火は行われない．

### 6. テレメトリの確認と保存

Loggerコンポーネントが全パケットを購読してSDカードへ書き込むため，
`I`および`S`パケットは追加設定なしで保存される．

地上側では`S`パケットの`Ph`と`Rt`を表示するとシーケンスの進行を追える．
`Ph`の値と状態の対応は第2節の表を参照する．

### 7. タイミングの変更

`src/components/IGN/IGNSequence.h`の定数を変更する．

| 定数 | 既定値 | 内容 |
| --- | ---: | --- |
| `startup_buzz_ms` | 1000 | 電源投入直後にブザーを鳴らす時間 |
| `countdown_ms` | 30000 | 間欠ブザーで待機する時間 |
| `countdown_beep_period_ms` | 1000 | 間欠ブザーの周期 |
| `countdown_beep_on_ms` | 200 | 間欠ブザー1回あたりの鳴動時間 |
| `final_buzz_ms` | 5000 | 点火直前にブザーを鳴らし続ける時間 |
| `ignition_ms` | 3000 | 通電時間．自動遮断タスクの待ち時間も兼ねる |

`ignition_ms`はシーケンスと自動遮断タスクの双方が参照するため，片方だけを変更することはできない．

### 8. 自動開始しない構成

`main.cpp`では`ign.begin(true)`により，最初のIGNループからフェーズ0を開始する．
初期化のみを行い，開始タイミングを別に決めたい場合は`begin(false)`を使う．

```cpp
if (!ign.begin(false)) {
    // 初期化失敗．点火出力はLOWに固定されている
}

// 任意のタイミングで開始する．1回のbootにつき1回だけ成功する
ign.startSequence();
```

`begin(false)`の場合，開始するまで`Ph`は5（Disarmed）となる．

### 9. 中止

```cpp
ign.abortSequence();  // 点火出力を直ちにLOWへ戻し，Disarmedへ移行する
```

中止後に`startSequence()`を呼んでも再開始はできない．再度実行するには電源の再投入が必要である．

なお，通信経由の開始コマンドおよび緊急停止コマンドは実装していない．
`abortSequence()`はモジュール内部のエラー処理から呼ぶことを想定している．

## テスト

```sh
pio test -e native
```

`IGNSequence`に対し，フェーズ境界（1秒，31秒，36秒，39秒），更新遅延時に期限後の通電要求を返さないこと，
中止時とFault時に出力要求が停止すること，同一boot中の再開始を拒否すること，
`millis()`の32 bit周回後も経過時間を正しく扱うことを検証している．

最終確認時の結果は次のとおり．

- `pio run -e IGN`: SUCCESS（RAM 40,504 bytes / 12.4%，Flash 433,785 bytes / 13.0%）
- `pio test -e native`: 6件中6件成功（うち4件がIGNシーケンス）

## トラブルシューティング

| 症状 | 想定原因 | 対処 |
| --- | --- | --- |
| ハートビートもテレメトリも出ない | NVSの保存済みモジュールIDが不一致 | `docs/nvs_module_id_troubleshooting.md`の復旧手順を実施する |
| ERRORインジケータ（GPIO41）が点灯したまま | `setup()`が初期化失敗で途中終了した | シリアル出力とエラーパケットで失敗したコンポーネントを特定する |
| NORMAL LEDが2 Hzで点滅する | `Fault`．INA226の初期化・校正失敗，または遮断タスクの起動失敗 | I2C配線とINA226（`0x4D`）を確認する |
| IGNのテレメトリだけ出ない | 他コンポーネントの初期化失敗により`ign.begin()`を実行していない | 起動順序と各コンポーネントの戻り値を確認する |
| SDカードに保存されない | カード未挿入または初期化失敗 | エラーコード`cNI`（未挿入），`cBF`（初期化失敗），`cWE`（書き込み失敗）を確認する |

## 注意事項

- 電源投入またはリセットで自動的にシーケンスが開始する．アーミング操作や通信による中止手段はない．
- 3秒自動遮断はソフトウェア上の高優先度タスクであり，外部の物理的なキルスイッチやハードウェアタイマーの代替ではない．
- `Hi`，`Lo`，`Nl`はソフトウェア上の要求状態であり，実際の電気的状態を保証するものではない．
- 実機での書き込み，GPIO波形の測定，INA226を接続した試験および点火試験は未実施である．

## 関連ファイル

- `src/components/IGN/IGNSequence.h`: 点火シーケンスと時間管理
- `src/components/IGN/IGN.h`: IGNの公開API，状態および安全機構の定義
- `src/components/IGN/IGN.cpp`: GPIO制御，センサ初期化，遮断タスク，テレメトリ
- `src/modules/IGN/main.cpp`: IGNモジュールの初期化順序と自動開始
- `src/library/test/test_kernel_unit.cpp`: IGNシーケンスのnativeテスト
- `docs/components.md`: IGNコンポーネントおよびテレメトリ仕様
- `docs/modules.md`: IGNモジュール構成
- `docs/env_IGN/IGN_ignition_sequence_fix.md`: 安全性・動作修正の経緯
- `docs/nvs_module_id_troubleshooting.md`: モジュールID不一致による起動停止の復旧手順
- `platformio.ini`: IGNビルド環境
