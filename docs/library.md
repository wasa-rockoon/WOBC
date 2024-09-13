
# WOBCライブラリの使い方

`src/library`下にあるWOBCライブラリによって，コンポーネントやモジュールのソフトウェアを書くための機能が提供される．

# WOBCシステムアーキテクチャ

WOBC上でのソフトウェアでは，複数のコンポーネントが並行に実行され，それらの間のデータのやり取りがwcppパケットを用いたコマンドやテレメトリにより行われる．バスに送信されたパケットは，CANバスで繋がれた他のモジュール上で実行されているものも含めた他の全てのコンポーネントに届けられる．コンポーネントはコマンド（や必要に応じてテレメトリ）を受信し処理を行う．

パケット処理等の機能は`src/library/kernel`によって提供される．具体的には，パケットのデータ領域を確保する参照カウンタを備えたヒープ，コンポーネントに必要なパケットを振り分ける機能等が含まれる．

これらのカーネルが提供する機能は`process::Process`クラスのメンバを通してアクセスするほか，一部の機能は`kernel::`名前空間にあるシステムコールを通して利用する．

# コンポーネント

ソフトウェアにおいても，コンポーネントが最も基本的な開発単位である．コンポーネントの機能は以下に集約される．

- コマンドやテレメトリを受け取る．
- センサやアクチュエータなどのハードウェアを制御する．
- データ処理を行う．
- コマンドやテレメトリを送信する．

コンポーネント同士はコード上でも疎結合とするべきである．コンポーネント間のデータのやり取りは全てパケットを介して行い，グローバル変数を共有することは禁止される．

##　ソースファイルの構成

`src/components`下に，以下のようにコンポーネントのディレクトリを作成する．

```
src/components
├── MyCompo               # コンポーネントディレクトリ（ディレクトリ名はアッパーキャメルケース）
    ├── my_compo.h        # コンポーネントのコード（ファイル名はスネークケース）
    ├── my_compo.cpp        
    ├── sensor_driver.h   # コンポーネント内で使うドライバ等
    ├── sensor_driver.cpp 
├── AnotherCompo
```

`src/modules`下に，以下のようにモジュールのディレクトリを作成する．

```
src/modules
├── MyMod        # モジュールディレクトリ（ディレクトリ名はアッパーキャメルケース）
    ├── main.cpp # メイン         
├── AnotherMod
```

platformio.iniの`build_src_filter`でモジュールディレクトリを指定することで，envごとに用いる`main.cpp`を区別する．

## コンポーネントの実装

コンポーネントは`component`名前区間の中に[`process::Component`](../src/library/process/component.h)クラスを継承したクラスとして実装する．

```cpp
#include <library/wobc.h>

namespace component {

class MyCompo : public process::Component {
public:
  static const uint8_t component_id = 0x01;

  // コンポーネント名とidを指定して継承
  MyCompo(): process::Component("MyCompo", component_id) {}; 

protected:
  void setup() override; // start時に一度だけ呼ばれる．
  void loop() override;  // setupの後，無限回呼ばれる．
  void onCommand(const wcpp::Packet& command) override; // 自component_id宛のパケットが来たときに呼ばれる．
};
}
```

`setup`, `loop`, `onCommand`の各メンバ関数を必要に応じてoverrideすることで実装を追加する．ただし`loop`関数の中で無限ループを使わないこと．

## タイマー

[`process::Timer`](../src/library/process/timer.h)クラスを継承することで，一定時間ごとに処理を実行するタイマーを作ることができる．

```cpp
#include <library/wobc.h>

namespace component {

class MyCompo : public process::Component {
public:
  static const uint8_t component_id = 0x01;

  MyCompo(): process::Component("MyCompo", component_id) {}; 

protected:
  void setup() override {
    start(my_timer_); // タイマーを開始
  }

  class SampleTimer : public process::Timer {
  public:
    static const unsigned interval_ms = 1000;

    SampleTimer(): process::Timer("MyTimer", interval_ms) {}
    
  protected:
    void callback() override; // 実行する処理を記述
  } my_timer_;
};
}
```

## タスク

[`process::Task`](../src/library/process/task.h)クラスを継承することで，コンポーネント本体とは並行に処理を実行するタスクを作ることができる．

```cpp
#include <library/wobc.h>

namespace component {

class MyCompo : public process::Component {
public:
  static const uint8_t component_id = 0x01;

  MyCompo(): process::Component("MyCompo", component_id) {}; 

protected:
  void setup() override {
    start(my_task_); // タスクを開始
  }

  class SampleTask : public process::Task {
  public:
    SampleTask(): process::Timer("MyTask") {}
    
  protected:
    void setup() override; // start後に一回だけ呼ばれる
    void loop() override;  // start後に，terminate()で終了するまで無限回呼ばれる
  } my_task;
};
}
```

## パケット関連

上述した`Component`, `Timer`, `Task`はいずれも[`process::Process`](../src/library/process/process.h)クラスのサブクラスとなっている．，`process::Process`には以下のようなパケットの送受信等に関わるメンバ関数が備わっている．

### パケットの作成と送信

#### `wcpp::Packet newPacket(uint8_t size)`

データ領域を確保し．空のパケットを返す．

#### `wcpp::Packet decodePacket(const uint8_t* data)`

データ領域を確保し．`data`をデコードして得たパケットを返す．

#### `void sendPacket(const wcpp::Packet& packet)`

パケットをバス上に送信する．

#### `void sendPacket(const wcpp::Packet &packet, const Listener& exclude)`

