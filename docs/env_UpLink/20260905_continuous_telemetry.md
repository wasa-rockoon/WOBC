# Tracker常時テレメトリとACK優先送信

> 2026-09-05追記: COM21モニタの起動フォルダを修正後、ユーザーが定期テレメトリとアップリンク／ACKの併用を実機確認した。経緯と画面の確認値は[統合作業ログ](20260905_telemetry_uplink_ack_worklog.md)を参照。

対象: `src/modules/Tracker/main.cpp`, `src/modules/GOLIDEN/main.cpp`,
`src/components/LoRa/{lora,e220}.{h,cpp}`。

## 1. Tracker定期送信

`Main::loop()`で2秒ごとに`'M'`を生成する。Mainのcomponent IDは`0x00`、
origin unitは`0x61`、宛先は`0xFF`。既存GPSの`'M'`はcomponent IDが`0x15`なので区別できる。

| データ | エントリ | 意味 |
| --- | --- | --- |
| 稼働時間 | `Ts` | `millis()`、ms |
| GPS | `LA`, `LO`, `AL` | 既存GPSパケットから緯度・経度・高度を転記 |
| 気圧 | `PR`, `PA` | 既存Pressureパケットから気圧・気圧高度を転記 |
| 連番 | パケットヘッダのsequence | 16 bit、キュー受理ごとに加算 |

センサーパケットは各1件だけキャッシュし、5秒以上更新されなければ該当エントリを省略する。
GPS測位の有効性は既存GPSコンポーネントの出力仕様に従う（本変更では測位有効フラグを追加していない）。
センサーが未接続でも`Ts`を含むテレメトリを送る。

```cpp
if (uint32_t(now - last_telemetry_ms_) < telemetry_interval_ms ||
    uplink_listener_ || !lora.canSendTelemetry()) return;
// ... 最新値からtelemetryを生成 ...
if (!lora.queuePacket(telemetry, component::LoRa::TxPriority::Telemetry)) return;
last_telemetry_ms_ = now;
```

送信を延期した分をまとめて送る動作はしない。ドライバも実際のUART送信間隔を最低2秒に制限する。
ACKを受理した後はMain側の周期をリセットして受信時間を確保する。

## 2. ACKとの排他制御

無線形式（E220長さバイト、WCPP、チェックサム）、チャンネル3、SF9/BW125、
透過モード、ACKの`'a' / St=0 / Ri=受信ID`は維持する。
TrackerのACK対象は`Ss`付きの`'t'`/`'c'` **command** のみ。

従来の`sendPacket('s' + Pa, listener)`は汎用command listenerの有限キューへ投入するだけで、
呼び出し側には受理成否が返らない。今回のTracker/GOLIDEN Mainは
`queuePacket(payload, priority)`を呼び、失敗したACK/PCコマンドを保持して次のloopで再試行する。
内部で`Pa`を包む処理が不要になっただけで、無線に載るpayload形式は同じ。
他の呼び出し元向けには従来の`'s' + Pa`インターフェースも残してある。

ドライバは以下の順に動く。

1. UART受信を処理し、検証済みパケットに`Ss`を付与してカーネルへ1回配信する。
2. Trackerの対象アップリンクなら、Mainに渡す前に`awaiting_ack_`を増やす。
   ACKがキューに入るまでテレメトリを停止し、タスク切り替えの隙間を防ぐ。
3. AUX Low、UART未読データ、送信後の待機時間中は送信しない。
4. ACKキュー、通常送信キュー、テレメトリ1件の順に選択する。
5. UART書き込み直前にも状態を再確認し、E220が受理しなければキューに残す。

ACK/通常キューは各8件、Mainの受信listenerは32件。キュー操作と送信開始はFreeRTOS mutexで排他制御し、
UART/E220へのアクセスはLoRaタスクだけが行う。従来の無限busy待ちと、2秒busyでの送信破棄を撤去した。
busyが続く間は`LoRa busy: TX retained for retry`を最大2秒に1回出す。

送信後は`200 + (WCPPサイズ + 16) * 10` msの待機時間に加えて、
AUX High・UART未読なしが100 ms続くまで次の送信を開始しない。
これは現在のSF9/BW125設定向けの保守的な初期値で、厳密なRF完了通知や測定値ではない。
搭載E220のファームウェア、キャリアセンスによる待ち、実測AirTimeに合わせて確認・調整する。
通常の受信待機状態そのものはbusy扱いせず、AUX/UARTで観測できる受信処理をbusy扱いする。

