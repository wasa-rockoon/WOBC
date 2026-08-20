# origin/feature/CMG と origin/feature/Trackersrc2 の差分・Pressure不安定事象 調査メモ

## 1. この文書の目的

この文書は、次の調査内容を後から追記・修正できるようにまとめたものである。

- `origin/feature/Trackersrc2` と `origin/feature/CMG` の全フォルダ差分
- `components`、`library`、`modules`、`hardware`、ビルド設定への影響
- TrackerからCMGへ変更したときにPressureなどが不安定に見える事象
- IMUを停止し、CMGTestでPressureだけを動かしても再現する事象
- `util.py` で一時停止したように見えた後、パケットがまとめて表示される可能性
- 現時点の原因候補、切り分け方法、推奨修正案

本書は静的なコード調査結果であり、実機測定結果ではない。確定事項と仮説を分けて記載する。

## 2. 比較条件

比較対象は、調査時点でローカルに存在した次のremote-tracking refである。作業ツリー同士ではなく、origin同士を比較した。

| ブランチ | コミット | 日時 | コミットメッセージ |
|---|---|---|---|
| `origin/feature/Trackersrc2` | `17da563f01c655dce754a8ca3ba30d94faa6d0cc` | 2025-11-04 00:50:59 +0900 | GPSの相対時間コメントアウト |
| `origin/feature/CMG` | `328ca86f29baaf160f7de5180625635fb5063932` | 2026-08-03 18:25:04 +0900 | heaterを修正 |

注意事項：

- この比較は、調査時点のローカルremote-tracking refを使用している。最新のリモート状態を再確認する場合は、先に `git fetch origin` が必要である。
- 作業ツリー内の未追跡ファイルは比較対象に含めていない。
- 比較方向は `Trackersrc2 -> CMG` である。

再現用コマンド：

```powershell
git diff --name-status origin/feature/Trackersrc2..origin/feature/CMG
git diff --stat origin/feature/Trackersrc2..origin/feature/CMG
git diff origin/feature/Trackersrc2..origin/feature/CMG -- <path>
```

## 3. 差分全体の概要

```text
108 files changed, 77084 insertions(+), 62710 deletions(-)
```

| 状態 | ファイル数 |
|---|---:|
| 追加 | 65 |
| 変更 | 42 |
| 削除 | 1 |

トップレベル別では次の構成である。

| 範囲 | 追加 | 変更 | 削除 | 主な内容 |
|---|---:|---:|---:|---|
| `.github` | 1 | 0 | 0 | PlatformIOビルドworkflow追加 |
| `hardware` | 37 | 24 | 1 | CMGTestMk2、IMU2、MissionBus、RCSなどのKiCadデータ |
| `platformio.ini` | 0 | 1 | 0 | RP2350、CMGTest、MissionBus環境などを追加 |
| `src` | 27 | 17 | 0 | IMU、Heater、Logger、CMGTestなど |

差分行数の大部分はKiCadデータ、製造zip、ICM42688ドライバーなどの追加によるものである。行数だけではランタイムへの影響度を判断できない。

## 4. ソフトウェア差分と影響

### 4.1 components

`src/components` は31ファイル、2923行追加、80行削除である。

