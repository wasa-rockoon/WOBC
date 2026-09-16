# LoRa役割分担変更・PC ↔ Separation通信 実装／試験ログ

記録日：2026年9月16日  
対象：GS、Ground LoRa、Flight LoRa、Separation  
作業者：shoko (Codex)

## 目的と変更した役割

以前はLoRa1をアップリンク専用、LoRa2をダウンリンク専用とし、Separation ACKもLoRa2で返していた。一般テレメトリのダウンリンクをACK応答で塞がないため、GOLIDEN／Trackerで成功した一つのLoRaによる半二重通信をLoRa1へ応用した。LoRa1でcommandとACKを往復させ、LoRa2は従来どおり一般テレメトリのダウンリンク専用とする。

| 無線 | 現行の役割 | 地上側 | 飛行側 | UART | channel |
|---|---|---|---|---:|---:|
| LoRa1 | commandアップリンク＋ACKダウンリンク（半二重） | command TX／ACK RX | command RX／ACK TX | 2 | 10 |
| LoRa2 | 一般テレメトリ専用ダウンリンク | telemetry RX | telemetry TX | 1 | 12 |

channel 3はTracker用、channel 11はMissionBus用としてLoRaには割り当てない。MissionBusのchannel 11は変更しない。GS／Ground LoRaのunit IDは`0x64`、Flight LoRa／Separationは`0x41`。

```text
command: PC → GS → CAN → Ground LoRa → LoRa1 → Flight LoRa → CAN → Separation
ACK:     PC ← GS ← CAN ← Ground LoRa ← LoRa1 ← Flight LoRa ← CAN ← Separation
M/E/F:   PC ← GS ← CAN ← Ground LoRa ← LoRa2 ← Flight LoRa ← CAN ← Separation
```

## 実装した内容

- `src/components/Uplink/uplink.h/.cpp`を新設し、Ground／Flightの役割を切り替えるLoRa1コンポーネントとした。`src/modules/LoRa_ESP/main.cpp`がRadioConfig、channel、unit IDを与えて起動する。
- Groundは宛先`0x41`のremote commandを送信キューへ入れ、現在のcommandに対するACKを待ってから次を送るstop-and-wait制御を行う。送信後は受信へ戻り、E220 AUX／UARTのbusy状態と送受信ターンアラウンドを見て半二重衝突を避ける。
- ACK待機は5秒、同じcommandとsequenceを最大3回送信する。ACKの`Ri`（command ID）、`Sq`（sequence）、ACK宛先などを照合し、成功時は`[UPLINK ACK MATCH]`を記録する。タイムアウト時は`[UPLINK ACK TIMEOUT]`を記録し、上限では`upTO`を記録する。
- FlightはLoRa1でcommandを受けてCANへ流し、Separationが生成したACK `0x61 ('a')`を優先してLoRa1で地上へ返す。Groundは受信ACKに無線RSSIの`Ss`を付けてCAN→GS→PCへ渡す。
- `src/components/LoRa/downlink.h/.cpp`はLoRa2の一般テレメトリ経路に分離した。ACK `a`はLoRa2へ載せず、無線を通過済みの`Ss`付きパケットは再送しない。これによりCAN上のパケットが同じ無線へ戻るループを避ける。
- Separationは安全な通常LED command `n`、component`0x00`を処理し、結果の`St`と重複状態`Dp`をACKへ入れる。同一origin・command ID・sequenceの直近8件をRAMに保持し、重複時にはLED操作を再実行しない。電源を入れ直すと、この重複キャッシュは消える。
- GS側にはFだけを間引く設定やテレメトリ専用の件数制限はない。一方、CAN／SerialBusなどの共通送信待ちキューは有限であり、経路全体が無損失であることを意味しない。

## 実機でできたこと

安全のためSeparationの分離出力GPIO47/48は作動させず、通常LED command `n / On=0`で検証した。PCからorigin`0x64`、destination`0x41`のcommandがLoRa1→Flight CAN→Separationへ届き、Separationで`On=0`と負の`Ss`を確認した。Separation ACKはFlight LoRa→LoRa1→Ground LoRa→GS（COM9）まで届いた。ACKの`Ri=110`、`Sq=sequence`、`St=1`、負の`Ss`から、PCまでの往復経路が成立した。

Step 10では、COM9でSeparation起源の`M`（GPS）、`E`（Pressure）、`F`（FlightPin）を監視しながら、sequence 21～25のcommandを各ACK確認後に送った。ACK前後にもM・Eを受信した。Fも一時停止はあるが更新を再開し、完全に止まったわけではなかった。

Step 11ではsequence 30の`n / On=0`を用いた。COM9のACKで初回`Dp=0`、同じsequenceの再到着後`Dp=1`を確認した。COM19では`[UPLINK ACK TIMEOUT] ... retry=2/3`、同一sequenceの再TX、`[UPLINK RX]`、`[UPLINK ACK MATCH] ... attempts=2`を確認した。送信、受信、照合のログも複数回分観測された。したがってタイムアウト後の再送、ACK照合、Separationの重複判別は実機で成立した。`On=0`は同じ値を繰り返し設定する試験なので、物理操作回数の測定ではなくACKと実装上の重複判別による確認である。

## 躓いた点と判断

- 初期のLoRa2経由ACKでは、PC側とFlight側に見えるパケットが一致せず、どこまでACKが届いたかの切り分けが必要だった。現行構成ではACKをLoRa1へ移し、Groundの`ACK MATCH`とPC側ACKを別々に確認するようにした。
- commandのGround TXログだけではSeparationの受信を証明できない。Flight側`[UPLINK RX]`、Separationのcommand、PC側のACKまでを順に照合した。実際に「Separationで見つからない」と思ったcommandも、後からCOM12のログで確認できた。
- COM9にACKが表示されても、Groundが待機中のcommandに対してACKを照合したかは別問題。COM19の`[UPLINK ACK MATCH]`で確認した。Step 11ではタイムアウト後`attempts=2`で照合された。
- Step 10のFは、約2秒周期で5～6回更新した後、10秒強停止する状態が見られた。別画面ではFlight監視のF累積282件に対してGS側94件だった。ただし観測時間が厳密に一致せず、Flight側の受信件数はLoRa2送信成功件数ではないので、これだけで損失率や原因は確定できない。ユーザーは「Fは1更新あたりのパケット数が多いため、今回は気にしない」との説明を受け、**Fの累積件数差を今回の合否条件から外す**と判断した。観測事実だけは今後の参考として残す。
- sequence 30を何度も新規送信に使うと、旧ACKと新しい待機commandの対応が読みづらい。重複試験以外では新しいsequenceを使う。

## 結論と未確認事項

**PC→Separationのcommand、Separation→PCのACK、LoRa1での同一sequence再送、Separationの重複判別を実機で確認し、双方向通信の実装は成功した。** LoRa2の一般テレメトリもACKと並行して受信できた。ただし「すべての条件で安定・無損失」という判定ではない。

ACKが3回とも届かない場合の`upTO`打ち切りと、その後の新sequenceでの復帰は未試験。安全なACK遮断機構がないため、電源断やアンテナ取り外しで無理に再現しない。距離・遮蔽物を変えた無線品質、定量的な長時間到達率・遅延、起動順やCAN復旧の反復耐久試験も未実施。分離出力の作動試験はこの通信試験の対象外である。
