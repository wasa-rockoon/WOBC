# パケット受信方法ガイド

他のコンポーネントから送信されたパケットを受信する方法について、logger.cpp を参考にした解説です。

## 5 つのステップで理解する

### ① リスナーを宣言する（ヘッダファイル）

```cpp
#include <library/wobc.h>

namespace component {

class MyComponent : public process::Component {
protected:
  kernel::Listener my_packets_;  // パケット受信用のリスナー
};

}
```

**何をしている？**
- パケットを受け取るための「箱」を用意します
- すべてのパケットか、特定のパケットのみ受け取るかは後で設定します

---

### ② リスナーを初期化する（setup()）

```cpp
void MyComponent::setup() {
  listen(my_packets_, 16);  // キューサイズを指定
  // my_packets_.xx();       // ←オプション：フィルタを設定
}
```

**何をしている？**
- `listen(my_packets_, 16)` = 「my_packets という箱に、最大 16 個のパケットを溜めるようにしてね」という指示
- キューサイズは、一度に受け取りたいパケット数に応じて決めます
  - **少ない場合（1～10 個）**: 8～16
  - **多い場合（100 個以上）**: 256 以上（logger の例）

---

### ③ フィルタを設定する（オプション）

受け取るパケットを限定したい場合に設定します。

#### パターン A: すべてのパケットを受け取る

```cpp
listen(my_packets_, 16);  // フィルタなし
```

#### パターン B: コマンドだけを受け取る

```cpp
listen(my_packets_, 16);
my_packets_.command();
```

#### パターン C: テレメトリだけを受け取る

```cpp
listen(my_packets_, 16);
my_packets_.telemetry();
```

#### パターン D: 特定のコンポーネントから受け取る

```cpp
listen(my_packets_, 16);
my_packets_.component(0x50);  // コンポーネント ID = 0x50
```

#### パターン E: 特定のユニットからのパケットを受け取る

```cpp
listen(my_packets_, 16);
my_packets_.unit_origin('a');  // ユニット 'a' からのパケットのみ
```

---

### ④ パケットがあるか確認する（loop()）

```cpp
void MyComponent::loop() {
  // 方法 1: if 文で確認
  if (my_packets_) {
    // パケットがある
  }

  // 方法 2: 利用可能なパケット数を取得
  unsigned count = my_packets_.available();
  if (count > 0) {
    // パケットがある
  }

  // 方法 3: while ループで全パケット処理
  while (my_packets_) {
    // パケットがなくなるまで処理
  }
}
```

---

### ⑤ パケットを取り出して処理する

```cpp
void MyComponent::loop() {
  while (my_packets_) {  // ④ パケットがある間、ずっと処理
    const wcpp::Packet packet = my_packets_.pop();  // ⑤ パケットを取り出す

    // パケットの内容を読む
    // 例：パケット内の "Ts" という項目を読む
    auto ts_field = packet.find("Ts");
    if (ts_field) {
      int timestamp = (*ts_field).getInt();
      Serial.println(timestamp);
    }
  }
}
```

---

## 実例で学ぶ

### 例 1: Logger コンポーネント（すべてのパケットを SD カードに記録）

```cpp
// ヘッダファイル
class Logger : public process::Component {
protected:
  Listener all_packets_;  // ① リスナー宣言
};

// .cpp ファイル
void Logger::setup() {
  listen(all_packets_, WOBC_LOGGER_PACKET_QUEUE_SIZE);  // ② 初期化
}

void Logger::sdWriteTask() {
  while (true) {
    if (file_ && all_packets_) {  // ④ パケット確認
      const wcpp::Packet packet = all_packets_.pop();  // ⑤ パケット取得
      
      // SD カードに書き込み
      file_.write(packet.encode(), packet.size());
    }
    delay(1);
  }
}
```

### 例 2: Telemeter コンポーネント（ユニット 'a' からのパケットを WebSocket で送信）

