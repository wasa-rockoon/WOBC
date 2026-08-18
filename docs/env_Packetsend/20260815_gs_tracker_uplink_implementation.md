# GS ➔ Tracker アップリンク通信の実装と使い方

作業者：Aoyama (Gemini)  
作成日：2026年8月15日  
関連ファイル：
- [src/components/LoRa/lora.cpp](file:///c:/Users/shoko/waseda/sa-kuru/WASA/WOBC/src/components/LoRa/lora.cpp)
- [src/modules/GS/main.cpp](file:///c:/Users/shoko/waseda/sa-kuru/WASA/WOBC/src/modules/GS/main.cpp)
- [src/modules/Tracker/main.cpp](file:///c:/Users/shoko/waseda/sa-kuru/WASA/WOBC/src/modules/Tracker/main.cpp)
- [platformio.ini](file:///c:/Users/shoko/waseda/sa-kuru/WASA/WOBC/platformio.ini)

---

## 目標

PCから Ground Station (GS) 経由で Tracker へ無線コマンドパケットを送信し、Tracker 側で受領・解読してシリアルログを出力する一連の**アップリンク経路（PC ➔ SerialBus ➔ GS ➔ LoRa無線 ➔ Tracker ➔ LOG）**を構築・実装する。

---

## 全体アーキテクチャとデータフロー

```
[ PC ] ──( send_command.py )──> .command File
                                    │
                             ( util.py: シリアル送信 )
                                    ▼
[ GS (Ground Station) ] ──( SerialBus 経由で内部カーネルへ受領 )
                         │
                         ├─ Main Listener: パケットを検知
                         └─ LoRa送信パケットに再ラッピング ('s' コマンド, Target: 0x10, "Pa" エントリ)
                                    │
                             ( E220 LoRa 送信 )
                                    ▼
[ Tracker ] ───────────────( E220 LoRa 受信 )
                         │
                         ├─ LoRa Component: チェックサム検証 & RSSI("Ss")付与 ➔ 内部カーネル放流
                         └─ Main Component (uplink_listener_): 受領通知ログ (LOG) を出力
```

---

## 変更タスクとコードの解説

### 1. ESP32用 LoRaコンポーネントの受信処理追加 (`src/components/LoRa/lora.cpp`)

#### 変更内容
`LoRa::loop()` の空関数だった部分に、E220 モジュールからのデータ受領処理を追加しました。

```cpp
void LoRa::loop() {
  uint8_t data[255];
  unsigned len = e220_.receive(data);

  while(e220_.isBusy()){}

  if (len > 0) {
    unsigned data_size = len - 1;
    uint8_t received_checksum = data[data_size];
    uint8_t* received_data = data;
    wcpp::Packet packet_received = decodePacket(received_data);
    uint8_t calculated_checksum = packet_received.checksum(received_data, data_size);

    if (calculated_checksum == received_checksum) {
      int rssi = e220_.getRSSI();
      wcpp::Packet packet = newPacket(packet_received.size() + 10);
      packet.copy(packet_received);
      packet.append("Ss").setInt(rssi);
      sendPacket(packet);
    } else {
      LOG("fail to receive");
    }
  }
}
```

#### コードの意味・解説
1. `e220_.receive(data)`: E220 無線モジュールから受信バイト配列を取得します。
2. **チェックサム検証**: 末尾の1バイト (`data[len - 1]`) に格納されたチェックサム値と、受信したバイナリから再計算したチェックサムが一致しているかを検証します。
3. **パケット復元と内部カーネルへの送出**:
   - `decodePacket()` により WCPP パケット構造体にデコード。
   - `getRSSI()` で無線信号強度を取得し、`"Ss"` エントリとしてパケットに付加。
   - `sendPacket(packet)` を呼ぶことで、マイコン内部の WOBC カーネルバスへパケットを放流し、他のコンポーネントが受信できるようにします。

---

### 2. GSモジュール (`src/modules/GS/main.cpp`) の転送ロジック追加

#### 変更内容
GS モジュールに `component::LoRa` を組み込み、PCから届いたパケットを LoRa 送信用コマンドパケットに自動転送するロジックを組み込みました。

```cpp
// 1. LoRa ピン定義とインスタンス化
#define LORA_CHANNEL 3
#define LORA_TX_PIN 38
#define LORA_RX_PIN 39
#define LORA_AUX_PIN 40
#define LORA_M0_PIN 12
#define LORA_M1_PIN 11

component::LoRa lora(LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN, LORA_TX_PIN, LORA_RX_PIN, LORA_CHANNEL, 0);

// 2. Main コンポーネントでの受領・転送処理
class Main: public process::Component {
public:
  Main(): process::Component("main", 0x00) {}
  kernel::Listener pc_listener_;

  void setup() override {
    listen(pc_listener_, 8); // 全パケットをキューサイズ8で受信
  }

  void loop() override {
    while (pc_listener_) {
      wcpp::Packet packet = pc_listener_.pop();

      // LoRa宛の送信指示コマンド自体は再度LoRa送信コマンドにラップしない（無限ループ防止）
      if (packet.component_id() == (component::LoRa::component_id_base + 0) &&
          packet.packet_id() == component::LoRa::send_command_id) {
        continue;
      }

      // PC等から届いたパケットを LoRa 送信用コマンドパケットに包んで送信
      wcpp::Packet lorapacket = newPacket(packet.size() + 32);
      lorapacket.command(lora.send_command_id, lora.component_id_base + 0);
      lorapacket.append("Pa").setPacket(packet);
      sendPacket(lorapacket, pc_listener_);
    }
  }
} main_;
```

#### コードの意味・解説
1. `listen(pc_listener_, 8)`: PC から `SerialBus` 経由で GS マイコンに届いたパケットを捕捉します。
2. **無限ループ防止**: 自身が作成した LoRa 送信指示コマンド (`Target = 0x10`, `CommandID = 's'`) や `sendPacket(lorapacket, pc_listener_)` の除外指定 (`exclude`) を使うことで、GS が自分の送信コマンドを二重に再転送する無限ループを防ぎます。
3. **パケットのカプセル化**: 元のパケットを LoRa 送信用コマンドの `"Pa"` エントリに格納（`setPacket(packet)`）し、`LoRa` コンポーネント宛に送出します。

---

### 3. Trackerモジュール (`src/modules/Tracker/main.cpp`) の受領ログ処理

#### 変更内容
Tracker 側でアップリンク受領時にシリアル出力する `uplink_listener_` を追加しました。

```cpp
class Main : public process::Component {
public:
    Main() : process::Component("main", 0x00) {}
    kernel::Listener my_listener_;
    kernel::Listener heartbeat_;
    kernel::Listener uplink_listener_;

    void setup() override {
        my_listener_.telemetry(); 
        listen(my_listener_, 8);
        heartbeat_.component(0x54);
        listen(heartbeat_,1);
        listen(uplink_listener_, 8);
    }

    void loop() override {
        ...
        while (uplink_listener_) {
            wcpp::Packet rx_packet = uplink_listener_.pop();
            auto rssi_entry = rx_packet.find("Ss");
            if (rssi_entry) {
                int rssi = (*rssi_entry).getInt();
                LOG("Uplink Received! Packet ID: '%c' (0x%02X), TargetComp: 0x%02X, Size: %d, RSSI: %d dBm",
                    rx_packet.packet_id(), rx_packet.packet_id(), rx_packet.component_id(), rx_packet.size(), rssi);
            }
        }
    }
} main_;
```

#### コードの意味・解説
1. `rx_packet.find("Ss")`: GS 経由で LoRa 受信されたパケットには、LoRa コンポーネントによって RSSI エントリ `"Ss"` が付与されています。
2. `(*rssi_entry).getInt()`: `find()` は `EntriesIterator` を返すため、`*` でデリファレンスして `Entry` の `.getInt()` メソッドを呼び出します。
3. `LOG(...)`: パケット ID・対象コンポーネント ID・パケット長・受信電波強度 (dBm) をシリアルログへ出力します。

---

## 機能の使い方・動作確認手順

### 1. マイコンへのファームウェア書き込み

```powershell
# GS 側へ書き込み (PORTは環境に合わせて変更)
.venv\Scripts\platformio.exe run -e GS -t upload --upload-port COM3

# Tracker 側へ書き込み
.venv\Scripts\platformio.exe run -e Tracker -t upload --upload-port COM4
```

### 2. PCからコマンド送信と受領テスト

#### ターミナル 1: GSとのシリアル通信ブリッジ起動
```powershell
.venv\Scripts\python.exe src/library/wcpp/python/util.py -p COM3
```

#### ターミナル 2: PCコマンドの送信
```powershell
# テストパケットの1発送信
.venv\Scripts\python.exe src/library/wcpp/python/send_command.py --packet-id t --component-id 0x10 --data "hello"

# 対話型メニューでの送信
.venv\Scripts\python.exe src/library/wcpp/python/send_command.py -i
```

#### ターミナル 3 / シリアルモニター (Tracker側: COM4, 115200bps)
Tracker 側で以下のログが出力されればアップリンク成功です。
```text
[LOG] Uplink Received! Packet ID: 't' (0x74), TargetComp: 0x10, Size: 18, RSSI: -45 dBm
```

---

## トラブルシューティング・ノウハウ (Gotchas)

### 1. Git SSH URL による PlatformIO ビルド失敗
- **現象**: `git@github.com:...: Permission denied (publickey)` でライブラリ（`can_common` 等）の取得に失敗する。
- **原因**: SSH公開鍵がGitHubに未登録の環境で `platformio.ini` の `lib_deps` が `git@github.com:` 形式になっている。
- **対策**:
  1. `git config --global url."https://github.com/".insteadOf "git@github.com:"` を実行。
  2. `platformio.ini` 内の `git@github.com:` を `https://github.com/` に置き換える。

### 2. `wcpp::Packet::find()` の戻り値アクセスエラー
- **現象**: `error: 'class wcpp::EntriesIterator' has no member named 'getInt'`
- **原因**: `find()` は `Entry` 直接ではなく `EntriesIterator` イテレータオブジェクトを返す。
- **対策**: `(*rssi_entry).getInt()` のようにデリファレンス演算子 `*` をつけて `Entry` を取得してからメソッドを呼び出す。
