# 気圧高度によるIGN開始条件

`src/modules/IGN/main.cpp`の`ignition_altitude_m`で点火高度（m）を設定する。
コンストラクタの引数順序は次のとおり。

```cpp
component::IGN ign(Wire, ign_normal_pin, ign_high_pin, ign_low_pin, unit_id,
                   15000, 1); // 点火高度15000m、電力計測1Hz
```

IGNが同一ユニットのPressureテレメトリ（component `0x25`、packet `E`）を
リスナーで購読し、整数エントリ`PA`を使用する。
Mainタスクから`altitudeConditionMet(removed)`を毎ループ呼び出して受信処理を進める。
このメソッドは単一タスクから使用する。

開始条件は次の両方が成立した場合。

- フライトピンの挿入（HIGH）を観測後、抜去（LOW）を観測し、現在のGPIOもLOW。
- 抜去後、`PA > ignition_altitude_m`を連続30パケット確認。

挿入中と抜去確認直後の保留パケットは破棄する。
再挿入、`PA`がしきい値以下、`PA`の欠落・型不正で回数をリセットする。
1Hzで受信する場合は約30秒に相当するが、経過時間ではなく受信回数で判定する。
受信が途切れている間は回数を進めず、タイムアウトによるリセットは行わない。

条件成立後は既存の`startSequence()`を呼ぶ。警告・カウントダウンの時間、
フライトピン再挿入ISRによるアボート、開始直後のピン再確認、再点火禁止は維持する。
`begin(true)`や`startSequence()`自体の明示的な開始APIは従来どおりで、
今回のAND条件はIGNモジュールのMainが適用する。

ホストテスト：

```powershell
g++ -std=c++14 -Wall -Wextra -pedantic -I . tests/ign_altitude_test.cpp -o "$env:TEMP\wobc_ign_altitude_test.exe"
& "$env:TEMP\wobc_ign_altitude_test.exe"
```

実機向けビルド：`platformio run -e IGN`。
