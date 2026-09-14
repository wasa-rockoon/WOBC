# PC ↔ Separation 双方向通信 実装・実機試験ログ

作業日：2026年9月13日～14日  
対象：WOBC（GS、Ground LoRa、Flight LoRa、Separation）  
作業者：shoko (Codex)

---

## 目標

PCからSeparationへ安全なコマンドを送り、Separationが生成したACKをPCまで返す、次の双方向経路を構築して実機で確認する。

```text
アップリンク:
PC → USB → GS → CAN → Ground LoRa → E220 LoRa1
   → Flight LoRa → CAN → Separation

ダウンリンク:
PC ← USB ← GS ← CAN ← Ground LoRa ← E220 LoRa2
   ← Flight LoRa ← CAN ← Separation ACK
```

試験コマンドはSeparation component `0x00` の通常LEDコマンド `'n'` とし、誤作動を避けるため最終確認まで `On=0` を使用する。分離出力GPIO47/48を作動させる試験は対象外とした。

---

## 使用構成

| 位置 | 基板・役割 | PlatformIO環境 | module ID | unit ID |
|---|---|---|---:|---:|
| PC側 | GS | `GS` | `0x47` | `0x64` |
| 地上側 | Ground LoRa | `LoRa_ESP_CAN_Ground` | `0x4C` | `0x64` |
| 飛行側 | Flight LoRa | `LoRa_ESP_CAN_Flight` | `0x4C` | `0x41` |
| 飛行側 | Separation | `Separation` | `0x53` | `0x41` |

CANは各側でCAN H、CAN L、GNDを接続した。VDDは接続せず、各基板を個別に給電した。E220にはアンテナを接続した。

LoRaの割り当ては次のとおり。

| 方向 | 無線 | UART | channel |
|---|---|---:|---:|
| Ground → Flight（アップリンク） | LoRa1 | UART2 | 10 |
| Flight → Ground（ダウンリンク） | LoRa2 | UART1 | 3 |

---

## 変更内容

### GSをCAN中継モジュールとして整理

`src/modules/GS/main.cpp` から旧単一LoRa componentと独自ラップ処理を外し、`SerialBus`と`CANBus`による汎用転送へ整理した。

PCからUSB SerialBusへ入ったWCPPパケットは、GS内部のkernelを経由してCANへ転送される。逆方向にCANから来たパケットはSerialBusを経由してPCへ返る。

### CANのcommand/telemetry種別を保持

`src/library/core/can_bus.cpp` のCAN ID生成に `packet.type_and_id()` を使用し、packet IDだけでなくcommand/telemetryビットもCAN上で保持するようにした。

診断中はTX/RXしたWCPPヘッダーを確認する一時的なCANトレースも使用した。診断完了後、通常コードを複雑にしないため削除した。

### CAN物理層診断環境を追加

切り分け中は一時的なraw CAN診断モジュールと以下の環境を使用した。

- `GS_CAN_Raw_Diagnostic`
- `LoRa_ESP_CAN_Ground_Raw_Diagnostic`
- `GS_CAN_Diagnostic`
- `LoRa_ESP_CAN_Ground_Diagnostic`

raw診断ではGSから拡張CANフレームを1秒ごとに送信し、Ground LoRa側で `RAW_RX`、拡張ID、データ、TWAI状態、TEC/RECなどを確認できるようにした。

これら4環境とraw診断モジュールは、原因特定と通常版での往復通信成功後に削除した。

### BUS-OFFからの自動復旧

`src/library/driver/esp32/can.cpp` で次を有効にした。

```cpp
CAN0.setForceRecovery(true, 2000);
```

相手ノードが停止している状態で送信側がBUS-OFFになっても、2秒後にTWAIドライバを停止・再インストール・再始動する。相手を後から起動した場合に、送信側を手動リセットせず復帰できる構成とした。

### CAN側unit IDを統一

同じCANセグメントに接続するモジュールは同じunit IDを使うようにした。

- Ground LoRa：`0x63` → GSと同じ `0x64`
- Flight LoRa：`0x61` → Separationと同じ `0x41`

これにより、異なる `Iu` のHeartbeatを同一CAN上で受信した際の `Kernel::sendPacket()` assertionを解消した。

### リモートコマンド生成対応

`src/library/wcpp/python/send_command.py` に以下を指定できるようにした。

- `--origin-unit-id`
- `--dest-unit-id`
- `--sequence`
- `--command-path`

