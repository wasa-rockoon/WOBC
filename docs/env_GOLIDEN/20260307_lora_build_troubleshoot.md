# LoRa build失敗の調査と対応（別PC向け）

作業者：narrow

---

## 目標

別PCで `LoRa` 環境のbuildが通らない事象について、
「バージョンを変更せず」に原因候補を整理し、再現しやすい対処手順を作る。

---

## 発生していた事象

別PCで以下のようなエラーが発生して `LoRa` buildが失敗。

- `Failed to recurse into submodule path 'pico-sdk/lib/tinyusb'`
- `Could not process command ['git', 'submodule', 'update', '--init', '--recursive', '--force']`

`LoRa` はRP2040系（arduino-pico）を使うため、依存取得時にサブモジュール展開で失敗するとbuild不可になる。

---

## 原因候補（今回の整理）

バージョン差分以外で、主に次の4点を典型要因として整理。

1. ネットワーク / プロキシ制限
2. Git取得の途中失敗（再帰submodule展開失敗）
3. `.platformio` キャッシュ破損
4. Windows長パス設定不足

---

## 実施したこと

### 1) 設定確認

- `platformio.ini` の `env:LoRa` が `env:rp2040` を継承していることを確認
- `framework-arduinopico` をGitHubから取得する構成であることを確認

### 2) 依存URLの最小修正（バージョン固定のまま）

SSH形式の依存URLをHTTPS形式へ置換する方針を実施。

- 変更方針：`git@github.com:...` → `https://github.com/...`
- 目的：別PCでSSH鍵が未設定でも依存取得できるようにする

### 3) build確認

- `platformio run --environment LoRa` を実行して、依存解決とbuild進行を確認

---

## 現時点の設定メモ

現在の `platformio.ini` 上では、`can2040` はHTTPS指定になっていることを確認。

- `https://github.com/KevinOConnor/can2040.git`

※ `can_common` / `esp32_can` は現時点ではSSH形式が残っているため、
別PCでESP32系環境を使う場合は同様にHTTPS化すると詰まりを減らせる。

---

## 別PCでの推奨復旧手順（バージョン変更なし）

PowerShellで以下を順番に実施。

1. `git config --global core.longpaths true`
2. `Remove-Item -Recurse -Force "$env:USERPROFILE\.platformio\packages\framework-arduinopico" -ErrorAction SilentlyContinue`
3. `Remove-Item -Recurse -Force "$env:USERPROFILE\.platformio\.cache" -ErrorAction SilentlyContinue`
4. `C:/Users/<username>/.platformio/penv/Scripts/platformio.exe run --environment LoRa -j 1 -v`

プロキシ環境の場合は、`git config --global -l` で `http.proxy` / `https.proxy` の誤設定も確認する。

---

## まとめ

- 今回は「バージョン変更なし」で、環境要因の切り分けと対処に絞って対応した。
- 失敗ログ上の主因は、RP2040依存取得時のsubmodule再帰展開失敗の可能性が高い。
- HTTPS依存化 + 長パス有効化 + キャッシュ再取得の組み合わせで、別PCの再現失敗を減らせる見込み。
