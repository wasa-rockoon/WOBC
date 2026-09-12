# LoRa_ESP: ESP32-S3 LoRa 2系統通信の作業・試験ログ

作業日：2026年9月12日  
対象：WOBC LoRa ESP基板（ESP32-S3、E220×2）  
作業者：shoko / Codex

---

## 目標

`docs/library.md` の構成に従い、`modules/LoRa_ESP/main.cpp` に集約されていた
LoRa通信処理をコンポーネントへ移す。新しい `modules/LoRa_ESP/main.cpp` は、
ピン・UART・チャンネル・ユニットIDの設定とコンポーネントの起動だけを担当する。

その上で、2枚のLoRa ESP基板をGroundとFlightとして使用し、以下の2系統の無線通信を
実機で確認する。

```text
アップリンク: PC -> Ground ESP -> LoRa1 -> Flight LoRa1 -> Flight ESP -> PC
ダウンリンク: PC -> Flight ESP -> LoRa2 -> Ground LoRa2 -> Ground ESP -> PC
```

## 方針と構成

| 経路 | 送信側 | 受信側 | 無線 | UART | チャンネル |
|---|---|---|---|---:|---:|
| アップリンク | Ground | Flight | LoRa1 | UART2 | 10 |
| ダウンリンク | Flight | Ground | LoRa2 | UART1 | 3 |

UART0はUSBシリアルコンソールに予約する。Groundのunit IDは`0x63`、Flightのunit IDは`0x61`とした。

## 変更内容

### モジュール

`src/modules/LoRa_ESP/main.cpp` を、以下だけを担当する構成にした。

- LoRa1/LoRa2のGPIO、UART番号、チャンネルの定義
- Ground/Flightごとのunit IDと役割の選択
- `SerialBus`、LEDインジケータ、`component::DualLoRa` の生成と起動
- 任意のCAN起動（`LORA_USE_CAN`が1の場合のみ）

`MyComp` は`library.md`内の例にある仮のコンポーネント名で、実装が存在しないため使用していない。
テンプレート中の`Main`コンポーネントも、今回のモジュール固有のパケット処理がないため作成していない。

### コンポーネント

`src/components/LoRa/dual_lora.h/.cpp` を追加した。`component::DualLoRa` は、元の
`LoRa_ESP/main.cpp` の通常2系統通信をコンポーネントとして分離したもの。

- E220の設定モード移行、設定読出し、通信条件の設定、通常モード復帰
- LoRa1/LoRa2用の独立した`HardwareSerial`とE220インスタンスの管理
- GroundではcommandをLoRa1から送信し、LoRa2から受信
- FlightではLoRa1から受信し、packet ID `'M'` のtelemetryをLoRa2から送信
- 受信フレームの長さとCRCを検証
- 受信パケットに`Ss`（RSSI）を追加してSerialBusへ送信
- `Ss`をすでに含むパケットは無線へ再送しない

既存の`components/LoRa/e220.h/.cpp`はE220ドライバとしてそのまま共用した。
既存の`component::LoRa`は単一無線用の送信キューとTracker向け処理を持つため変更していない。

### PlatformIO環境

`platformio.ini` に以下を追加した。

| 環境 | 用途 |
|---|---|
| `LoRa_ESP_Ground` | Ground通常版 |
| `LoRa_ESP_Flight` | Flight通常版 |
| `LoRa_ESP_LoRa1_UART1_Diag` | LoRa1のみをUART1で初期化する比較試験 |
| `LoRa_ESP_LoRa1_UART2_Diag` | LoRa1のみをUART2で初期化する比較試験 |

診断環境では`DualLoRa`の第4引数に`false`を渡してLoRa2を停止し、LoRa1のUART条件だけを比較できるようにした。

## 初期化失敗の診断と修正

最初はE220のUART速度を115200bpsに変更する構成だったが、LoRa2は`DATA_RATE`で失敗した。
9600bpsのまま後続設定を実行する構成へ変更すると、LoRa2の初期化は成功した。

その後、LoRa1はUART1なら`bring-up=OK`、UART2では`CONFIG_READ`となった。2枚の基板で同じ結果を確認した。

UART2失敗時の応答ログには、設定読出し応答の前に余分な`C0`が残っていた。

```text
E220 response baud=9600 length=11 bytes=C0 C1 00 08 FF FF 70 20 0A 83 00
```

設定読出し要求の直前に受信バッファを確認すると、`C0`が1バイト残っていることを確認した。

```text
PRE_RX baud=9600 buffered=1 drained=1 first=C0 00 00 00
```

そこで通常版・診断版の両方で、各UART速度の設定後に20ms待機し、設定読出し要求を送る前に受信バッファを最大512バイト取り除く処理を追加した。これにより、UART2のLoRa1も`bring-up=OK`となった。

この処理はE220設定中だけに実行される。通常の無線受信処理で受信パケットを破棄するものではない。

## 実機試験結果

通常版をGround（COM14）とFlight（COM15）へ書き込み、両基板で次を確認した。

```text
LoRa1=UPLINK(ch10,UART2) bring-up=OK
LoRa2=DOWNLINK(ch3,UART1) bring-up=OK
```

### ダウンリンク

FlightからGroundへ、LoRa2/ch3でtelemetryを送信した。

```text
origin id: 0x61
dest id:   0x63
sequence:  3
Ts:        DOWNLINK_DUAL_OK
Ss:        -89
```

Groundが`Ts`と`Ss`を受信したため、LoRa1を同時起動した通常版でもダウンリンクが動作した。

### アップリンク

GroundからFlightへ、LoRa1/ch10でcommandを送信した。

```text
origin id: 0x63
dest id:   0x61
sequence:  1
Ts:        UPLINK_DUAL_OK
Ss:        -107
```

Flightが`Ts`と`Ss`を受信したため、アップリンクも動作した。

## できたこと

- 通信の詳細処理を`DualLoRa`コンポーネントへ移し、`LoRa_ESP/main.cpp`をモジュール設定・起動中心に整理した。
- LoRa1/UART2とLoRa2/UART1を同時に初期化できた。
- Ground -> FlightのLoRa1アップリンクを実機で確認した。
- Flight -> GroundのLoRa2ダウンリンクを実機で確認した。
- 両方向で、受信側がRSSIの`Ss`を追加したパケットを確認した。

## できなかったこと・未確認事項

- 長時間連続送受信、パケット損失率、距離・遮蔽物を変えた通信品質試験は未実施。
- 再起動を繰り返した場合に、常に`PRE_RX`で残留データを除去して初期化できるかは未確認。
- 起動時に余分な`C0`が残る根本原因は未特定。現状は設定要求の前に除去して回避している。
- CANを有効にした構成は未試験。
- 既存の`modules/LoRa_ESP/main.cpp`および既存の単一UART診断環境は変更していない。

## ビルドコマンド

```powershell
pio run -e LoRa_ESP_Ground
pio run -e LoRa_ESP_Flight
```

書き込み時は`-t upload --upload-port COMxx`を追加する。書き込み対象のCOMポートを使用中のモニタは、事前に終了する。
