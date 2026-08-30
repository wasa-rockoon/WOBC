# パケット・シーケンス番号

## 目的

シーケンス番号は、同一種別のパケットを受信側で並べ、欠落・重複を検出するための
16-bit カウンタです。たとえば IGN の電源テレメトリが `41` の次に `44` と受信された
場合、`42` と `43` の 2 パケットが欠落したと判断できます。

このプロジェクトでは、全パケット共通のカウンタにはしていません。GPS や気圧などの
別テレメトリが間に送信されても、IGN 電源テレメトリの連番に欠番が生じないようにする
ためです。

## 管理単位

シーケンス番号は、次の 4 項目が一致するパケット系列ごとに `0, 1, 2, ...` と進みます。

| 項目 | 説明 |
| --- | --- |
| origin unit ID | 送信元ユニット |
| destination unit ID | 宛先ユニット |
| component ID | 送信コンポーネント |
| type and packet ID | command / telemetry の種別と packet ID |

したがって、同じ packet ID でも command と telemetry は別系列です。各系列は
`65535` の次に `0` へ周回します。

## 実装

カーネルの `kernel::nextPacketSequence()` が系列ごとの次の番号を返します。
実装は [kernel.h](../../src/library/kernel/kernel.h) と
[kernel.cpp](../../src/library/kernel/kernel.cpp) にあります。

```cpp
uint16_t kernel::nextPacketSequence(
    uint8_t origin_unit_id,
    uint8_t dest_unit_id,
    uint8_t component_id,
    uint8_t type_and_id);
```

カーネルは固定長の系列テーブルを使用するため、動的メモリは使用しません。標準では
32 系列まで同時に管理します。多数の packet 種別を持つモジュールでは、ビルドフラグで
`WOBC_PACKET_SEQUENCE_STREAMS` を必要数に増やしてください。

番号の取得と更新はカーネルの mutex で保護されているため、複数の FreeRTOS タスクから
呼び出しても同じ系列に同じ番号は発行されません。

## IGN での使用例

IGN の電源テレメトリは、[IGN.cpp](../../src/components/IGN/IGN.cpp) で次のように生成します。

```cpp
packet.telemetry(
    Powertelemetry_id,
    ign_.component_id,
    unit_id_,
    0xFF,
    kernel::nextPacketSequence(
        unit_id_,
        0xFF,
        ign_.component_id,
        wcpp::packet_type_mask | Powertelemetry_id));
```

`wcpp::packet_type_mask | Powertelemetry_id` とすることで、telemetry であることも系列の
識別子に含めます。command を生成するときは packet ID に `packet_type_mask` を付けずに
渡してください。

## ローカルパケットの扱い

ローカル形式のパケットはヘッダが 4 byte で、sequence フィールドを持ちません。
Telemeter と Logger はローカルパケットをリモート形式のヘッダへ変換する際に、送信元・
宛先・component・type/packet ID に対応する sequence を新規に割り当てます。

すでにリモート形式で届いたパケットは転送・記録時に sequence を変更しません。これに
より、受信済みパケットの sequence が中継経路によって書き換わることを防ぎます。

## 受信側での判定

受信側も送信側と同じ 4 項目で系列を分けて、各系列の直前の sequence を保持します。

- 期待値と一致: 正常
- 期待値より先: 途中のパケットが欠落した可能性がある
- 同じ値: 重複パケットの可能性がある
- `65535 -> 0`: 正常な周回

シーケンス番号は RAM 上のカウンタです。モジュールを再起動すると全系列が `0` から
始まるため、受信側では unit の再起動を別途検出するか、タイムスタンプと組み合わせて
判定してください。
