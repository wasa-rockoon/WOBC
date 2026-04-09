# Packet::telemetry(2引数版) と Packet::telemetry(5引数版) の違い

作業者：narrow

---

## 目標
- `Packet::telemetry(uint8_t packet_id, uint8_t component_id)`
- `Packet::telemetry(uint8_t packet_id, uint8_t component_id, uint8_t origin_unit_id, uint8_t dest_unit_id, uint16_t sequence)`
の違いが知りたい


---

## 実装の違い

### 2引数版（ローカル形式）
実装: `src/library/wcpp/cpp/Packet.cpp` の 398-404 付近

- `buf_[1] = packet_id | packet_type_mask`（telemetry種別ビットを立てる）
- `buf_[2] = component_id`
- `buf_[3] = unit_id_local`
- `resize(0, 4, buf_[0])` によりヘッダ長4バイトにする

### 5引数版（リモート形式）
実装: `src/library/wcpp/cpp/Packet.cpp` の 406-416 付近

- `buf_[1] = packet_id | packet_type_mask`（telemetry種別ビットを立てる）
- `buf_[2] = component_id`
- `buf_[3] = origin_unit_id`
- `buf_[4] = dest_unit_id`
- `buf_[5] = sequence & 0xFF`
- `buf_[6] = sequence >> 8`
- `resize(0, 7, buf_[0])` によりヘッダ長7バイトにする

### 共通点
- どちらも telemetry パケットを作る（種別は同じ）
- 違いは「ヘッダ情報量（4バイト or 7バイト）」

---

## 内部判定への影響

`packet.h` 側では以下の判定になっている。

- `header_size()` は `isLocal() ? 4 : 7`
- `isLocal()` は `buf_[3] == unit_id_local (0x00)`

つまり、5引数版であっても `origin_unit_id` に `0x00` を入れると local 扱いになる。
その場合、`dest_unit_id()` や `sequence()` の見え方が意図とズレる可能性があるため、通常は `origin_unit_id` に実際のユニットIDを入れる。

---

## 使い方の違い

### 2引数版を使う場面
- 同一ユニット内の内部バスで完結するテレメトリ
- 余計なヘッダを持たせず軽くしたいとき

### 5引数版を使う場面
- ユニット間転送や中継を想定するテレメトリ
- `origin/dest/sequence` を明示したいとき
- ログ解析時に経路情報を残したいとき

---

## 今回の LiPoPower の行について

対象行:

```cpp
packet1.telemetry(Powertelemetry_id, lipo_power_.component_id, unit_id_, 0xFF, 1234);
```

これは **5引数版** を使っており、

- `origin = unit_id_`
- `dest = 0xFF`
- `sequence = 1234`

を明示したテレメトリになっている。

---

## 参照ファイル
- `src/library/wcpp/cpp/Packet.cpp`
- `src/library/wcpp/cpp/packet.h`
- `src/components/LiPoPower/lipo_power.cpp`