| コンポーネント | 主な差分 | 想定される影響 |
|---|---|---|
| Attitude | Madgwick実装を追加。KalmanAttitudeは空ファイルとして追加 | 姿勢推定機能の土台を追加。空ファイルは未実装状態 |
| GPS | `HardwareSerial`の所有から`Serial1`ポインター利用へ変更。ESP32/RP2040/RP2350で初期化を分岐 | RP系対応が増える一方、常に`Serial1`を使うため他用途との競合に注意 |
| Heater | MCP3424の4ch温度測定を追加。各chで最大300 msポーリング | 同じI2Cを共有すると長時間占有し得る。ただしPressure単体再現時の主因ではない |
| IMU | BMI/BMMとICM42688/MMC5603、Madgwick/Kalman、100 Hz等の処理を追加 | I2C負荷、タイマー負荷、パケット量が増える。ただしIMU停止時にも再現するため今回の必要条件ではない |
| LiPoPower | 無効ピンを許容し、充電判定からSTAT2条件を除外 | MissionBus等で未接続ピンを使える。充電状態判定条件が変わる |
| LoRa | `E220.h`を`e220.h`へ修正、baudを9800から9600へ修正 | 大文字小文字を区別する環境でのビルド改善、通信速度設定の正常化 |
| Logger | SD書き込みを別タスク/別コアへ分離、キューを32から4096へ増加、1秒flush、20分ファイル分割を追加 | センサー処理のブロック軽減を狙う一方、RAM使用量、並行処理、SDエラー時の再試行、共有状態の影響が増える |
| Pressure | `Pressure::setup()`内の`Wire.begin()`をコメントアウト | main側のI2C初期化条件がそのまま残る。CMGTestでは1 MHz設定が維持される |
| Servo | 角度指令・ADC角度読出し・テレメトリを追加 | タイマー、ADC、PWM資源を追加使用 |
| Tachometer | 割り込みによるRPM計測を追加 | 割り込み資源を使用。2パルス/回転を前提 |
| Telemeter | listener条件とWebSocket処理、送信対象パケットを変更 | サーバーへ送るパケット形式と対象が変わる。作成した`packet_tele`ではなく元の`packet`を送信している点に注意 |

Loggerについての補足：

- CMG側では旧実装の `bool ok = true; ok |= write_result;` が `ok &= write_result;` に変わっており、書込み失敗判定は改善している。
- 一方で、旧実装が行っていたローカルパケットへのunit ID・`Ts`追加をせず、生パケットを直接保存するようになっている。ログ解析側が旧形式を期待している場合は影響する。
- `WOBC_LOGGER_PACKET_QUEUE_SIZE=4096` は大量のキュー領域を必要とする。対象MCUごとのRAM使用量確認が必要である。

### 4.2 library

`src/library` は7ファイル、133行追加、4行削除である。

| ファイル群 | 主な差分 | 想定される影響 |
|---|---|---|
| `common.h` | RP2350対応、`request_file_split`、SPI/I2C初期化ラッパー追加 | MCU共通化。CMGTestのI2Cクロックをmainから指定可能になった |
| `core/can_bus.cpp` | CANフレーム番号判定を8 bitから5 bit maskへ変更 | CAN ID内のフレーム番号領域に合わせた修正 |
| `driver/can_if.h` | `#pragma once`追加 | 多重include防止 |
| RP2040 CAN | includeとheader guard相当を追加 | RP2040ビルド安定化 |
| RP2350 CAN | ドライバーを新規追加 | RP2350でCANを利用可能にする |

`src/library/wcpp` のsubmodule参照は両ブランチとも次の同一コミットであり、差分はない。

```text
116237d2961fd7037552ff049ad40530e659e73e
```

したがって、後述する`util.py`とPacketデコードの問題はCMGブランチで新規に入った差分ではなく、両ブランチに共通して存在する。

### 4.3 modules

| モジュール | 主な差分 | 想定される影響 |
|---|---|---|
| CMGTest | 新規追加。RP2350/ESP32向けCMG・IMU・Heater・Servo・Pressure試験構成 | I2C 1 MHz、IMU 100 Hz、Pressure 20 Hzなど高頻度構成 |
| MissionBus | 新規追加 | Pressure、IMU、GPS、Logger、LiPoPower等を統合 |
| Tracker | GPS/Loggerを外し、構成を簡略化。PressureとLiPoPowerの生成引数に問題あり | Tracker環境ではunit ID不整合を発生させる |
| GS | サンプルテレメトリ送信をコメントアウト | 定期的なテストパケットが送られなくなる |
| LoRa | channelを5から3へ変更 | 使用周波数/チャネル設定が変わる |

Tracker固有の注意点：

```cpp
// Trackersrc2
constexpr uint8_t unit_id = 0x61;
component::Pressure pressure(Wire, unit_id);

// CMG
component::Pressure pressure(Wire, 0);
constexpr uint8_t unit_id = 0x61;
```

CMGのTrackerでは、Pressureがremote telemetry用の5引数overloadを使いながら送信元unit IDに`0`を設定する。wcppでは`0`がlocal packetを表すため、7 byteで作ったremote headerを4 byte local headerとして解釈し、Python側のデコードが失敗する。

