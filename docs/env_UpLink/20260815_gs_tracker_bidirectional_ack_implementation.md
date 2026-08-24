# 双方向通信（ACK応答および衝突回避制御）の実装と使い方

作業者：Aoyama (Gemini)  
作成日：2026年8月15日  
関連ファイル：
- [src/modules/Tracker/main.cpp]
- [src/components/LoRa/lora.cpp]
- [src/modules/GS/main.cpp]
- [platformio.ini]

---

## 目標

GS ➔ Tracker への一方通向のコマンド送信に加え、Tracker 側からの**「コマンド受領応答パケット(ACK)」の返信**と、LoRaモジュール送信時の**衝突回避・タイムアウト保護**を実装し、双方向通信（PC ⇄ GS ⇄ Tracker）を確立・完成させる。

---

## 全体アーキテクチャとデータフロー

```
 [ PC ]
   │  ▲
   │  │ 4. ACK受領 (SerialBus 透過)
   │  │
   ▼  │
 [ GS (Ground Station) ] ───( 1. アップリンクコマンド送信 )───> [ Tracker ]
   ▲                                                             │
   │                                                             │ 2. アップリンク受領・解読
   │                                                             ▼
   └───────────────────( 3. ACKパケット返信 )─────────────────────┘
```

1. **PC ➔ GS ➔ Tracker**: コマンドパケット（例: Packet ID `'t'`) を送信。
2. **Tracker受領**: `uplink_listener_` で受領ログを出力し、ACK応答パケット (Packet ID `'a'`) を生成。
3. **Tracker ➔ GS (LoRa)**: ACKパケットを LoRa送信コマンド (`'s'`) にラップして無線送信。
4. **GS ➔ PC (SerialBus)**: GS の `LoRa` コンポーネントが受領し、`SerialBus` 経由で PC へ透過送信。GS の `pc_listener_` が ACK を再度無線送出し直さないよう無限ループ防止フィルタを実行。

---

## 変更タスクとコードの解説

### 1. Trackerモジュール (`src/modules/Tracker/main.cpp`) へのACK返信機能追加

#### 変更内容
`uplink_listener_` でアップリンクパケット（`"Ss"` エントリを持つもの）を受信した際、受領確認を示す ACK パケットを自働生成して GS / PC 宛に返信する処理を追加しました。

```cpp
        while (uplink_listener_) {
            wcpp::Packet rx_packet = uplink_listener_.pop();
            auto rssi_entry = rx_packet.find("Ss");
            if (rssi_entry) {
                int rssi = (*rssi_entry).getInt();
                LOG("Uplink Received! Packet ID: '%c' (0x%02X), TargetComp: 0x%02X, Size: %d, RSSI: %d dBm",
                    rx_packet.packet_id(), rx_packet.packet_id(), rx_packet.component_id(), rx_packet.size(), rssi);

                // ACK応答パケットの作成 (Packet ID 'a', Status "St"=0, 受信Packet ID "Ri")
                wcpp::Packet ack_packet = newPacket(32);
                ack_packet.telemetry('a', rx_packet.component_id());
                ack_packet.append("St").setInt(0); // 0: Success
                ack_packet.append("Ri").setInt(rx_packet.packet_id());

                // ACKパケットを LoRa 送信用コマンドパケット ("Pa" エントリ) に包んで返信
                wcpp::Packet lora_send_packet = newPacket(ack_packet.size() + 32);
                lora_send_packet.command(lora.send_command_id, lora.component_id_base + 0);
                lora_send_packet.append("Pa").setPacket(ack_packet);
                sendPacket(lora_send_packet, uplink_listener_);
            }
        }
```

#### コードの意味・解説
- **`ack_packet.telemetry('a', ...)`**: Packet ID を `'a'` (ACK) に設定し、受信完了を表現。
- **`"St" = 0` / `"Ri"`**: 実行ステータス (`Status = 0`: 正常完了) および受領した元パケットの ID を記録。
- **`sendPacket(lora_send_packet, uplink_listener_)`**: `exclude` 指定により、Tracker 自身の `uplink_listener_` が自身で放流したパケットをキャッチしないように保護。

---

### 2. LoRaコンポーネント (`src/components/LoRa/lora.cpp`) の送信前ビジーチェック強化

