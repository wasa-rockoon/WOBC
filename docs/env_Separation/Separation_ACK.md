# Separation 通信確認ACK

`SeparationAck` は通信確認コマンドを受理するとACKを返す。GPIO操作やNichrome呼び出しは行わない。現在の分離開始条件・フライトピン処理は維持する。

## コマンド仕様

| 項目 | 値 |
|---|---|
| 型 | remote command |
| component | `0x26`（本ブランチで通信確認用に割当） |
| packet | `'q'` (`0x71`) |
| origin | `0x64`（現在のGS） |
| destination | `0x41`（Separation） |
| sequence | 送信側で採番する16bit値。再送時は同じ値 |
| payload | なし（WCPP全体7バイト） |

例: `packet.command('q', 0x26, 0x64, 0x41, 1);`
WCPPバイト列は `07 71 26 64 41 01 00`。CANでは拡張ID `0x0E24CC80`、データ `07 41 01 00`、DLC=4、125kbpsとなる。

上記以外の型・ID・送信元・宛先・payloadには応答しない。旧 `component=0x00 / 'n' / On` は非対応。

## ACK仕様

remote telemetry `'a'` (`0x61`)、component `0x26`、origin `0x41`、destination `0x64`。ヘッダーsequenceは元コマンドと同じ。

| entry | 意味 |
|---|---|
| `Ri` | 元コマンドID `'q'` (=113) |
| `Sq` | 元コマンドsequence |
| `St` | `0`: 通信確認を受理。実分離の成功を意味しない |
| `Dp` | 初回 `0`、重複 `1` |

直近16件の異なるsequenceをRAMに保持する。同じsequenceの再送にもACKを返す。16件を超えて追い出された値、再起動後の値は初回扱い。受理対象の送信元・宛先・IDは固定なのでsequenceだけで重複を判定できる。

## 接続と確認

Separationの既存CAN・SerialBusからkernelに入ったコマンドを処理し、ACKをkernel経由で各Busへ送る。CAN RX/TXはGPIO44/43。CAN初期化失敗は `cbInit` として記録し、未初期化driverへの送受信処理を停止する。

今回の実装範囲はSeparation側。LoRa二系統の移植やGround/Flightの転送・照合処理は含まない。無線経由で利用するときは両端をこの仕様に合わせ、ACKのcomponent・Ri・Sq・送信元・宛先も照合する。

ビルド: `pio run -e Separation`

ホストテスト（リポジトリルート）:

```powershell
g++ -std=c++14 -Isrc tests/separation_ack_test.cpp src/library/wcpp/cpp/packet.cpp src/library/wcpp/cpp/float16.cpp -o .pio/separation_ack_test.exe
.pio/separation_ack_test.exe
```

実機では分離負荷を外して、正常コマンド→ACK、同一sequence再送→Dp=1、不正宛先・旧LEDコマンド→応答なしを確認する。既存の自動分離処理は独立して動作する。
