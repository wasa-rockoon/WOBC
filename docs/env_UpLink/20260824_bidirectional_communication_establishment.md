# PC <-> GOLIDEN <-> Tracker 双方向通信確立の作業ログと仕様解説

作業者：Aoyama
作成日：2026年8月24日  
関連：
- [docs/env_GOLIDEN/20260626_PC_to_SerialBus_packet_send.md]
- [docs/env_GOLIDEN/20260726_send_command_tool.md]

---

## 目標

PC ↔ GOLIDEN (Ground Station) ↔ Tracker (機体) 間において、LoRa 無線を用いた完全な双方向通信（PCからのコマンド送出 ➔ GOLIDENでのLoRa転送 ➔ Trackerでの受領・解読 ➔ ACK応答パケット返信 ➔ GOLIDEN受信 ➔ PC画面表示）を確立する。
今後の開発・引き継ぎ資料として、全体アーキテクチャ、変更箇所のコード意味、ツールの使い方、および開発中に直面したトラブルと解消ノウハウを蓄積する。

---

## 1. 全体アーキテクチャとデータフロー

本システムは **GOLIDEN (GS&LoRa一体型基板)** と **Tracker (機体側基板)** の 2基板構成で動作します。

```
 [ PC ]
   │  (1) send_command.py でパケット生成
   ▼
 .command ファイル
   │  (2) util.py が常時監視 & USB Serial (COM_A) へ送信
   ▼
 【 GOLIDEN (ESP32) 】
   │  (3) core::SerialBus がバイナリ受信 -> 内部カーネルへ放流
   │  (4) GOLIDEN Main が検知し LoRa 送信パケット ('s', "Pa" タグ) に再梱包
   │  (5) component::LoRa (E220) で 920MHz 無線送出
   │
   ▼  (LoRa 電波)
   │
 【 Tracker (ESP32) 】
   │  (6) component::LoRa (E220) が電波受信 -> チェックサム検証 & RSSI("Ss")付与 -> 内部カーネル放流
   │  (7) Tracker Main が受領検知 -> ログ出力 (LOG)
   │  (8) Tracker Main が ACK応答パケット ('a', St=0, Ri=受信ID) を自動生成
   │  (9) LoRa 電波で返信
   │
   ▼  (LoRa ACK電波)
   │
 【 GOLIDEN (ESP32) 】
   │  (10) component::LoRa が ACK電波を受信 -> 内部カーネル放流
   │  (11) core::SerialBus が拾って USB Serial (COM_A) 経由で PC へ送信
   ▼
 [ PC ]
   └─ (12) util.py 画面の左ツリーに 0x61 (Tracker) -> 0x61 ('a') ACKパケットが表示される！
```

---

## 2. 実際の変更箇所とコード解説

### (1) ESP32用 LoRa ドライバの受信対応
* **変更ファイル**: [src/components/LoRa/lora.cpp]
* **変更内容と意味**:
  `LoRa::loop()` の空関数だった部分に `e220_.receive()` による電波受領処理を追加。受信バイナリからチェックサムを再計算して検証し、正しければ `getRSSI()` で信号強度（`"Ss"` エントリ）を付与して `sendPacket(packet)` で WOBC 内部カーネル空間へ放流するようにしました。

### (2) GOLIDEN モジュール (GS&LoRa一体型基板)
* **変更ファイル**: [src/modules/GOLIDEN/main.cpp]
* **変更内容と意味**:
  `Main::loop()` にて、PC (SerialBus) 経由で届いたパケットを捕捉。LoRa 送信用パケット（Target: LoRa, Command: `'s'`, `"Pa"` エントリに元パケットを収納）に包み直して送出する転送ロジックを実装しました。この際、無限ループ防止のため自リスナー除外 (`sendPacket(lorapacket, tx_listener_)`) を徹底しました。

### (3) Tracker モジュール (機体側基板)
* **変更ファイル**: [src/modules/Tracker/main.cpp]
* **変更内容と意味**:
  - `uplink_listener_` を追加し、GSからの電波を受信。
  - 関係のない自動センサーパケット（例: `'M'`）を無視するため、`rx_packet.packet_id() != 't' && rx_packet.packet_id() != 'c'` によるフィルタリングを実装。
  - コマンド受領時、`LOG()` 出力を行うとともに ACK応答パケット（`Packet ID = 'a'`, Status `"St"`=0, 受信パケットID `"Ri"`）を生成して LoRa 電波で返信。
  - 自分自身のリスナーを除外指定 (`sendPacket(..., my_listener_)`) してパケット無限増殖バグを修正。