ただし、今回使用しているのはCMGTest環境であるため、このTracker固有の`Pressure(Wire, 0)`問題は直接の原因ではない。CMGTestの想定コードは`Pressure(Wire, unit_id, 20)`で、`unit_id=0x66`である。

LiPoPowerにも同種の引数ずれがある。

```cpp
// Trackersrc2: unit_id=0x61、sample=1 Hz
component::LiPoPower power(..., TEMP, unit_id, 1);

// CMG Tracker: unit_id=1、sampleは既定値
component::LiPoPower power(..., TEMP, 1);
```

### 4.4 platformio.ini / CI

主な変更は次のとおり。

- `env:rp2350`を追加
- `env:CMGTest_RP2350`、`env:CMGTest_Esp32`を追加
- `env:MissionBus`、`env:MissionBus_RP2350`を追加
- RP2040 platformを特定revisionへ変更
- Arduino BMI270/BMM150、MadgwickAHRS、ESP32Servo等を追加
- MCU別のsource filterを調整
- `.github/workflows/build_platformio.yaml`を追加
- `include_dir = src`を削除

同じソースでも環境、framework、MCU、依存ライブラリ版、source filterが変わるため、動作差はcomponentの直接差分だけでは決まらない。

## 5. hardware差分

hardwareは追加37、変更24、削除1である。

主な範囲：

- IMU2の基板・回路図を追加
- BMI270/BMM150のsymbol/footprintを追加
- CMGTestMk2の基板・回路図とサブ回路を追加
- MissionBusの基板・回路図を大幅更新し、IMU2、製造データを追加
- RCSの基板・回路図を更新し、IMU2、Pressure、製造データを追加
- TrackerのKiCad projectを変更
- MissionBusの旧`9DoF.kicad_sch`を削除

KiCadのネット、ピン割当、電源、プルアップ、I2C配線長などの電気的影響は、テキスト差分だけでは確定できない。CMGTestの1 MHz I2Cを評価する場合は、回路図確認、プルアップ値、配線容量、オシロスコープ/ロジアナによる立上り時間確認が必要である。

## 6. Pressureのコード差分

Pressure component自身の直接差分は1か所だけである。

```cpp
// Trackersrc2
Wire.begin();

// CMG
// Wire.begin();  // Wireはmain.cppのsetupで初期化されている前提
```

Pressureの測定・高度計算・パケット生成アルゴリズム、および同梱BME280ドライバーは両ブランチで同じである。

この変更の意味は、PressureがI2Cを再初期化せず、mainで設定されたピンとクロックをそのまま使うようになったことである。CMGTestでは次の設定が残る。

```cpp
#define I2C_SCL_PIN 5
#define I2C_SDA_PIN 4
#define I2C_freq 1000000

wobc::beginI2C(Wire, I2C_SDA_PIN, I2C_SCL_PIN, I2C_freq);
```

そのため、CMGTestのPressureは1 MHz I2Cで動く。Trackersrc2と同じ配線・センサーであっても、main側設定が異なれば信号品質とエラー率が変化する。

## 7. CMGTestでPressureだけでも不安定になる理由

### 7.1 現在の優先順位

| 優先度 | 原因候補 | 判定 | 根拠 |
|---|---|---|---|
| 高 | 1 MHz I2Cと実配線条件 | 未確定・有力 | CMGTest固有設定。Pressure側で`Wire.begin()`しなくなったため維持される |
| 高 | Pressure timer開始順序 | コード上の問題を確認 | `bme.begin()`・係数初期化より前にtimerを開始している |
| 高 | BME280 forced modeの完了待ち不足 | コード上の問題を確認 | 測定開始直後にdata registerを読む |
| 中 | Serial/USB/PC側のバッファリング | 発生可能 | SerialBusキュー、USB/OS buffer、`read_all()`の一括取得がある |
| 中 | `util.py`の長時間動作問題 | コード上の問題を確認 | 履歴無制限、raw data処理不備、例外握りつぶし |
| 低/除外 | IMUとのI2C競合 | 今回の必要条件ではない | IMU停止・Pressure単体でも再現 |
| 低/除外 | Heater/Logger等との競合 | Pressure単体条件では主因ではない | 対象componentを開始しなくても再現する前提 |
| 対象外 | Trackerの`unit_id=0` | CMGTestには非該当 | CMGTestは`unit_id=0x66` |