AUXは内部バッファの状態を表し、Highだけでは空中送信完了や未検出の電波がないことまでは判定できない。
根拠: [E220-900T22S(JP) データシート、6.9節](https://dragon-torch.tech/wp-content/uploads/2024/09/DS240911JA_E220-900T22Sv1_Rev2.1.1.pdf#page=49)。

送信中のテレメトリ電波を途中で中断してACKへ切り替える処理は行わない。
ACK最優先は「受信済みアップリンクへのACKを、次の送信機会に優先する」という意味。

## 3. GOLIDENからPCへの経路とループ防止

`LoRa::loop() -> kernel -> SerialBus::all_packets -> USB -> PC`が元から存在する。
GOLIDEN Mainの`Ss`判定はMainから無線へ再送しないための判定であり、SerialBus配信を止めない。
ACK `'a'`、定期テレメトリ`'M'`、その他の受信IDをこの同じ経路で転送する。
Mainから受信パケットを再び`sendPacket()`するとPCへ二重転送されるため、再配信しない。

GOLIDENの無線送信対象をcommandに限定し、地上局自身のGPS、電源、LOGが無線帯域を消費しないようにした。
これらのローカルテレメトリは従来どおりSerialBusからPCへ流れる。
大きすぎるコマンド（245 byte超）はログを出して拒否し、後続コマンドを停止させない。

直接キューへ入れた送信要求はカーネルへ戻らないため、自己受信・再ラップは発生しない。
追加したGOLIDENログの配信は`sendPacket(trace, tx_listener_)`で自分を除外する。
Trackerの受信listenerはcommand限定なので、自分のテレメトリ・ログを拾わない。
LoRaにはcatch-all listenerを設けず、RF受信の`Ss`付きパケットを`onCommand()`で再送させない。

## 4. ログと検証

| ログ | 意味 |
| --- | --- |
| `[Tracker] Telemetry sent (UART accepted, Seq: ...)` | E220へのUART送信を受理。PC到達の保証ではない |
| `[Tracker] Uplink received & ACK sending... (Packet ID: ...)` | ACK優先キューへの投入成功 |
| `[GOLIDEN] LoRa Rx -> SerialBus Tx (Packet ID: ...)` | LoRa由来パケットをMainも観測。SerialBusは独立して転送 |

LOGは既存フレームワークの`'#'`パケット（文字列エントリ`Ms`）。
USB Serialへ平文を混在させない。既存の`util.py`等、WCPPを復号するモニタで確認する。
GOLIDENの追加ログも同じ`'#' / Ms`形式で、自listenerを除外している。

E220受信処理では、同一長のパケットを繰り返し受けたときに以前の受信タイムアウトを使い続けないよう、
受信完了ごとに長さ追跡をリセットした。未初期化だった追跡変数も初期化した。

確認済み:

- `pio run -e Tracker -e GOLIDEN`: 両環境でビルド・リンク成功。
- `tests/lora/test_lora.cpp`: 実際のLoRa/E220/WCPP実装を模擬UART/AUXで動かす11シナリオが成功。
  ACK優先、10秒busy後の復帰、RXからACK生成までの待機、UART部分受信、キュー満杯時の再試行、
  AUXがHighのままでもTXを重ねないこと、送信直前のbusy、mutex競合、旧Pa形式の寿命、
  2秒周期、millis桁あふれ、不正フレーム、同一長RSSI受信30秒継続を含む。
- 初期検証時点では実機未確認。その後、ユーザーが実機で定期テレメトリとアップリンク／ACKの併用を確認した。並行動作の網羅的検証、電波衝突・AirTimeの測定は未実施。

テスト再実行（ホスト用C++14コンパイラがある場合）:

```powershell
python tests/lora/run_tests.py --cxx g++
```

この環境では既存Unity付属EmscriptenでWebAssemblyにコンパイルし、Node.jsで実行した:

```powershell
& C:/Users/numat/.platformio/penv/Scripts/python.exe tests/lora/run_tests.py --emsdk 'C:/Program Files/Unity/Hub/Editor/6000.0.45f1/Editor/Data/PlaybackEngines/WebGLSupport/BuildTools/Emscripten'
```

実機では、定期送信を60秒以上継続させた状態でPCから`'t'`/`'c'`を1件ずつ送信し、
各ACKの`St`/`Ri`、テレメトリ連番、重複がないこと、10秒を超えて受信が継続することを確認する。
次に送信時刻をテレメトリ周期の前後へずらし、ACK遅延と欠落を測定する。
PCコマンドは前のACKを確認してから次を送り、無制限に投入しない。

有限キューを超える連続投入、PC側の受信停止、無線上の同時送信について無損失は保証しない。
今回の実装は、ローカルのbusyを理由に受理済みACKを捨てないためのもの。
電波上の配送保証には、PCとの時分割・ACKタイムアウト再試行・重複排除などの追加プロトコルが必要になる。