これによりPCから、送信元 `0x64`、宛先 `0x41` のremote commandを明示的に生成できるようにした。

### LoRa受信・再送防止

`src/components/LoRa/dual_lora.cpp/.h` で受信フレームの長さとCRCを検証し、受信RSSIを `Ss` として追加した。

`Ss` を持つパケットは「無線を一度通過したパケット」として扱い、同じ方向へ再送しないことでループを防止した。

### ACKダウンリンク

Flight LoRaへACK専用listenerを追加した。

- Separation ACK：telemetry packet `'a'` (`0x61`)
- 既存ミッションテレメトリ：packet `'M'` (`0x4D`)

ACKを通常テレメトリより優先してLoRa2から送信する。送信時には `[DOWNLINK TX]` とE220 UART書き込み結果をログへ出すようにした。

Ground LoRaがACKを受信すると `Ss` を追加し、CAN → GS → USBの既存経路でPCへ返す。

---

## 切り分け経緯

### 1. PCで生成したパケットが両基板のTXに見えた

複数の `util.py` を同じ作業ディレクトリから起動すると、同じ `.command` を各USB接続が読み込むため、GSとGround LoRaの両方へ直接コマンドが注入されていた。

監視ディレクトリを分離し、コマンドの出力先を `--command-path` でGS用 `.command` に固定した。

```text
.lora-test/ground/.command        PCコマンド注入先（GS）
.lora-test/lora-can-observe/      Ground LoRa監視
.lora-test/flight-observe/        Flight LoRa監視
```

### 2. 診断表示はTXだがGroundでRXにならなかった

ESP32_CANの `sendFrame()` は内部の `twai_transmit()` の成否にかかわらずtrueを返すため、アプリケーションのTX表示だけでは物理送信成功を証明できなかった。

raw CAN診断を追加した結果、初回は次の状態だった。

```text
GS:     TX_REQ増加、RX=0、TWAI state=0、bus_errors=7555
Ground: RX=0、TWAI state=1
```

GSがGroundより先に送信を開始してBUS-OFFとなり、通常のTWAI recovery完了後にSTOPPEDのまま残っていたことが原因だった。

Groundを先に起動してGSをリセットすると、Groundで次を連続受信した。

```text
RAW_RX id=18FF6413 ext=1 len=8
data=47 53 43 41 4E 00 00 13
```

`47 53 43 41 4E` はASCIIで `GSCAN`。RX/EXTも25から75まで連続して増え、CAN物理通信の成立を確認した。この結果を受け、通常版へBUS-OFF自動復旧を追加した。

### 3. 通常版でassert・再起動ループ

Ground側で `Iu=0x63`、GS側で `Iu=0x64` のHeartbeatが同一CAN上を流れ、次のassertが発生した。

```text
assert failed: kernel.cpp:82 ((*e).getInt() == unit_id())
```

Ground側を `0x64`、Flight側を `0x41` に変更した。変更後は同じCAN上のHeartbeatがそれぞれ同じ `Iu` となり、assertは解消した。

### 4. PlatformIO monitorで文字化けした

通常版のUSB SerialBusはWCPPバイナリを出力するため、テキスト用の `pio device monitor` では文字化けして見える。これは通信異常ではない。

以後の確認には `src/library/wcpp/python/util.py` を使用した。

### 5. Flight側USBにACKが見えない場合があった

Flight LoRaのUSB表示にはPressure、GPS、FlightPin、`'M'`など多くのパケットが流れるため、単発ACKが表示用SerialBusキューに残らない場合があった。

最終的にはPC側COM9で、Ground LoRaが付加したRSSI `Ss=-72` を含むACKを受信した。Flight LoRaがACKを受け取ってLoRa2へ送信しなければこのパケットはPCへ到達しないため、COM9の受信結果をEnd-to-End成功の判定に使用した。

---

## 実機試験

### 試験コマンド

```powershell
$repo = "C:\Users\shoko\waseda\sa-kuru\WASA\WOBC"

& "$repo\.venv\Scripts\python.exe" `
  "$repo\src\library\wcpp\python\send_command.py" `
  --packet-id n `
  --component-id 0x00 `
  --origin-unit-id 0x64 `
  --dest-unit-id 0x41 `
  --sequence 4 `
  --entry-name On `
  --data 0 `
  --data-type int `
  --command-path "$repo\.lora-test\ground\.command"
```

### アップリンク結果

Ground LoRaで次を確認した。

