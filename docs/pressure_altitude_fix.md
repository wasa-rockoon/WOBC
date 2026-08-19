# Pressureコンポーネントの気圧高度計算修正

## 概要

Pressureコンポーネントにおいて、気圧が変化しても気圧高度（`PA`）がほとんど変化しない問題を修正した。

- 修正日: 2026-08-19
- 対象ファイル: `src/components/Pressure/pressure.cpp`
- 対象関数: `Pressure::height(float pressure)`

## 原因

気圧が基準表の上限である `101325 Pa` 以上の場合、高度の二次近似式ではなく、その微分値（傾き）を返していた。

修正前の処理:

```cpp
return 2 * coe[0].a * (pressure - p[0].pressure) + coe[0].b;
```

この式の気圧に対する変化量は非常に小さい。そのため、測定気圧と海面気圧の両方が `101325 Pa` 以上の場合、次の計算結果がほぼゼロになっていた。

```cpp
pressure_.height(pres) - pressure_.height(sealevel_Pa)
```

さらに、送信時に気圧高度を整数へ変換しているため、微小な計算結果は `0 m` になっていた。

```cpp
packet.append("PA").setInt((int)pressureAlt);
```

## 変更内容

`101325 Pa` 以上の場合にも、ほかの気圧範囲と同じ二次近似式を使用するよう変更した。

修正後の処理:

```cpp
if (pressure >= p[0].pressure) {
    const double pressure_delta = pressure - p[0].pressure;
    return coe[0].a * pow(pressure_delta, 2)
         + coe[0].b * pressure_delta
         + coe[0].c;
}
```

これにより、標準気圧より高い領域でも気圧変化に応じて高度が変化する。

## 確認結果

PlatformIOで次の環境をビルドし、どちらも成功した。

- `Tracker`: SUCCESS
- `IGN`: SUCCESS

## 補足

気圧高度 `PA` は現在も整数メートルで送信される。海面付近ではおよそ `12 Pa` の変化が高度 `1 m` に相当するため、それより小さい気圧変化はテレメトリ上で見えない場合がある。

作業前から存在していたデフォルト海面気圧の変更（`100310 Pa` から `101542 Pa`）は、この修正では変更していない。
