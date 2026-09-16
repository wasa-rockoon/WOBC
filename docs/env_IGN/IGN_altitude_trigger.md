# 高度・更新停止・タイマーによるIGN開始条件

## 設定

`src/modules/IGN/main.cpp`で気圧高度と代替条件の待ち時間を変更する。

```cpp
constexpr int32_t ignition_altitude_m = 15000; // m、以上で判定
constexpr uint32_t ignition_fallback_delay_ms = 60UL * 60UL * 1000UL; // 1時間
```

例えば30分にする場合は`30UL * 60UL * 1000UL`とする。
タイマーの起点は、MainがフライトピンのLOWを確認して監視を開始した時刻。
電源投入時刻や、気圧高度の更新が止まった時刻ではない。
従来どおり、先にHIGHを観測してからLOWを観測する必要がある。

## 開始条件

```text
フライトピン抜去確認済み、かつ現在のGPIOがLOW
AND
(
  抜去後の有効な気圧高度が15,000m以上を連続30回
  OR
  (
    有効な気圧高度が60秒以上更新されていない
    AND LOW確認から1時間以上経過
    AND MissionBusのGPS高度が10,000m以上（UT更新・受信鮮度確認付き）
  )
)
```

通常の高度条件には1時間の待ち時間もGPSも不要。
代替条件は3条件が同時に成立した場合のみ有効で、1時間到達だけでは開始しない。
1時間到達時に条件が不足していても、後から揃えば開始する。

挿入中と抜去確認直後の保留パケットは破棄する。再挿入・ピン状態不正では
回数、LOW起点のタイマー、測定状態をリセットする。
開始済みシーケンスの再挿入ISRによる中止と、同一ブート中の再点火禁止は維持する。

条件成立後は既存の`startSequence()`を呼ぶ。現在の`IGNSequence.h`では
1秒のStartup、60秒のCountdown、5秒のFinalを経て通電するため、
条件成立から通電までは最短66秒。代替条件の1時間とは別の時間である。

## 気圧高度の扱い

- 同一unitのPressure（component `0x25`、telemetry `'E'`）を購読する。
- `Sm`がIGN自身のmodule ID（`'I'`）、`Va=1`、整数の`PA`・`Ts`が必要。
  MissionBusにも同じunit/componentのPressureがあるため、`Sm`で区別する。
- 新しい測定の`Ts`ごとに1回だけ数える。`PA`が同じ数値でも新規測定なら更新。
- 15,000mちょうどは数える。しきい値未満・無効測定で連続回数をリセットする。
- 抜去前、未来、60秒以上古い測定は使用しない。重複・逆順の有効測定も数えない。
- 無受信中は回数を進めない。従来どおり、無受信だけでは連続回数をリセットしない。
- 最後の有効測定から60秒以上経過すると更新停止。一度も有効測定がなければ、
  LOW確認時刻から60秒を数える。更新が再開したら代替条件は成立しなくなる。

IGN側のPressureはセンサ・高度計算係数の初期化後に測定を開始する。
非有限値や高度計算の範囲外などで高度が無効なら`Va=0`、`PA=null`を送る。
無効パケットの送信が続いても、IGNの最終有効測定時刻は更新しない。
更新停止はデータ利用不可の判定であり、センサ自体の故障を断定するものではない。
正常値に見える固定値をセンサが返し続ける故障の検出は含まない。

## GPSの扱い（MissionBusの変更・書き換え不要）

同一unit（現在`0x62`）のGPS（component `21`、telemetry `'M'`）を購読する。
このGPSをMissionBusだけが送信する現在の構成を前提とする。
既存パケットには送信元module IDがないため、module単位での識別はできない。

| フィールド | 内容 |
|---|---|
| `AL` | GPS高度、整数m |
| `UT` | GPS時刻、`YYYY-MM-DD hh:mm:ss.cc` |

整数の`AL`と正しい形式・日付の`UT`が必要。初期値の日付や欠落・型不正は使わない。
`UT`が以前より進んだパケットだけを新しい受信とし、同じ時刻や逆順の再送では
受信時計を延長しない。

`IGNStartGate.h`の`gps_max_age_ms`は5,000ms。
Mainのポーリング間隔をキュー滞留時間の上限として、受信処理後の経過時間に
加算し、5秒未満の場合だけ使う。CAN転送自体の遅延は計測していない。
これは受信停止・UT更新停止の検出であり、GPS高度自体の測定年齢ではない。
異なる基板の`millis()`や、GPS時刻とIGNの`millis()`を直接引き算しない。

**既存GPSパケットには高度の有効性・測定時刻がないため、GPS時刻だけ更新されて
高度だけが古い場合は検出できない。** MissionBusのGPS送信処理・パケット形式を
変更せず、IGNの受信側だけで対応する。MissionBusの書き換えは不要。

## 実装と検証

- `IGNStartGate.h`: ハードウェアに依存しない開始判定。
- `IGNGPSTelemetry.h` / `IGNGPSTime.h`: 既存GPSパケットとUTC文字列の検査。
- `IGN.cpp`: Mainタスクから`startConditionMet(removed, ignition_fallback_delay_ms)`を
  毎ループ呼び、パケット受信・型・気圧の送信元確認を行う。
- `begin(true)`や`startSequence()`は明示的な開始APIのまま。
  この複合条件はIGNモジュールのMainが適用する。

ホストテスト（リポジトリ直下で実行）：

```powershell
g++ -std=c++14 -Wall -Wextra -Werror -pedantic -I . tests/ign_altitude_test.cpp -o "$env:TEMP\wobc_ign_altitude_test.exe"
& "$env:TEMP\wobc_ign_altitude_test.exe"
g++ -std=c++14 -Wall -Wextra -Werror -pedantic -I . tests/ign_start_test.cpp -o "$env:TEMP\wobc_ign_start_test.exe"
& "$env:TEMP\wobc_ign_start_test.exe"
g++ -std=c++14 -Wall -Wextra -pedantic -I . -I src tests/ign_gps_telemetry_test.cpp src/library/wcpp/cpp/Packet.cpp src/library/wcpp/cpp/float16.cpp -o "$env:TEMP\wobc_ign_gps_test.exe"
& "$env:TEMP\wobc_ign_gps_test.exe"
```

実機向けビルド：`platformio run -e IGN`。
ホストテスト・ビルドと、実機でのCAN通信・センサ故障・GPIO動作の検証は別途行う。