### 7.2 Pressure timerの初期化順序

現在の`Pressure::setup()`は次の順序である。

```cpp
void Pressure::setup() {
  start(sample_timer_);
  storeOnCommand('Q');
  while (!bme.begin()) {
    LOG("Could not find BME280");
    delay(1000);
  }
  initialize_pressure_data();
  initialize_coefficients();
}
```

CMGTestの20 Hz設定では、timer periodは50 msである。BME初期化や係数初期化が50 msを超えた場合、初期化途中のBME/高度計算データへcallbackがアクセスする競合が起こり得る。

推奨順序：

```cpp
void Pressure::setup() {
  storeOnCommand('Q');

  while (!bme.begin()) {
    LOG("Could not find BME280");
    delay(1000);
  }

  initialize_pressure_data();
  initialize_coefficients();
  start(sample_timer_);
}
```

この問題は両ブランチに共通するが、CMGTestの20 HzではTrackersrc2/Trackerの既定1 Hzより顕在化しやすい。

### 7.3 BME280 forced mode

BME280ドライバーの既定設定はforced modeである。`ReadData()`は次の処理を行う。

1. `WriteSettings()`でforced測定を開始
2. 測定完了を待たずに`ReadRegister()`でdata registerを読む

このため、直前の測定値、初回の未確定値、またはI2Cエラー時の`NAN`を読む可能性がある。

さらに次の弱点がある。

- register writeの`endTransmission()`結果が上位へ正しく伝わらない
- Pressure callbackはread成功/失敗を直接判定しない
- read失敗時に入る`NAN`を`int`へcastしてパケット化している
- `Pressure`は`TwoWire& wire`を受け取るが、BME280I2C実装の一部はglobal `Wire`を直接使用する

推奨対応：

- forced測定開始後にstatus registerの`measuring` bitをtimeout付きで待つ
- I2C write/read結果を上位まで返す
- read失敗時は前回正常値を維持するか、status fieldを付けて送る
- `NAN`を整数化する前に`std::isfinite()`で検査する
- 渡された`TwoWire`インスタンスをBMEドライバーでも一貫して使う

## 8. パケットが止まった後にまとめて表示される可能性

結論として、可能性はある。ただしBME280自体が複数サンプルを保存しているわけではない。滞留場所は次のいずれかである。

```text
Pressure callback
    -> Kernel / SerialBus queue（最大16パケット、満杯時は古いものを破棄）
    -> MCU USB/UART送信buffer
    -> PCのUSB/serial driver buffer
    -> pyserial read_all()
    -> util.py parse_packet()
    -> 10 Hzの画面更新
```

### 8.1 MCU側SerialBus

`SerialBus`は全パケットをqueue size 16、`force_push=true`でlistenする。

- Serial書込みが一時停止すると、最大16パケットまで待機し得る
- queueが満杯になると、最も古いパケットをpopして新しいものを入れる
- Serialが復帰すると、queueを`while`で連続送信する

したがって、復帰時に最大16パケット程度が連続してPCへ届く可能性がある。Pressure 20 Hzだけならqueue上の約0.8秒分に相当する。ほかのパケットも流れていればPressureの保持数はさらに少なくなる。USB/OS側bufferには別途それ以上が溜まる可能性がある。

### 8.2 FreeRTOS software timer

Pressure callbackはFreeRTOS software timerのcontextでBME280のI2C通信を行う。I2C処理がブロックするとtimer service自体が遅れる。

復帰後、期限を過ぎたtimer callbackが短時間に連続実行される可能性がある。この場合は「生成済みパケットが溜まっていた」のではなく、「遅れてcallbackが連続実行され、新しいパケットが短時間に生成された」状態である。

### 8.3 util.py

`util.py`には次の動作がある。

- `ser.read_all()`でPC側に到着済みのbytesをまとめて読む
- 1回の`parse_packet()`で複数のcomplete frameをまとめてdecodeする
- UI更新は10 Hzなので、Pressure 20 Hzなら正常時でも複数パケットが画面1更新分にまとまる
- `Packet.decode()`の例外を握りつぶして`None`を返すため、壊れたpacketが画面上で無言で消える
- `all_packets`へ全packetを無制限にappendするため、長時間動作でmemory使用量が増える
- `raw_data.extend(data)`が新規受信chunkではなく未処理残りを含む`data`全体を毎loop追加するため、保存用raw bufferが重複し、memory使用量が急増し得る
- parserがframe処理後に`size + 1` byteしか消費せず、末尾delimiterの`0`を次loopへ残す

