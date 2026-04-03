# LoRa build失敗時の復旧手順（Windows長パス問題）

作業者：sands

---

## 目標

`env:LoRa` の build が通らないときに、
原因の切り分けと復旧を同じ手順で再実行できるようにする。

---

## 事象

`env:LoRa` の build 実行時に、`env:rp2040` 側の依存（`framework-arduinopico`）取得で停止することがある。

主な要因:

- Windows のパス長制限に引っかかる
- `.platformio` キャッシュ破損や中途半端な展開が残っている

`env:LoRa` は `env:rp2040` を継承しているため、LoRa単体の問題に見えても RP2040 依存取得で失敗する。

---

## 復旧コマンド（PowerShell）

作業ディレクトリへ移動:

```powershell
cd C:\Users\sands\WOBC
```

`sands` は自分のユーザー名に置き換える。

1. Windows 長パスを有効化

```powershell
git config --global core.longpaths true
```
```powershell
git config --global --get core.longpaths
```
> ここで`true`と返ってくれば大丈夫

2. 失敗しやすい依存キャッシュを削除

```powershell
Remove-Item -Recurse -Force "$env:USERPROFILE\.platformio\packages\framework-arduinopico" -ErrorAction SilentlyContinue
```
```powershell
Remove-Item -Recurse -Force "$env:USERPROFILE\.platformio\.cache" -ErrorAction SilentlyContinue
```
```powershell
Remove-Item -Recurse -Force ".\.pio" -ErrorAction SilentlyContinue
```

3. 削除確認

```powershell
Test-Path "$env:USERPROFILE\.platformio\packages\framework-arduinopico"
```
```powershell
Test-Path "$env:USERPROFILE\.platformio\.cache"
```
```powershell
Get-ChildItem "$env:USERPROFILE\.platformio\.cache" -Force
```

4. build 再実行

コマンドでやる必要は全くないので，VSCode上でまずbuildしてみてください．

```powershell
C:\Users\sands\.platformio\penv\Scripts\platformio.exe run --environment LoRa
```

必要なら詳細ログで再実行:

```powershell
C:\Users\sands\.platformio\penv\Scripts\platformio.exe run --environment LoRa -j 1 -v
```

---

## 判定の目安

- `git config --global --get core.longpaths` が `true` を返す
- `platformio run --environment LoRa` が `SUCCESS` で完了する

補足:

- `.platformio\.cache` は削除後に `downloads` や `tmp` が再作成される場合がある（正常）

---

## 再発時の確認ポイント

- 別PCで Git の長パス設定が未実施ではないか
- セキュリティソフトやプロキシで Git 取得が失敗していないか
- `platformio.ini` の `env:LoRa` が継承している `env:rp2040` の依存取得に失敗していないか

---

## 参考

- `docs/env_GOLIDEN/20260307_lora_build_troubleshoot.md`
