# ESP32-S3 LoRa 2系統通信の実装・検証まとめ（旧実装）

> この文書はコンポーネント化前の`modules/LoRa_ESP`実装に関する履歴である。現行の構成と実機試験結果は[20260912_lora_esp_dual_lora_test_log.md](20260912_lora_esp_dual_lora_test_log.md)を参照する。

作業日：2026年9月  
対象：WOBCのLoRa ESP基板（ESP32-S3、E220 LoRaモジュール2台）
作業者：Aoyama

---

## 1. 目的

GS基板・Separation基板を使用せず、1枚のLoRa ESP基板上にあるLoRa1とLoRa2を使って、Ground基板とFlight基板の2系統双方向通信を構成する。

今回の役割は次の通りとした。

```text
アップリンク（Ground → Flight）
PC → Ground ESP → LoRa1 → 無線 → Flight LoRa1 → Flight ESP

ダウンリンク（Flight → Ground）
Flight ESP → LoRa2 → 無線 → Ground LoRa2 → Ground ESP → PC
```

LoRa1をアップリンク、LoRa2をダウンリンクに割り当て、各E220を別々のUARTコントローラで動作させることを目標とした。

## 2. 対象GPIOとUART方針

| 無線 | E220 UART接続 | AUX | M0 | M1 | RFスイッチ |
|---|---:|---:|---:|---:|---|
| LoRa1 / U301 | RX=GPIO12、TX=GPIO13 | GPIO11 | GPIO14 | GPIO21 | GPIO39/40 |
| LoRa2 / U401 | RX=GPIO18、TX=GPIO7 | GPIO8 | GPIO5 | GPIO6 | GPIO9/10 |

UART番号とGPIO番号は別の概念であるため、`HardwareSerial(n)`と
`serial.begin(baud, SERIAL_8N1, rx, tx)`を組み合わせて、基板配線GPIOへ明示的に接続する。

最終的な検証方針は、USBコンソールとの競合を避けるためUART0を予約し、LoRa1をUART2、LoRa2を実績のあるUART1へ割り当てる構成である。

## 3. 実装した内容

### 3.1 LoRa ESP専用のGround/Flightファームウェア

`src/modules/LoRa_ESP/main.cpp`に、GroundとFlightをビルドフラグで切り替える2系統制御を実装した。

- `LORA_ESP_ROLE_GROUND`：Ground側。PCからのcommandをLoRa1へ送信し、LoRa2を受信。
- `LORA_ESP_ROLE_FLIGHT`：Flight側。LoRa1を受信し、LoRa2からtelemetryを送信。
- LoRa1とLoRa2を別々の`HardwareSerial`およびE220インスタンスで管理。
- 一方の無線初期化に失敗しても、もう一方を診断できるようにした。

### 3.2 E220初期化と設定

E220の設定モードへ移行し、レジスタ読出し後に次の設定を行う。

- UART速度：115200bps
- データレート：SF9 / BW125kHz
- RSSI取得：有効
- 送信モード：Transparent
- モジュールアドレス：Broadcast
- LoRa1チャンネル：10
- LoRa2チャンネル：3

設定後はESP側UARTも115200bpsへ切り替える。E220は設定済みUART速度を保持するため、起動時には9600、115200、その他の対応速度を順に読出し診断する。

### 3.3 アップリンク診断ログ

Ground側の送信直前・送信結果、Flight側の受信・CRC・packet復元を記録する。

```text
[UPLINK TX] ...
[E220 TX] bytes=...
[UPLINK RX] bytes=...
[UPLINK RX] crc_received=... crc_calculated=... CRC=OK/NG
[UPLINK PACKET] ...
```

送信対象外の場合は`[UPLINK DROP]`を出力し、Groundのpacket routingで止まった場合と無線区間で止まった場合を区別できるようにした。

### 3.4 UART単体診断環境

`platformio.ini`に次の環境を追加した。

| 環境 | 用途 |
|---|---|
| `LoRa_ESP_Ground` | Ground通常版（LoRa1=UART2、LoRa2=UART1） |
| `LoRa_ESP_Flight` | Flight通常版（LoRa1=UART2、LoRa2=UART1） |
| `LoRa_ESP_LoRa1_UART2_Diag` | LoRa1のみをUART2で診断。LoRa2は無効 |
| `LoRa_ESP_LoRa1_UART1_Diag` | LoRa1をUART1で動かす比較試験 |
| `LoRa_ESP_LoRa2_UART1_Diag` | LoRa2のみをUART1で診断 |
| `LoRa_ESP_LoRa2_UART0_Diag` | LoRa2をUART0で診断 |

UART2診断では、UARTの実接続先を次の形式で表示する。

