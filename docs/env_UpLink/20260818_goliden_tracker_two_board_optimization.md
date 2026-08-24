# GOLIDEN/Tracker 2基板構成最適化（RP2040中継依存整理）

作業者：Aoyama (GitHub Copilot)

---

## 目標

WOBCプロジェクトを「GS&LoRa一体型基板（GOLIDEN）」と「Tracker基板」の2基板構成に最適化し、
GOLIDEN単体で LoRa送受信と SerialBus送受信が完結するように整理する。

---

## 変更ファイル

- `src/modules/GOLIDEN/main.cpp`
- `src/modules/Tracker/main.cpp`
- `platformio.ini`
- `src/library/wcpp/__init__.py`（新規追加）

---

## 実施内容

### 1) GOLIDENメイン処理の中継依存整理

`src/modules/GOLIDEN/main.cpp` にて以下を整理。

- 未使用だった `HardwareSerial lora_serial(1);` を削除。
- `heartbeat_.component(0x4D)`（旧MissionBus由来ID）と `heartbeat_` リスナー自体を削除。
- 送信リスナーを `tx_listener_` に集約し、`listen(tx_listener_, 8)` でカーネル全体を監視。
- LoRa再送ループ防止フィルタを追加。
  - 既に LoRa 送信コマンド (`component_id == LoRa` かつ `packet_id == 's'`) のパケットは再ラップしない。
  - LoRa受信由来マーカー `"Ss"` を持つパケットは再ラップしない。
  - 既存仕様を維持し `"Im"` を持つパケットもLoRa送信対象から除外。
- LoRa送信用ラップパケットの確保サイズを固定 `64` から `packet.size() + 32` に変更。
- `sendPacket(lorapacket, tx_listener_)` を使用し、自分のリスナーへの即時再流入を抑制。

### 2) default_envs の2基板構成化

`platformio.ini` の `default_envs` を次の通り変更。

- 変更前: `GS, Tracker`
- 変更後: `GOLIDEN, Tracker`

### 3) Python WCPP ブリッジのimport修正

`src/library/wcpp` が `namespace package` として解決され、`Packet` と `frame_packet` の import が失敗していたため、
`src/library/wcpp/__init__.py` を追加して package root を regular package として登録した。

これにより、以下が正常に起動する。

```powershell
.\.venv\Scripts\python.exe src/library/wcpp/python/util.py --help
```

また、`util.py` は直接実行時に `from transport import frame_packet` のような相対 import を含むため、
`pip install -e src/library/wcpp` を行い、`wcpp` package が利用可能な状態にしておくことが前提である。

### 4) Tracker のACK無限ループ修正

Tracker の `uplink_listener_` は購読条件なしで全パケットを受信していたため、Tracker自身が生成した
LoRa送信コマンドやACK telemetryまでACK対象となり、パケットが連続生成されていた。

- 受信処理を `Ss`（LoRa受信RSSI）を持つパケットだけに限定。
- ACK送信用の telemetry listener と旧heartbeat listenerを削除。
- ACK送信用パケットを `uplink_listener_` に再投入しないよう除外指定を追加。

これにより、1つのLoRa受信パケットに対してACKを1つだけ生成し、Tracker内の自己ループを停止する。

---

## 検証

以下コマンドでビルド確認。

```powershell
platformio run -e GOLIDEN
platformio run -e Tracker
```

結果:

- `GOLIDEN`: SUCCESS
- `Tracker`: SUCCESS

実行コマンド:

```powershell
platformio run -e GOLIDEN
platformio run -e Tracker
```

Python ブリッジ検証:

```powershell
.\.venv\Scripts\python.exe src/library/wcpp/python/util.py --help
```

上記は `ImportError` なしで help 表示まで進み、起動確認完了。

Tracker ACKループ修正後のビルド:

```powershell
platformio run -e Tracker
```

結果: `SUCCESS`

実機確認では、Tracker再upload後に一度だけテストパケットを送り、`util.py` の total packets が増え続けず、
Trackerの `Uplink Received!` とGOLIDEN側のACK受信が各1回になることを確認する。

---

## 変更の意味

- GOLIDEN 側から旧RP2040中継を前提とした残骸（モジュールID `0x4D` への不要監視）を除去。
- GOLIDEN単体で、
  - SerialBus入力パケットの LoRa 送信ラップ
  - LoRa受信パケットのカーネル配信（LoRaコンポーネント）
  - カーネル配信パケットの SerialBus 出力（SerialBusコンポーネント）
  の経路が完結。
- 受信済み LoRa パケットを再度無線送出してしまうループを防止。
- Windows/Linuxで `wcpp` を使う Python ツールが package 経由で起動できるようにし、PC⇄GOLIDEN bridge の起動障害を解消。