```cpp
// ヘッダファイル
class Telemeter : public process::Component {
protected:
  Listener up_packets_;
};

// .cpp ファイル
void Telemeter::setup() {
  up_packets_.unit_origin('a');  // ③ フィルタ設定：ユニット 'a' のみ
  listen(up_packets_, 8);        // ② 初期化
}

void Telemeter::loop() {
  while (up_packets_) {  // ④ パケット確認
    const wcpp::Packet packet = up_packets_.pop();  // ⑤ パケット取得
    
    // WebSocket で送信
    webSocket_.sendTXT((const char*)packet.encode());
  }
}
```

---

## よく使うメソッド一覧

### Listener のメソッド

| メソッド | 説明 | 使用例 |
|---------|------|--------|
| `available()` | キュー内のパケット数を取得 | `if(listener.available() > 0)` |
| `operator bool()` | パケットがあるか確認 | `if(listener)` |
| `pop()` | パケット取得＆キューから削除 | `auto p = listener.pop()` |
| `peek()` | パケット確認（削除しない） | `auto p = listener.peek()` |
| `clear()` | キューをクリア | `listener.clear()` |

### フィルタ設定メソッド

| メソッド | 説明 |
|---------|------|
| `command()` | コマンドのみ受信 |
| `telemetry()` | テレメトリのみ受信 |
| `packet(ID)` | 特定のパケット ID |
| `component(ID)` | 特定のコンポーネント ID |
| `unit_origin(ID)` | 送信元ユニット ID |
| `unit_dest(ID)` | 宛先ユニット ID |

### Packet のメソッド（取得したパケットから情報を読む）

| メソッド | 説明 | 戻り値 |
|---------|------|--------|
| `find("key")` | 特定フィールドを検索 | `Optional<Field>` |
| `packet_id()` | パケット ID を取得 | `uint8_t` |
| `component_id()` | コンポーネント ID を取得 | `uint8_t` |
| `isCommand()` | コマンドか確認 | `bool` |
| `isLocal()` | ローカルか確認 | `bool` |
| `encode()` | バイト列に変換 | `uint8_t*` |
| `size()` | サイズを取得 | `unsigned` |

---

## よくある間違い

### ❌ 間違い：setup() でフィルタを設定してから listen() する

```cpp
void MyComponent::setup() {
  my_packets_.command();      // ← これは効かない
  listen(my_packets_, 16);    // ← listen() が後だから
}
```

### ✅ 正解：listen() のあとでフィルタを設定する

```cpp
void MyComponent::setup() {
  listen(my_packets_, 16);    // ← 先に listen()
  my_packets_.command();      // ← その後でフィルタ設定
}
```

### ❌ 間違い：パケット処理を loop() でずっと続ける（ブロック）

```cpp
void MyComponent::loop() {
  while (my_packets_) {  // ← 1000 個あったら 1000 個全部処理
    auto packet = my_packets_.pop();
    // 重い処理...
  }
  // ← loop() が返らない可能性がある
}
```

### ✅ 正解：バックグラウンドスレッドで処理（Logger のように）

```cpp
void MyComponent::setup() {
  xTaskCreatePinnedToCore(processTaskWrapper, "Task", 8192, this, 1, &task_handle_, 0);
}

void MyComponent::processTask() {
  while (true) {
    while (my_packets_) {
      auto packet = my_packets_.pop();
      // 重い処理...
    }
    delay(1);  // CPU を独占しない
  }
}
```

---

## チェックリスト

新しいコンポーネントでパケット受信を追加する時：

- [ ] ヘッダファイルに `Listener my_packets_` を追加した
- [ ] `setup()` で `listen(my_packets_, サイズ)` を呼んだ
- [ ] 必要に応じてフィルタを設定した（`.command()` など）
- [ ] `loop()` または別スレッドでパケットを処理している
- [ ] `while(my_packets_)` と `pop()` を正しく使っている
- [ ] 処理が重い場合は、Logger のようにバックグラウンドスレッド化を検討した

---

## 参考ファイル

- [logger.cpp](../src/components/Logger/logger.cpp) - SD カード記録（複数パケット処理）
- [logger.h](../src/components/Logger/logger.h) - Listener の宣言例
- [telemeter.cpp](../src/components/Telemeter/telemeter.cpp) - フィルタ設定の例
- [listener.h](../src/library/kernel/listener.h) - Listener クラスの定義