`raw_data`の重複は表示packet自体を複製するわけではないが、長時間後のmemory圧迫とUI遅延につながり得る。

推奨修正イメージ：

```python
chunk = ser.read_all() or b''
data += chunk
raw_data.extend(chunk)
```

frame処理後はpacket本体、checksum、delimiterをすべて消費する。

```python
buf = buf[size + 2:]
```

加えて、履歴を`collections.deque(maxlen=...)`等で上限管理し、decode失敗数・CRC失敗数を画面またはlogへ出すことが望ましい。

## 9. `Ts`による滞留箇所の判定

Pressure packet内の`Ts=millis()`と、PC受信時刻を同時に記録すると判別しやすい。

| 観測 | 可能性が高い箇所 |
|---|---|
| 画面では一気に表示されるが、各`Ts`は約50 ms間隔 | MCU送信後のUSB/OS/util側buffering |
| 長い空白後、複数packetの`Ts`がほぼ同時刻 | timer/I2Cが止まり、復帰後にcallbackが連続実行 |
| `Ts`が大きく飛び、間のpacketがない | callback未実行、decode/CRC失敗、または16 packet queueでdrop |
| `Ts`は更新するがPR/TE/HUが同じ値のまま | forced modeで前回値を読んでいる可能性 |
| `Ts`もPressure packetも更新せず、ほかのpacketは更新 | Pressure timer/BME/I2C側 |
| 全packetが止まり、その後まとめて来る | SerialBus、USB、PC driver、util側 |

## 10. CMGTestで「Pressureだけ」にする際の注意

origin/feature/CMGのCMGTestでは、Pressureは初期状態でコメントアウトされている。

```cpp
// component::Pressure pressure(Wire, unit_id, 20);
// pressure.begin();
```

本調査では、これを有効化して試験している前提である。

また、ほかのsensorの`begin()`を止めても`main_.begin()`が残っていると、CMGTestの`Main::loop()`は約100 msごとにダミーの`'C'` telemetryを生成する。そのため厳密なPressure単体試験では、次も停止する必要がある。

```cpp
// main_.begin();
```

コンストラクタでhardwareを操作するcomponentがないかも別途確認する。

## 11. 推奨する段階的な切り分け

### Step 1: 最小構成

- PressureとSerialBusだけを開始
- `main_.begin()`、IMU、Heater、Logger、Servoを停止
- Pressureを1 Hz
- I2Cを400 kHz

```cpp
#define I2C_freq 400000
component::Pressure pressure(Wire, unit_id, 1);
```

### Step 2: 初期化順序修正

- `bme.begin()`成功後に高度用配列・係数を初期化
- 最後に`start(sample_timer_)`

### Step 3: 計測完了待ちとエラー可視化

- BME280の`measuring`待ちを追加
- I2C read/write error counterをpacketへ追加
- `NAN`をそのままint化しない
- callback開始/終了時刻、処理時間を記録

### Step 4: 周波数を段階的に上げる

次の順で各条件を数分以上確認する。

1. 400 kHz / 1 Hz
2. 400 kHz / 5 Hz
3. 400 kHz / 10 Hz
4. 400 kHz / 20 Hz
5. 必要な場合だけ1 MHz / 20 Hz

### Step 5: PC側を分離

- util画面とは別にraw serialを時刻付きで保存
- packet内`Ts`とPC受信時刻を比較
- decode失敗数、CRC失敗数、受信byte数を表示
- utilを再起動した直後だけ改善するか確認

## 12. 現時点の結論