#### 変更内容
`onCommand()` メソッドで E220 モジュールに送信を命令する際、モジュールがビジー状態（`e220_.isBusy()`）の場合の安全なタイムアウト保護を追加しました。

```cpp
void LoRa::onCommand(const wcpp::Packet& packet) {
  unsigned long start_time = millis();
  constexpr unsigned long busy_timeout_ms = 2000;
  while (e220_.isBusy()) {
    if (millis() - start_time > busy_timeout_ms) {
      LOG("LoRa send error: module busy timeout");
      return;
    }
    delay(10);
  }
  delay(100);
```

#### コードの意味・解説
- 従来の無限 `while(e220_.isBusy());` ループを廃止し、2000ms (2秒) 以上のビジー継続時にログを出力して安全に送信処理をキャンセル。
- モジュール障害や連続送信時の衝突によるマイコン全体のハングアップを回避。

---

### 3. GSモジュール (`src/modules/GS/main.cpp`) の受領パケット透過・再送信防止処理

#### 変更内容
Tracker から無線経由で GS に届いた ACK やテレメトリパケット（`"Ss"` RSSI エントリ付き）を、GS が PC 送信コマンドと誤認して再度 Tracker へ送り返してしまうループを防止するフィルタを追加しました。

```cpp
  void loop() override {
    while (pc_listener_) {
      wcpp::Packet packet = pc_listener_.pop();

      // LoRa宛の送信指示コマンド自体は再度LoRa送信コマンドにラップしない
      if (packet.component_id() == (component::LoRa::component_id_base + 0) &&
          packet.packet_id() == component::LoRa::send_command_id) {
        continue;
      }

      // Tracker等からLoRa受信されて内部カーネルに放流された受領パケット（"Ss" エントリを持つ）はLoRa再送信しない
      if (packet.find("Ss")) {
        continue;
      }

      // PC等から届いたコマンドパケットを LoRa 送信用コマンドパケットに包んで送信
      wcpp::Packet lorapacket = newPacket(packet.size() + 32);
      lorapacket.command(lora.send_command_id, lora.component_id_base + 0);
      lorapacket.append("Pa").setPacket(packet);
      sendPacket(lorapacket, pc_listener_);
    }
  }
```

#### コードの意味・解説
- **`if (packet.find("Ss")) continue;`**: Tracker から届いた ACK やテレメトリパケットには LoRa コンポーネントによって RSSI エントリ `"Ss"` が付加されています。これを検知して再ラップ処理から除外。
- **透過出力**: 内部カーネル上のパケットは `core::SerialBus serial_bus(Serial);` によって自動的に PC (`util.py`) へ送信されます。

---

## 動作確認・検証手順

### 1. マイコンへのファームウェア書き込み

```powershell
# GS モジュールへの書き込み
.venv\Scripts\platformio.exe run -e GS -t upload --upload-port COM3

# Tracker モジュールへの書き込み
.venv\Scripts\platformio.exe run -e Tracker -t upload --upload-port COM4
```

### 2. 双方向通信テスト

#### ターミナル 1: シリアルブリッジ (GS側)
```powershell
.venv\Scripts\python.exe src/library/wcpp/python/util.py -p COM3
```

#### ターミナル 2: PCコマンドの送信
```powershell
.venv\Scripts\python.exe src/library/wcpp/python/send_command.py -p t -c 0x10 -d "test"
```

#### ログ出力の確認

- **Tracker 側シリアルモニター (COM4)**:
  ```text
  [LOG] Uplink Received! Packet ID: 't' (0x74), TargetComp: 0x10, Size: 18, RSSI: -42 dBm
  ```

- **PC 側ターミナル (util.py / send_command.py)**:
  GS の `SerialBus` 経由で Tracker からの ACK パケット (Packet ID: `'a'`, `"St" = 0`, `"Ri" = 't'`) を自動受領。

---

## ノウハウ・ハマりどころ

1. **再送信（Ping-Pong）無限ループの回避**:
   受信パケットと送信コマンドが同一の内部カーネルバスを流れるシステム構造のため、`packet.find("Ss")` や `sendPacket(packet, listener)` の除外引数を適切に使用してルーティングループを遮断することが不可欠です。
2. **タイムアウト付きビジーチェックの重要性**:
   ハードウェアピン（E220 AUXピン）の状態が HIGH/LOW で固まった場合でも、ソフトウェア側でタイムアウト判定を入れておくことでシステム全体の停止を回避できます。