```text
ROLE=GROUND LoRa1=UPLINK(ch10,UART2) \
rx=12 tx=13 attached_rx=12 attached_tx=13 bring-up=CONFIG_READ
```

`attached_rx`および`attached_tx`はArduino-ESP32のUART GPIOマトリクス設定を読み出した値である。

## 4. 実施した試験と結果

### 4.1 Ground・Flight間のアップリンク

PCからGround ESPへWCPP commandを送り、Ground側の入力までは確認できた。

UART1を使用した比較試験では、Flight側で次のpacketを確認できた。

```text
type=command
packet id=0x74 (t)
origin=0x63
destination=0x61
sequence=1
Ts=UPLINK_TEST
Ss=-77 付近
```

この結果から、LoRa1 E220、アンテナ、RF経路、WCPP packet復元は、UART1を使用した場合には動作することを確認した。

### 4.2 LoRa2 UART1

LoRa2をUART1で単独初期化した場合は、`bring-up=OK`を確認できた。LoRa2モジュールが未実装・未接続という状況ではない。

### 4.3 UART0/UART2比較

| 対象 | UART0 | UART1 | UART2 |
|---|---:|---:|---:|
| LoRa1 | 失敗 | 成功 | 失敗 |
| LoRa2 | 失敗 | 成功 | 失敗 |

UART2診断では、次のログを確認した。

```text
UART2 probe v2
E220_CONFIG probe baud=... result=NG
ROLE=GROUND LoRa1=UPLINK(ch10,UART2)
rx=12 tx=13 attached_rx=12 attached_tx=13
bring-up=CONFIG_READ
```

一部の速度で11バイトを受信した場合もあったが、内容は`00 00 00 00 FE...`などで、E220の正常応答である`C1 00 08 ...`ではなかった。

## 5. できたこと

- Ground/Flightの役割別ファームウェアを追加した。
- LoRa1とLoRa2を別E220インスタンスとして扱える構造にした。
- LoRa1のUART1比較試験で、Ground→Flightアップリンクを確認した。
- LoRa2のUART1単体初期化に成功した。
- PC→Ground ESPまでのWCPP packet受信を確認した。
- Ground送信、Flight受信、CRC、packet解析の診断ログを追加した。
- UART2のGPIOマトリクス割当てをソフトウェアで確認できるようにした。

## 6. できていないこと

- LoRa1をUART2でE220初期化すること。
- Ground/Flight双方でLoRa1=UART2、LoRa2=UART1の2系統同時動作を確認すること。
- LoRa2を使ったダウンリンク実通信を確認すること。
- UART2使用時のE220正常応答（`C1 00 08 ...`）を受信すること。

UART2のログではGPIOマトリクス自体は`rx=12/tx=13`として正しく設定されているが、E220の有効な設定応答がなく`CONFIG_READ`で停止している。このため、問題は無線チャンネルやアンテナより前段のUART2とE220間の通信に残っている。

## 7. 現時点の判断

書込み失敗や診断環境の選択間違いは、`UART2 probe v2`、全ボーレート探索、`attached_rx=12 attached_tx=13`のログにより否定できる。

UART1では同じLoRa1が成功しているため、E220本体・アンテナ・LoRa1の基本RF経路が完全に使用不能という状態ではない。一方、UART2ではGPIO割当てが正しいにもかかわらずE220応答がないため、次のいずれかが残る。

- UART2 TX信号が実際にGPIO13からE220 RXDへ届いていない
- E220 TXDの応答がGPIO12へ戻っていない
- UART2使用時の信号レベル・波形・タイミングの問題
- E220のM0/M1、電源、リセット、またはUART端子側の電気的問題

GPIO12/13をE220接続中に単純短絡するループバックは、E220 TX出力とESP32 TX出力の衝突を起こす可能性があるため実施しない。次の確認は、UART2 TX/RX波形の測定、またはE220 TXを切り離した安全なループバック試験とする。

## 8. 使用コマンド

通常版のビルド：

```powershell
pio run -e LoRa_ESP_Ground -e LoRa_ESP_Flight
```

LoRa1 UART2単体診断：

```powershell
pio run -e LoRa_ESP_LoRa1_UART2_Diag
```

`util.py`のモニタでは、GroundをCOM14、FlightをCOM15、速度115200bpsで接続する。起動後は`0x23`のログpacketを選択し、`ROLE=...`、`E220_CONFIG`、`bring-up=...`を確認する。

## 9. 次の作業

1. UART2診断ファームウェアで`attached_rx/attached_tx`を記録する。
2. UART2 TX/RXの電圧レベル、アイドル電位、設定コマンド波形、E220応答波形を測定する。
3. 必要ならE220のTX線を分離した安全なループバック試験を行う。
4. UART2が電気的に正常と確認できた後、Ground/Flight通常版へ戻してLoRa2ダウンリンクを試験する。