### (4) PC側通信ツール群の改修
* **変更ファイル**: [src/library/wcpp/python/send_command.py], [src/library/wcpp/python/util.py]
* **変更内容と意味**:
  - `send_command.py`: CLI引数指定（`-p`, `-c`, `-d`）および対話型（REPLメニュー）でパケットを作成し `.command` に書き込むツールを新規作成。
  - `util.py`: ループ内に微小スリープ (`time.sleep(0.005)`) を挿入し、キー入力時の即時描画リフレッシュ処理を追加。CPU100%占有を解消し、爆速の描画レスポンスを実現。

---

## 3. 追加機能の使い方と運用マニュアル

### (1) PCからコマンドを送信する方法 (`send_command.py`)

#### 1発コマンド送信 (CLI引数モード)
```powershell
.venv\Scripts\python.exe src/library/wcpp/python/send_command.py --packet-id t --component-id 0x10 --data "hello"
```
* `--packet-id` (`-p`): 命令の種類を表す文字 (`'t'`, `'c'`, `'p'` など)。
  * `'t'`: テスト用パケット
  * `'c'`: 制御コマンドパケット
  * `'p'`: 生存確認 (Ping) パケット
* `--component-id` (`-c`): 送信先の機能ブロックID (無線送信宛ては `0x10`)。
* `--data` (`-d`): 送信する文字列や数値。

#### 対話型メニュー送信 (REPLモード)
```powershell
.venv\Scripts\python.exe src/library/wcpp/python/send_command.py -i
```
画面の数字選択（1: Ping, 2: テスト送信, 3: 任意パケット作成）に従って入力するだけで簡単に送信可能です。

### (2) パケット受信とリアルタイム表示 (`util.py`)

```powershell
.venv\Scripts\python.exe src/library/wcpp/python/util.py -p COM6 -b 115200
```
* **画面操作キー**:
  * `j` / `k`: パケットツリーの上下移動
  * `l` (エル) / `h`: フォルダの展開 / 閉じ
  * 中央パネルにパケットの中身（`St: 0`, `Ri: 116`, `Dt: 'hello'` 等）がリアルタイム表示されます。

---

## 4. 開発トラブルシューティングと得られたノウハウ

### 事象 1: Trackerのモニターが極めて重くフリーズする
* **原因**: Tracker の `main.cpp` 内で `sendPacket(lorapacket)` を呼ぶ際、第2引数に自リスナー（`my_listener_`）の除外指定が漏れていた。そのため、自作した送信指示パケットを自身で再度受信して再梱包・再送信する「無限増殖マトリョーシカバグ（1秒間に数万個のゴミパケット生成）」が発生していた。
* **解消法**: `sendPacket(lorapacket, my_listener_)` のように自分自身のリスナーを除外指定することで完治。

### 事象 2: コマンドを送っていないのに `LOG` や受信記録が出る
* **原因**: GOLIDEN 基板自身が自動生成して飛ばしているセンサーパケット（例: `'M'`）を、Tracker の `uplink_listener_` がすべて無差別キャッチしていた。
* **解消法**: `if (rx_packet.packet_id() != 't' && rx_packet.packet_id() != 'c') continue;` のフィルタリングを追加。

### 事象 3: シリアルモニタ画面が文字化けする (`&␕a...`)
* **原因**: WOBC の `SerialBus` はテキストではなく WCPP バイナリフォーマット (`[Data][CRC8][0x00]`) を直接出力しているため。
* **解消法**: テキストモニタではなく `util.py` を接続してデコード表示させる。

---

## 5. まとめと今後の展望

本作業により、**「PC ➔ GOLIDEN ➔ LoRa ➔ Tracker ➔ ACK返信 ➔ GOLIDEN ➔ PC」** の完全な双方向無線通信インフラが完成しました。
今後は、この通信基盤の上に乗せる形で、ロケットの**「分離コマンドの安全送出（二重認証・インターロック）および分離完了ステータスのフィードバック」**の実装へ進みます。