```text
[UPLINK TX] type=command packet_id=0x6E component=0x00
origin=0x64 destination=0x41 sequence=1 payload_size=2
[E220 TX] uart_write=OK
```

Flight側およびSeparation（COM17）で次のcommandを確認した。

```text
type:         command
packet id:    0x6e (n)
component id: 0x00
origin id:    0x64
dest id:      0x41
sequence:     2
On:           0
Ss:           -70
```

`Ss=-70` が付いているため、USBやCANだけではなくE220アップリンクを実際に通過したことを確認できた。

### Separationの実行結果

SeparationがACKを生成し、Flight側CANまで到達した。

```text
packet id:    0x61 (a)
origin id:    0x41
dest id:      0x64
sequence:     2
Ri:           110
Sq:           2
St:           1
Dp:           0
```

値の意味は次のとおり。

| Entry | 値 | 意味 |
|---|---:|---|
| `Ri` | 110 | 受信command ID `'n'` |
| `Sq` | 2 | 応答対象sequence |
| `St` | 1 | executed（実行成功） |
| `Dp` | 0 | duplicateではない |

### PCまでのACKダウンリンク結果

ACKダウンリンク実装後、PC接続のGS（COM9）でsequence 4のACKを確認した。

```text
type:         telemetry
packet id:    0x61 (a)
component id: 0x00
origin id:    0x41
dest id:      0x64
sequence:     4
Ri:           110
Sq:           4
St:           1
Dp:           0
Ss:           -72
```

`Ss=-72` はGround LoRaがダウンリンク受信時に付加したRSSIである。これにより、ACKがFlight LoRa → E220 LoRa2 → Ground LoRa → CAN → GS → PCを通過したことを確認した。

参考として、同じCOM9でFlight側のpacket `'M'` と `Ss=-86` も受信し、一般テレメトリのダウンリンク経路も継続動作していることを確認した。

---

## できたこと

- PCから生成したremote commandをGSのUSBへ注入できた。
- GS → Ground LoRa間のCAN物理通信とWCPP転送を確認できた。
- Ground LoRa → Flight LoRaのE220アップリンクを確認できた。
- Flight LoRa → Separation間のCAN通信を確認できた。
- Separationが宛先、型、component ID、command ID、payloadを検証して通常LEDコマンドを実行した。
- Separationが実行結果、sequence、重複状態を含むACKを生成した。
- ACKをFlight LoRaからGround LoRaへダウンリンクし、GS経由でPCまで返せた。
- アップリンク `Ss=-70`、ダウンリンク `Ss=-72` を確認した。
- CANの起動順序に依存しないBUS-OFF自動復旧を通常ドライバへ追加した。
- 地上側 `0x64`、飛行側 `0x41` のunit ID構成でHeartbeatと通常通信が安定した。

最終結果として、PC ↔ Separationの双方向End-to-End通信が実機で成立した。

---

## できなかったこと・未確認事項

- 長時間連続運用時のパケット損失率、再送、遅延は未測定。
- 遠距離、遮蔽物、高高度環境でのRSSI・通信品質は未試験。
- BUS-OFF後の強制再始動ログは確認したが、あらゆる電源投入順・抜き差し条件での反復耐久試験は未実施。
- Flight側USB表示では単発ACKが見えない場合がある。End-to-End判定はPC側COM9のACK受信で行った。
- `On=1` による通常LED点灯確認は今回の最終試験では実施していない。
- GPIO47/48の分離出力作動試験は安全上の理由から実施していない。現在の `'n'` commandは通常LEDのみを操作する。
- ACK消失時のPC側タイムアウト、再送ポリシー、sequence管理の自動化は未実装。
- 一時的なCAN診断環境とraw診断モジュールは通常コードから削除済み。診断結果は本ログに残した。

---

## 通常運用時の書き込み環境

```powershell
# GS
pio run -e GS -t upload --upload-port COM9

# Ground LoRa
pio run -e LoRa_ESP_CAN_Ground -t upload --upload-port COM16

# Flight LoRa
pio run -e LoRa_ESP_CAN_Flight -t upload --upload-port COM15

# Separation（COM番号は接続環境に合わせる）
pio run -e Separation -t upload --upload-port COMxx
```

通常版USBの表示にはWCPPバイナリが含まれるため、観測には `util.py` を使用する。複数基板を同時に監視する場合は基板ごとに作業ディレクトリを分け、PCコマンドは `--command-path` でGS用 `.command` に明示的に書き込む。