1. CMGTestではTracker固有の`unit_id=0`問題は該当しない。
2. Pressure componentのブランチ差分は`Wire.begin()`削除だけであり、CMGTest mainの1 MHz I2C設定がそのまま適用される。
3. IMU停止・Pressure単体でも再現するため、IMU負荷は今回の必要条件ではない。
4. Pressure timerをBME初期化前に開始する順序と、forced modeの測定完了待ち不足は、コード上で確認できる問題である。
5. パケットが後からまとめて表示されることはあり得る。SerialBusの16 packet queue、USB/OS buffer、`read_all()`、10 Hz UI更新、timer callbackの遅延が候補となる。
6. `Ts`間隔とPC受信時刻を比較すれば、生成前のtimer遅延か、生成後の通信bufferingかを切り分けられる。
7. 最初に試す条件は「400 kHz、1 Hz、PressureとSerialBusのみ、timer開始を初期化後へ移動」である。

## 13. 未検証項目・追記欄

- [ ] 使用環境：`CMGTest_RP2350` / `CMGTest_Esp32`
- [ ] 使用BME280の型番・I2C address
- [ ] 実測I2Cクロック
- [ ] SDA/SCL pull-up抵抗値
- [ ] 配線長・接続台数
- [ ] 400 kHz / 1 Hzでの再現有無
- [ ] 400 kHz / 20 Hzでの再現有無
- [ ] 1 MHz / 20 Hzでの再現時間
- [ ] 停止前後のPressure `Ts`
- [ ] PC受信時刻との差
- [ ] SerialBus queue drop数
- [ ] BME read/write error数
- [ ] util.pyのmemory使用量推移

実験結果：

```text
日時:
環境:
コミット:
I2Cクロック:
Pressure周波数:
有効component:
再現までの時間:
停止中のTs:
復帰時のTs:
備考:
```

## 14. 全差分ファイル一覧

記号：`A=追加`、`M=変更`、`D=削除`。

