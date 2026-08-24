# WOBC PC側コマンド送信ツール (send_command.py) の実装と使い方

作成日：2026-07-26  
作成者：Aoyama (Gemini)  
関連：[20260618_PC_to_MCU_packet_send.md]（.command ファイルと util.py 連携の解説）

---

## 目標

PCから Ground Station (GS) 経由でマイコンにコマンドパケットを送信するため、`util.py` と連携するコマンド送信用スクリプト `send_command.py` を実装し、CLIおよび対話型（REPL）両方で簡単にパケットを送出できるようにする。

---

## 全体像と仕組み

`send_command.py` は、`wcpp` ライブラリの `Packet` および `Entry` クラスを利用してコマンドパケットを生成し、`enqueue_packet` を用いて `.command` ファイルへ安全にバイナリ追記します。

```
[ ユーザー ] ──( CLI / メニュー選択 )──> send_command.py
                                           │
                                    ( enqueue_packet )
                                           ▼
                                    .command ファイル ───> util.py (シリアル転送) ───> MCU
```

---

## 実装内容とコードの解説

### 1. 新規追加ファイル

- [src/library/wcpp/python/send_command.py]

### 2. 主な実装箇所とそれぞれの意味

#### ① モジュールインポートの自動フォールバック処理
`send_command.py` は単体スクリプトとしても、パッケージの一部としても呼び出される可能性があります。また、`transport.py` 内部での相対インポート (`from .packet import Packet`) を壊さずにロードするため、`sys.path` に複数の探索パス（`src/library/wcpp`, リポジトリルート等）を自動追加する安全なインポート処理を組み込みました。

```python
try:
    from wcpp import Entry, Packet, enqueue_packet
except ImportError:
    # 探索パスを動的に追加してパッケージインポート
    ...
```

#### ② 柔軟な ID パース関数 (`parse_id`)
パケットIDやコンポーネントIDとして、10進数数値 (`16`)、16進数文字列 (`0x10`)、およびASCII文字 (`'c'`, `'p'`) のいずれを入力しても自動的に数値 (int) に変換します。

```python
def parse_id(val: str) -> int:
    val = val.strip()
    if val.startswith("0x") or val.startswith("0X"):
        return int(val, 16)
    if len(val) == 1 and not val.isdigit():
        return ord(val)
    return int(val)
```

#### ③ Entry生成とバリデーション (`create_entry`)
WCPP の仕様上、エントリ名 (`Entry.name`) は必ず **2文字** である必要があります。2文字でない場合は明示的に例外を発生させ、不正なパケット送出を防ぎます。また、入力値の型（`int`, `float`, `bool`, `str`）の自動判定および明示指定に対応しています。

```python
def create_entry(name: str, value_str: str, data_type: str = "auto") -> Entry:
    if len(name) != 2:
        raise ValueError(f"Entry名は必ず2文字である必要があります: '{name}'")
    ...
```

#### ④ 対話型（REPL/メニュー）モード (`interactive_mode`)
引数なしで実行した場合、コンソールにメニューを表示します。
1. **Ping送信**: PacketID `'p'` (0x70), ComponentID `0x00`
2. **テストコマンド送信**: PacketID `'t'` (0x74), ComponentID `0x10`
3. **任意コマンド作成**: ユーザー入力で PacketID, ComponentID, 任意の複数 Entry を組み立てて送信
4. **終了**

#### ⑤ CLI引数モード (`main`)
`argparse` を用いて、コマンドライン引数経由で1発送信できます。

---

## 使い方

### 前提条件

パケット送信モジュールは `crc` ライブラリ（Pythonパッケージ）に依存しているため、リポジトリの仮想環境 (`.venv`) を使用して実行します。

### 1. CLI引数モード（1発送信）

#### 基本的な使用例
```bash
# Windows
.venv\Scripts\python.exe src/library/wcpp/python/send_command.py --packet-id c --component-id 0x10 --data "hello"

# Linux / Mac
.venv/bin/python src/library/wcpp/python/send_command.py --packet-id c --component-id 0x10 --data "hello"
```

#### コマンドライン引数オプション一覧

| オプション | 短縮形 | 説明 | 例 |
|---|---|---|---|
| `--packet-id` | `-p` | パケットID (文字, 10進数, 16進数) | `'c'`, `0x10`, `99` |
| `--component-id` | `-c` | 送信先コンポーネントID (既定: `0x01`) | `0x10`, `16` |
| `--data` | `-d` | 送信データ | `"hello"`, `100`, `25.5` |
| `--entry-name` | `-e` | データEntry名 (2文字, 既定: `"Dt"`) | `"Dt"`, `"Vx"` |
| `--data-type` | `-t` | データ型 (`auto`, `int`, `float`, `bool`, `str`) | `int` |
| `--command-path` | | 出力先 `.command` ファイルのパス (既定: `.command`) | `.command` |
| `--interactive` | `-i` | 対話型メニューモードを起動 | |

### 2. 対話型（REPL / メニュー）モード

引数なしで起動すると、インタラクティブメニューが起動します。

```bash
.venv\Scripts\python.exe src/library/wcpp/python/send_command.py
```

実行例：
```text
=============================================
    WOBC Command Sender (Ground Station)
=============================================
 1. Ping送信 (PacketID: 'p', CompID: 0x00)
 2. テストコマンド送信 (PacketID: 't', CompID: 0x10)
 3. 任意コマンド作成
 4. 終了
=============================================
メニュー番号を選択してください (1-4): 3

--- 任意コマンド作成 ---
Packet ID を入力してください (例: c, p, 10, 0x10) [既定: c]: c
Component ID を入力してください (例: 0x10, 16) [既定: 0x01]: 0x10

Entry (パラメータ) の追加:
  Entry名 (2文字, 空白で終了): Vx
  Entryの値: 100
  型 (auto/int/float/bool/str) [既定: auto]: int
  [OK] Entry 'Vx' を追加しました。
  さらにEntryを追加しますか？ (y/N): n

==================================================
 [SUCCESS] パケットをキューに追加しました (.command)
==================================================
```

---

## ノウハウ・注意点 (Troubleshooting)

| 事象 / 注意点 | 原因と対策 |
|---|---|
| `ModuleNotFoundError: No module named 'crc'` | グローバル環境の Python で実行すると発生します。必ず `.venv\Scripts\python.exe`（仮想環境）を使用して実行してください。 |
| Entry作成時にエラーになる | WCPP仕様により **Entry名は厳密に2文字** である必要があります（1文字や3文字以上はエラーになります）。 |
| シリアル送信されない | `send_command.py` は `.command` ファイルに書き込むだけです。マイコンに実際に送信するには、別ターミナル等で `util.py -p COMx` を起動しておく必要があります。 |
