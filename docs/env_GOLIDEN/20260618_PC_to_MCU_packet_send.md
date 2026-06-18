# util.py を使った PC → MCU パケット送信

作成日：2026-06-18

作成者：Numata（ClaudeCode）

関連：[20260307_PC_Serial.md](20260307_PC_Serial.md)（受信側 PC→WOBC の解説）

---

## 概要

`src/library/wcpp/python/util.py` は通常 MCU からのテレメトリを受信・表示するツールだが、
**PC → MCU の送信機能** も組み込まれている。その通り道が `.command` という1つのファイル。

```
あなた ──→ .command ファイル ──→ util.py が読む ──→ シリアル経由で MCU へ送信
```

`.command` を「郵便ポスト」、util.py を「配達員」とイメージすると分かりやすい。
ポスト（`.command`）に手紙（パケット）を入れると、配達員（util.py）が MCU に届けてくれる。

---

## 仕組みの詳細

### シリアルは1つのプログラムしか開けない

「直接シリアルに書けばいい」と思うが、シリアルポートは1プロセスしか開けない。
util.py が受信表示でポートを占有しているので、送信したい内容は **ファイルに書いておき、
util.py がまとめてシリアルに流す** 設計になっている。

### 起動時：ファイル末尾にカーソルを置く

`util.py` 起動時（`util.py:105-106` 付近）:

```python
command_file = open('.command', mode='rb')
command_file.seek(0, 2)   # ファイルの「末尾」にカーソルを移動
```

→ **起動前から `.command` に入っていた古い内容は読まれない**。読むのは起動後に追記された分だけ。

### ループ内：新しく追記された分だけ読んで送る

`util.py:158, 165-166` 付近:

```python
command_data = command_file.read() or b''          # カーソル位置〜末尾を読む
command_data, packets = parse_packet(command_data)
for packet in packets:
    if ser and ser.isOpen():
        ser.write(packet.encode())                 # ① パケット本体
        ser.write(bytes([packet.checksum(), 0]))   # ② チェックサム + 0x00
        ser.flush()
```

`read()` は「今のカーソル位置から末尾まで」を読み、読んだ分だけカーソルが前に進む。

---

## よくある疑問：何度も送信される？

**されない。書いたパケットは util.py が次に読んだ瞬間に1回だけ送られて終わり。**

理由：ファイルの読み取りカーソルは前にしか進まない。

```
1回目のread → 新しく書かれた分を読む → カーソルが末尾へ進む
2回目のread → まだ何も足されてなければ b'' → 何も送らない
3回目のread → さらに追記されていればその分だけ読む → 1回送る
```

連続で送りたいなら `send()` を繰り返し呼ぶ。

> 補足：`.command` ファイル自体は消されず内容は溜まっていく（ファイルは大きくなる）が、
> カーソルが前にしか進まないため再送はされない。

---

## 送信フォーマット（ワイヤー形式）

MCU に送るバイト列は必ずこの順番：

```
[ パケット本体 ][ チェックサム1バイト ][ 0x00（区切り） ]
   encode()        checksum()           終端マーク
```

- `encode()` … パケットをバイト列に変換（`packet.py:357`）
- `checksum()` … CRC8（CCITT）の誤り検出用バイト（`packet.py:331`）
- `0x00` … 「ここでパケット終わり」の区切り

この3つをこの順で `.command` に追記すればよい。

---

## 使い方

### ステップ1：配達員（util.py）をシリアル接続で起動しておく

```bash
python util.py -p COM3 -b 115200
```

起動したまま放置でOK。これが MCU との通信係（受信＋送信）になる。

### ステップ2：別スクリプトでパケットを `.command` に書き込む

```python
from wcpp import Packet, Entry

# --- 空のコマンドパケットを作る ---
p = Packet.command(
    packet_id    = ord('a'),  # パケットの種類
    component_id = 0x01,      # 送り先のコンポーネント番号
)

# --- 中身（データ）を詰める。Entry の名前は必ず「2文字」 ---
p.entries.append(Entry('Vx').set_int(100))      # 整数
p.entries.append(Entry('En').set_bool(True))    # 真偽値
p.entries.append(Entry('Tp').set_float32(25.5)) # 小数

# --- .command に追記モード('ab')で書き込む ---
with open('.command', 'ab') as f:   # 'a'=追記, 'b'=バイナリ
    f.write(p.encode())                  # ① 本体
    f.write(bytes([p.checksum(), 0]))    # ② チェックサム + 0x00
    f.flush()                            # ③ すぐ書き出す
```

### 毎回書くのが面倒なら関数にする

```python
def send(packet):
    with open('.command', 'ab') as f:
        f.write(packet.encode())
        f.write(bytes([packet.checksum(), 0]))
        f.flush()

# 使い方
p = Packet.command(packet_id=ord('a'), component_id=0x01)
p.entries.append(Entry('Vx').set_int(100))
send(p)
```

これは REPLモード（`-r`）の `send_command` 関数（`util.py:71-74`）とまったく同じ中身。

---

## 書き込み時の注意点

| 注意点 | 理由 |
|---|---|
| Entry の名前は **2文字**（英字） | エンコード／デコードの仕様（`packet.py:189, 241-242`） |
| ファイルは **`'ab'`（追記）** で開く | `'wb'`（上書き）だと中身が消え、読み取り位置とズレて壊れる |
| `flush()` を忘れない | OSのバッファに溜まったままだと util.py がすぐ拾えない |

### 値のセッター一覧

| メソッド | 用途 |
|---|---|
| `.set_int(n)` | 整数 |
| `.set_bool(b)` | 真偽値（中身は整数） |
| `.set_float16/32/64(x)` | 小数（精度で使い分け） |
| `.set_string("...")` | 文字列 |
| `.set_bytes(b"...")` | 生バイト列 |
| `.set_struct([...])` | 構造体 |
| `.set_packet(pkt)` | ネストしたパケット |

---

## パケット作成メモ

- `Packet.command(packet_id=..., component_id=..., origin_unit_id=..., dest_unit_id=...)`
  でコマンドパケットを作成（`packet.py:379`）。
- `origin_unit_id=0`（既定）→ **ローカルパケット**。まずはローカルコマンドで機能試験がしやすい。
- `origin_unit_id` が非0 → **リモートパケット**（`dest_unit_id`, `sequence` も使われる）。

---

## 別の方法：REPLモード（`-r`）を併用する

REPLモード（`util.py:66-80`）は `send(packet)` で `.command` に書き込む。
ただし **REPLモードは serial を開かない**（`util.py:80` で `return`）。
なので2インスタンス同時起動が必要：

```bash
# ターミナルA: シリアル接続側（.command を監視して MCU へ転送）
python util.py -p COM3 -b 115200

# ターミナルB: REPL でコマンドを投入
python util.py -r
```

ターミナルB（REPL）内で：

```python
p = Packet.command(packet_id=ord('a'), component_id=0x01)
p.entries.append(Entry('Vx').set_int(100))
send(p)   # .command に書き込まれ、ターミナルA経由で MCU へ送信される
```

---

## まとめ

| やること | 役割 |
|---|---|
| ① `python util.py -p COM3` を起動しっぱなしにする | MCU との通信係（受信＋送信） |
| ② 別スクリプトで `.command` にパケットを追記する | 送信したい内容を投函 |

- **再送はされない** → 1回 `send()` を呼べば1回だけ届く。連続送信は呼び出しを繰り返す。
- **書き込みは `'ab'`（追記）＋ `flush()`** が鉄則。
- フォーマットは **本体 → チェックサム → 0x00** の順。
