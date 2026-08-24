# PCで作成したWCPPパケットをSerialBusへ流すための実装メモ

作成日：2026-06-26

作業者：Aoyama (GitHub Copilot)

関連：
- [20260307_PC_Serial.md]
- [20260618_PC_to_MCU_packet_send.md]

---

## 目標

PC側でWCPPパケットを作成し、既存の `.command -> util.py -> Serial` 経路でWOBCの `SerialBus` に投入できるようにする。

---

## 今回の変更内容（実ファイル）

### 1) 送信フレーム化処理を共通化

- 追加: `src/library/wcpp/python/transport.py`
  - `frame_packet(packet)`
    - 役割: 送信フレーム `[packet.encode()][packet.checksum()][0x00]` を1回で作る。
  - `enqueue_packet(packet, command_path='.command')`
    - 役割: `.command` に追記して flush する。

#### 変更の意味

これまで `packet.encode()` と `bytes([checksum, 0])` を個別に書いていたため、同じロジックが複数箇所に分散していた。
共通化により、送信フォーマットの定義を1箇所に固定できる。

---

### 2) パッケージ公開APIに送信ヘルパーを追加

- 変更: `src/library/wcpp/python/__init__.py`
  - `from .transport import enqueue_packet, frame_packet` を追加

#### 変更の意味

`from wcpp import frame_packet, enqueue_packet` で直接使えるようになり、PC側スクリプトからの利用が簡単になる。

---

### 3) util.pyの送信経路を共通処理へ統一

- 変更: `src/library/wcpp/python/util.py`
  - import に `frame_packet` を追加
  - REPLモードの `send_command()` を `f.write(frame_packet(packet))` に変更
  - `.command` から読み込んだパケットをシリアル送信する箇所も `ser.write(frame_packet(packet))` に変更

#### 変更の意味

REPL経由の送信、監視ループ経由の送信で、フレーム生成方式が完全一致する。
どの入口から送っても、SerialBusが期待するフォーマットになる。

---

### 4) 最小テストを追加

- 変更: `src/library/wcpp/python/test_packet.py`
  - `test_frame_packet`
    - `frame_packet(packet)` が `encode + checksum + 0x00` になっていることを確認
  - `test_enqueue_packet`
    - `enqueue_packet(packet, path)` で `.command` 相当ファイルに正しいバイト列が追記されることを確認

#### 変更の意味

フォーマット破壊（終端忘れ、checksum位置ずれ）を検出できる最低限の回帰ポイントを用意した。

---

## 使い方（今回追加した機能）

### A. util.pyを送信中継として起動

```bash
python util.py -p COM3 -b 115200
```

### B. 別プロセスでパケットを作って `.command` に投入

```python
from wcpp import Packet, Entry, enqueue_packet

p = Packet.command(packet_id=ord('A'), component_id=0x11)
p.entries.append(Entry('Cd').set_string('test'))

enqueue_packet(p)
```

`enqueue_packet` は内部で `frame_packet` を使うため、
必ず `[本体][CRC8][0x00]` の形で書き込まれる。

---

## 既存SerialBusとの対応

`src/library/core/serial_bus.cpp` の受信側は、
`0x00` 終端・サイズ整合・CRC整合を確認した後に `decodePacket()` して `sendPacket()` している。

今回の実装は、そこに渡すためのPC側フレーム生成を安全に定型化したもの。
受信側の仕様そのものは変更していない。

---

## 実施した確認

- Python構文確認
  - `py_compile` で以下4ファイルを確認
    - `__init__.py`
    - `transport.py`
    - `util.py`
    - `test_packet.py`
- 実行確認
  - `frame_packet` の末尾2バイト（checksum, `0x00`）が期待通りであること
  - `enqueue_packet` で出力ファイル内容が `frame_packet` と一致すること

補足：環境により `pytest` 未導入だったため、今回は直接実行で追加部分を検証した。

---

## 今回のポイント（引き継ぎ用）

- 送信フレームの定義は `transport.py` に集約した。
- 今後、PC送信処理を増やす場合は `enqueue_packet` を使う。
- 送信フォーマットを変える必要が出た場合も `frame_packet` だけ修正すればよい。

---

## 今後の改善候補

- `wcpp-send` のようなCLIを追加して、JSON/引数からパケットを直接作って `enqueue_packet` できるようにする。
- `pytest` 実行環境を固定（requirements整備）して、自動テストで `test_frame_packet` / `test_enqueue_packet` を常時回す。