パケットをバス上に送信する．その際，`exclude`で指定した`Listener`がパケットを受け取らないようにする．`Listener`でパケットの受信をトリガーに新たなパケットを送信するような場合に，自ら送信したパケットによって再び`Listener`がトリガーされるといったループを起こさないために用いる．

### パケットの受信

#### コマンドの受信

自コンポーネント宛のコマンドに対する処理は，`Component::onCommand(const wcpp::Packet& packet)`をオーバーロードして記述する．

#### その他のパケットの受信

上記以外のコマンドやテレメトリを受信する必要がある場合には，[`process::Process::Listener`](../src/library/kernel/listener.h)を用いる．

```cpp
#include <library/wobc.h>

namespace component {

class MyCompo : public process::Component {
public:
  static const uint8_t component_id = 0x01;
  static const unsigned listener_queue_size = 4;

  MyCompo(): process::Component("MyCompo", component_id) {}; 

protected:
  Listener my_listener_;

  void setup() override {
    // Listenerは何も指定しないと全てのパケットを受信する

    // テレメトリまたはコマンドのみ受信するようにフィルター可能
    my_listener_.telemetry(); 
    // my_listener_.command();
    
    // パケットid，コンポーネントid，送信元id, 送信先idでフィルター可能
    // それぞれビットマスクを指定できる（省略した場合は完全一致のみ通過）
    my_listener_.packet(id, mask);
    my_listener_.component(id, mask);
    my_listener_.unit_origin(id, mask);
    my_listener_.unit_dest(id, mask);

    listen(my_listener_, listener_queue_size); // 受信開始
  }
  void loop() override {
    while (my_listener_) { // キューにパケットがある場合
      const wcpp::Packet& packet = my_listener_.pop(); // キューから一つ取り出す
      // Listenerがパケットを受信したときの処理
    }
  }
  void onCommand(const wcpp::Packet& command) override {
    // 自コンポーネント宛のコマンドを受信したときの処理
  }
};
}
```

### ログとエラー出力

ログやエラーの出力もパケットを用いて行う．特に`SerialBus`を用いている場合には，直接`Serial.print`を用いてログを出力することはシリアル出力が汚染されるのみならず並列実行によりクラッシュする可能性があるため推奨されない．

#### void LOG(const char* format, Args... args)

デバッグ等のためのログパケット(id=0x23='#')を出力する．メッセージ，記述したファイル名，行番号をエントリに含むパケットが送信される．
`printf`と同様に，フォーマット文字列と可変長の引数を与えることができる．

#### void error(const char* code, const char* format, Args... args)
エラーパケット(id=0x21='!')を出力する．4文字以下のエラーコードと，メッセージを含むパケットが送信される．
`printf`と同様に，フォーマット文字列と可変長の引数を与えることができる．

### パケットの不揮発メモリへの保存

#### `bool storePacket(const wcpp::Packet& packet)`

コンポーネントidとパケットidをキーに，パケットを不揮発メモリに保存する．すでに保存されているパケットがある場合は上書き保存される．

#### `wcpp::Packet loadPacket(uint8_t packet_id)`

コンポーネントidと`packet_id`をキーに，パケットを不揮発メモリから読み込む．保存されているパケットがない場合には`wcpp::Packet::null()`を返す．

#### `bool storeOnCommand(uint8_t packet_id)`

コンポーネントの`setup`内で呼んでおくと，指定した`packet_id`のコマンドを受信したとき自動的に不揮発メモリに上書き保存されるようになる．設定値をリモートコマンドで変更できるようにするのに便利である．

### その他のProcessメンバ関数

#### void delay(unsigned ms)

Arduinoで提供される`delay`の代わりに用いる．


# モジュール

モジュールはマイコンに書き込むプログラムの単位であり，`main.cpp`に記述する．
`setup`内で搭載するコンポーネントの初期化等を行う．ユニット内にただ一つ存在するMainコンポーネントを搭載するMainモジュールの場合は，これも記述する．

### Mainモジュールの場合
```cpp
constexpr uint8_t module_id = 0xF0;
constexpr uint8_t unit_id = 0x01;

// Core機能
core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

// コンポーネント
component::MyComp my_comp;

// Mainコンポーネント
class Main: public process::Component {
public:
  Main(): process::Component("Main", 0x00) {} // Mainコンポーネントのidは0x00

  void setup() override;
  void loop() override;
} main_;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  kernel::setUnitId(unit_id); // unit id を設定
  
  // モジュールidを確認
  // もし前回書き込んだモジュールidと異なる場合は，実行を中断して回路を保護する
  // モジュールidを変更したい場合は kernel::begin(module_id, false) とする
  if (!kernel::begin(module_id, true)) return;

  // Core機能を起動
  can_bus.begin();
  serial_bus.begin();

  delay(1000);

  // コンポーネントを起動
  my_comp.begin();
  main_.begin();
}
```

### Main以外のモジュールの場合

Mainモジュールと接続されていれば，Mainモジュールで指定したunit_idを`kernel::unit_id()`で取得できる．

```cpp
constexpr uint8_t module_id = 0xF1;

// Core機能
core::CANBus can_bus(44, 43);
core::SerialBus serial_bus(Serial);

// コンポーネント
component::MyComp my_comp;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!kernel::begin(module_id, true)) return;

  // Core機能を起動
  can_bus.begin();
  serial_bus.begin();

  delay(1000);

  // コンポーネントを起動
  my_comp.begin();
}
```