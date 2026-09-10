# Separation CANの切り分け

通常環境は `env:Separation`。USB SerialはWCPPバイナリ用なので、
`Serial.printf` による文字ログを混ぜない。

## 文字表示で実機を調べる

`env:SeparationCanDiagnostic` をSeparation基板に書き込む。
この環境はSerialBus、kernel、コマンド処理を起動せず、GPIO47/48をLOWに保つ。
CANのアプリケーション送信はしないが、通常CANモードなので受信ACKは返す。
保存済みmodule IDは読み取りのみで変更しない。

```powershell
pio run -e SeparationCanDiagnostic -t upload --upload-port COM番号
pio device monitor -b 115200 -p COM番号
```

送信側から既存のheartbeatなどを送る。設定はRX=GPIO44、TX=GPIO43、125000 bps。
1秒ごとに集計し、生フレーム表示は毎秒最大20件。RXは診断アプリまで届いた
フレーム数であり、表示省略分も含む。高速通信ではライブラリ内部のキューでも
欠落し得るため、全フレームを記録する用途では使わない。

| 表示 | 判断と次の確認 |
| --- | --- |
| `id=MISMATCH` | 保存IDと期待値0x53が不一致。通常SeparationはCAN初期化前のassertで停止する。基板の用途と保存IDを確認してからID変更を判断する。 |
| `id=MATCH` | 保存IDは一致。CAN初期化・受信側の確認へ進む。 |
| `id=not_stored` | 保存IDなし。通常環境は初回起動時に0x53を書き込む。 |
| `id=KVS_absent_or_open_failed` | KVS未作成か読み取り開始失敗。これだけでは区別できない。 |
| `id=invalid_or_read_failed` | 保存データ長または読み取りに問題あり。 |
| `CAN=INIT_FAILED` | CANドライバ初期化失敗。配線以前に初期化ログ・リソースを確認する。 |
| `CAN=OK RX=0` | 初期化成功だけでは受信確認にならない。送信側が送っているか、ビットレート、CAN H/L、共通GND、トランシーバ電源、終端を確認する。 |
| `RX` と `EXT` が増える | 拡張CANフレームがアプリに到達した。通常環境へ戻しWCPP復元・宛先を調べる。 |
| `RX` のみ増える | 標準フレームなどを受信。通常WCPP CANBusは拡張データフレームのみ処理する。`rtr=1` も対象外。 |
| `REC` / `bus_errors` が増える | CANコントローラがエラーを検出。配線・ビットレートなどを切り分ける。値だけで故障箇所は断定しない。 |

`TWAI state` は0=停止、1=稼働、2=bus-off、3=復旧中。
`TEC` は送信エラーカウンタ、`REC` は受信エラーカウンタ。
診断環境では送信とWCPPパケット復元・LEDコマンド・ACKの動作は検証しない。

## 通常環境での確認

```powershell
pio run -e Separation -t upload --upload-port COM番号
```

module IDは0x53、unit IDは0x41。LEDコマンドの宛先はunit IDの0x41であり、
module IDの0x53ではない。component ID=0x00、command ID='n'、整数 `On=0/1`。
remoteコマンドで、送信元unit IDはlocal (0x00) 以外が必要。

通常環境で生受信を追う場合、`CAN::update()` の `CAN0.read()` だけでは
全受信を観測できない。使用中のESP32_CANは、フィルタに登録したcallbackが
あればcallbackキューへ、それ以外はreadキューへ振り分ける。
診断環境ではcallbackを登録せず、標準・拡張ともreadキューで確認する。

ソフトウェアのビルド／パケット分割復元テスト成功は、実機CAN受信の証明にはならない。

## 2026-09-10 実機ログの確認結果

COM29の診断ログで `id=MATCH CAN=OK`、RX/EXTが8から24まで増加。
観測中のTEC、REC、bus_errors、rx_missed、rx_overrunはすべて0。
診断環境の実機受信と保存module ID一致は確認済み。

受信 `id=04498000 data=07 09 4D 4C` を現在のWCPPで復元すると
`07 22 4C 00 09 4D 4C`。packet ID=0x22、component=0x4C、origin=0x00、
`Im=0x4C` で、内容はLモジュールのheartbeatに合致するが、
type_and_id=0x22のため **command扱い** になる。
同じ内容の正しいtelemetryはtype_and_id=0xA2、CAN ID=`0x14498000`。
この復元結果はリポジトリのPython Packet.decodeでも確認した。

送信側または中継側でtelemetryビットが落ちている疑いが強い。
特にCAN送信側に旧 `packet.packet_id() << 21` 実装のファームウェアが
残っていないかを確認する。Separationだけを書き換えても送信済みIDは直らない。
Lモジュールの実際の基板・使用envを確認し、`type_and_id()` 修正済みの
対応ファームウェアを書き込んでから診断ログのIDを再確認する。
このログにはLEDコマンドはなく、通常環境の復元・応答・CAN送信は引き続き未確認。