```text
A .github/workflows/build_platformio.yaml
M hardware/components/IMU/IMU.kicad_pro
M hardware/components/IMU/IMU.kicad_sch
A hardware/components/IMU2/IMU2.kicad_pcb
A hardware/components/IMU2/IMU2.kicad_pro
A hardware/components/IMU2/IMU2.kicad_sch
A hardware/components/IMU2/fp-lib-table
A hardware/components/IMU2/sym-lib-table
M hardware/components/RCS/RCS.kicad_pro
A hardware/library/BMI270.kicad_sym
A hardware/library/BMM150.kicad_sym
A hardware/library/WOBCLibrary.pretty/Board_1U_60x70.kicad_mod
A hardware/library/WOBCLibrary.pretty/XDCR_BMI270.kicad_mod
A hardware/library/WOBCLibrary.pretty/XDCR_BMM150.kicad_mod
A hardware/modules/CMG/Test/CMGTestMk2/CMGTestMk2.kicad_pcb
A hardware/modules/CMG/Test/CMGTestMk2/CMGTestMk2.kicad_pro
A hardware/modules/CMG/Test/CMGTestMk2/CMGTestMk2.kicad_sch
A hardware/modules/CMG/Test/CMGTestMk2/HariboteDCDC.kicad_sch
A hardware/modules/CMG/Test/CMGTestMk2/LibraryforCMG.kicad_sym
A hardware/modules/CMG/Test/CMGTestMk2/MotorDriver.kicad_sch
A hardware/modules/CMG/Test/CMGTestMk2/Power.kicad_sch
A hardware/modules/CMG/Test/CMGTestMk2/Relay.kicad_sch
A hardware/modules/CMG/Test/CMGTestMk2/Servo.kicad_sch
A hardware/modules/CMG/Test/CMGTestMk2/sym-lib-table
A hardware/modules/CMG/Test/CMGTestMk2/untitled.kicad_sch
D hardware/modules/MissionBus/9DoF.kicad_sch
M hardware/modules/MissionBus/ESPCore.kicad_sch
M hardware/modules/MissionBus/GPS.kicad_sch
M hardware/modules/MissionBus/IF1.kicad_sch
M hardware/modules/MissionBus/IF2.kicad_sch
A hardware/modules/MissionBus/IMU2.kicad_sch
M hardware/modules/MissionBus/LiPo.kicad_sch
M hardware/modules/MissionBus/LoRa.kicad_sch
M hardware/modules/MissionBus/Logger.kicad_sch
M hardware/modules/MissionBus/MissionBus.kicad_pcb
M hardware/modules/MissionBus/MissionBus.kicad_pro
M hardware/modules/MissionBus/MissionBus.kicad_sch
M hardware/modules/MissionBus/Pressure.kicad_sch
A hardware/modules/MissionBus/fabrication-toolkit-options.json
A hardware/modules/MissionBus/fp-lib-table
A hardware/modules/MissionBus/production/MissionBus.zip
A hardware/modules/MissionBus/production/backups/MissionBus_2025-08-20_20-09-14.zip
A hardware/modules/MissionBus/production/backups/MissionBus_2025-08-22_20-47-38.zip
A hardware/modules/MissionBus/production/backups/MissionBus_2025-08-22_20-53-36.zip
A hardware/modules/MissionBus/production/netlist.ipc
M hardware/modules/RCS/IF.kicad_sch
A hardware/modules/RCS/IMU2.kicad_sch
M hardware/modules/RCS/LoadCell1.kicad_sch
M hardware/modules/RCS/LoadCell2.kicad_sch
A hardware/modules/RCS/Pressure.kicad_sch
M hardware/modules/RCS/RCS.kicad_pcb
M hardware/modules/RCS/RCS.kicad_pro
M hardware/modules/RCS/RCS.kicad_sch
M hardware/modules/RCS/RCSComp.kicad_sch
M hardware/modules/RCS/RP2040.kicad_sch
M hardware/modules/RCS/Tank1.kicad_sch
A hardware/modules/RCS/fabrication-toolkit-options.json
A hardware/modules/RCS/fp-lib-table
A hardware/modules/RCS/production/RCS.zip
A hardware/modules/RCS/production/backups/RCS_2025-08-06_17-51-13.zip
A hardware/modules/RCS/production/netlist.ipc
A hardware/modules/RCS/sym-lib-table
M hardware/modules/Tracker/Tracker.kicad_pro
M platformio.ini
A src/components/Attitude/KalmanFilter/KalmanAttitude.cpp
A src/components/Attitude/KalmanFilter/KalmanAttitude.h
A src/components/Attitude/MadgwickFilter/MadgwickAttitude.cpp
A src/components/Attitude/MadgwickFilter/MadgwickAttitude.h
M src/components/GPS/gps.cpp
M src/components/GPS/gps.h
A src/components/Heater/Heater.cpp
A src/components/Heater/Heater.h
A src/components/IMU/IMU.cpp
A src/components/IMU/IMU.h
A src/components/IMU/src/ICM42688/ICM42688.cpp
A src/components/IMU/src/ICM42688/ICM42688.h
A src/components/IMU/src/ICM42688/registers.h
A src/components/IMU/src/Kalmanfilter/KalmanFilter.cpp
A src/components/IMU/src/Kalmanfilter/KalmanFilter.h
A src/components/IMU/src/MMC5603/MMC5603.cpp
A src/components/IMU/src/MMC5603/MMC5603.h
M src/components/LiPoPower/lipo_power.cpp
M src/components/LoRa/e220.cpp
M src/components/LoRa/rplora.cpp
M src/components/Logger/logger.cpp
M src/components/Logger/logger.h
M src/components/Pressure/pressure.cpp
A src/components/Servo/LogServo.cpp
A src/components/Servo/LogServo.h
A src/components/Tachometer/Tachometer.cpp
A src/components/Tachometer/Tachometer.hpp
M src/components/Telemeter/telemeter.cpp
A src/components/breakout/CMG_Test/BWCheck/BWCheck.ino
A src/components/breakout/CMG_Test/micropython/flash_nuke.uf2
A src/components/breakout/CMG_Test/micropython/tachometer/tachometer.py
A src/idf_component.yml
M src/library/common.h
M src/library/core/can_bus.cpp
M src/library/driver/can_if.h
M src/library/driver/rp2040/can.cpp
M src/library/driver/rp2040/can.h
A src/library/driver/rp2350/can.cpp
A src/library/driver/rp2350/can.h
A src/modules/CMGTest/main.cpp
M src/modules/GS/main.cpp
M src/modules/LoRa/main.cpp
A src/modules/MissionBus/main.cpp
M src/modules/Tracker/main.cpp
```

## 15. 更新履歴

| 日付 | 内容 |
|---|---|
| 2026-08-20 | origin同士の全フォルダ差分、Pressure/CMGTest/util.py調査を初版として整理 |
