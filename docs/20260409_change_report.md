# WOBC 変更レポート（2026-04-09）

## 1. 目的
本ドキュメントは、今回の修正について、
- 何を
- どのような背景で
- どのように変更し
- 変更後に何が期待されるか
を一つにまとめたものです。

あわせて、実装過程で実施したこと、失敗した点、うまくいった点、利用方法、FAQ を統合しています。

## 2. 背景
今回の作業開始時点で、Tracker 環境はビルド失敗状態でした。主な背景は次の通りです。

1. 依存ライブラリ取得時の認証失敗
- SSH URL 形式の依存先が含まれており、環境側の鍵設定により取得失敗。

2. GPS 実装にマージコンフリクト痕跡
- [src/components/GPS/gps.cpp](src/components/GPS/gps.cpp) にコンフリクトマーカーが残存。

3. 将来的な運用リスク（ESP32 タスク配置）
- タスク/コンポーネントが core affinity を明示できず、実機挙動の再現性に課題。

4. 警告が多く、障害調査時のノイズが大きい
- switch 未処理列挙値、型変換警告などが混在。

## 3. 変更内容（何を、どう変えたか）

### 3.1 依存取得方式の変更（SSH -> HTTPS）
対象:
- [platformio.ini](platformio.ini)

変更:
- git@github.com:... 形式を https://github.com/... 形式に変更。

理由:
- ビルド環境に SSH 鍵がない場合でも依存取得できるようにするため。

期待効果:
- 初回セットアップや別PCでのビルド失敗率を低減。

### 3.2 GPS のマージコンフリクト除去
対象:
- [src/components/GPS/gps.cpp](src/components/GPS/gps.cpp)

変更:
- コンフリクトマーカーを削除し、UT（UTC時刻文字列）を送信する実装へ統一。
- 文字列生成を sprintf から snprintf へ変更。
- バッファを 27 から 32 へ拡張。
- SampleTimer の初期化順序を宣言順に合わせて調整。

理由:
- コンパイルエラーの直接原因を除去。
- 文字列フォーマット時のオーバーフロー系警告を低減。
- 初期化順警告の抑制。

期待効果:
- GPS コンポーネントが安定してビルド可能。
- UT フィールドの取り扱いが明確化。

### 3.3 WebSocket イベントの列挙値対応追加
対象:
- [src/components/Telemeter/telemeter.cpp](src/components/Telemeter/telemeter.cpp)

変更:
- WStype_PING と WStype_PONG を switch に追加。

理由:
- 未処理列挙値警告を低減し、将来的な保守性を向上。

期待効果:
- テレメータのイベント処理が網羅的になり、警告ノイズを削減。

### 3.4 ESP32 の core affinity 指定機能を追加
対象:
- [src/library/process/task.h](src/library/process/task.h)
- [src/library/process/task.cpp](src/library/process/task.cpp)
- [src/library/process/component.h](src/library/process/component.h)
- [src/library/process/component.cpp](src/library/process/component.cpp)

変更:
- Task / Component のコンストラクタに core_id を追加。
- 型は BaseType_t を採用（tskNO_AFFINITY と整合）。
- ESP32 では xTaskCreatePinnedToCore を使用。
- 非ESP32 は従来通り xTaskCreate を使用。
- Component コンストラクタで priority_ や task_handle_ を明示初期化。

理由:
- 実機でのタスク配置の明示性を高め、動作の再現性を向上。
- 既存APIを大きく崩さず、デフォルトは従来互換（tskNO_AFFINITY）を維持。

期待効果:
- 必要箇所でコア固定運用が可能。
- 既存コードは原則そのまま動作。

### 3.5 wcpp サブモジュール内の警告・互換修正
対象:
- [src/library/wcpp/cpp/packet.h](src/library/wcpp/cpp/packet.h)
- [src/library/wcpp/python/util.py](src/library/wcpp/python/util.py)

変更:
- setBool 実装を明示的な1バイト格納へ変更し、警告を低減。
- Python import を相対importに変更。

理由:
- bool 取り扱いの曖昧さを減らすため。
- パッケージ実行時の import 安定化のため。

期待効果:
- ビルド時警告の一部削減。
- Python 側の実行互換性向上。

## 4. 実装過程で行ったこと（時系列）

1. 失敗した Tracker ビルドログを取得し、原因を分類。
2. 認証エラーを解消するため依存URLを HTTPS 化。
3. 再ビルドで GPS のコンフリクトマーカー残存を検出。
4. GPS ファイルを修正し、コンフリクト記号を除去。
5. warning を段階的に削減（Telemeter、Task/Component、wcpp）。
6. Tracker を再ビルドして SUCCESS を確認。
7. GS でも SUCCESS を確認し、変更の副作用を確認。

## 5. 失敗した点と対処

### 5.1 端末コマンド差異によるログ取得失敗
事象:
- head / tail を想定した手順が PowerShell でそのまま使えず失敗。

対処:
- Select-Object -First / -Last に切り替え。

学び:
- Windows 前提では PowerShell 標準コマンドで統一する方が安定。

### 5.2 GPS の置換編集が一度失敗
事象:
- 文字コードや差分状態の影響で、想定文字列が一致せず置換失敗。

対処:
- ファイル内容を再確認し、確定内容で再保存して修正。

学び:
- コンフリクト解消直後のファイルは、厳密一致置換より再生成の方が安全な場合がある。

### 5.3 診断情報の一時的な不整合
事象:
- 実ファイル修正後も、エディタ側の診断に古いエラー表示が残る場面があった。

対処:
- 再ビルド結果を最優先し、実ログで最終確認。

学び:
- 最終判断はコンパイラ実行結果で行う。

## 6. うまくいった点

1. 根本エラーの早期特定
- 依存取得失敗とコンフリクト残存を最初に解消できた。

2. 互換性を保った設計変更
- core_id はデフォルト値を持たせ、既存呼び出しをほぼ無変更で維持。

3. 段階的検証
- 変更後に都度ビルド確認を行い、影響範囲を局所化できた。

## 7. 検証結果

### 7.1 Tracker
結果:
- SUCCESS
- RAM 12.5%（40908 / 327680）
- Flash 13.1%（438965 / 3342336）

### 7.2 GS
結果:
- SUCCESS

## 8. 使用方法（運用ガイド）

### 8.1 通常ビルド
実行:
- PlatformIO タスクの Build（Tracker など）を実行
または
- platformio run --environment Tracker

期待される状態:
- 依存取得で SSH 鍵エラーが出ない
- コンフリクトマーカー由来エラーが発生しない

### 8.2 コア固定を使う場合
考え方:
- Task / Component 生成時に core_id を指定すると、ESP32 ではそのコアに固定可能。
- 指定しない場合は tskNO_AFFINITY（従来互換）。

運用の目安:
- センサ読み取りや通信など、競合しやすい処理を固定してジッタを抑える。
- まずはデフォルト運用で安定性を確認し、必要箇所のみ固定する。

### 8.3 GPS UT の扱い
内容:
- UT は YYYY-MM-DD HH:MM:SS.cc 形式の文字列。

注意:
- GPS の date/time 有効性判定は用途に応じて追加検討する。

## 9. FAQ

### Q1. なぜ SSH URL をやめたのか
A. CIや新規PCで秘密鍵未設定のままでも依存取得できるためです。

### Q2. core affinity 追加で既存コードは壊れないか
A. デフォルト引数が tskNO_AFFINITY のため、既存呼び出しは基本的に互換です。

### Q3. まだ warning は残っているか
A. 残っています。今回の主眼はビルド不能要因の除去と主要警告の低減です。未使用変数や一部初期化順など、残警告は段階的に解消できます。

### Q4. Tracker は本当に直ったか
A. 最新ビルドで SUCCESS を確認済みです。

### Q5. Upload が失敗する場合はどうすればよいか
A. ビルド成功とUpload失敗は別問題です。まずポート選択、ボード接続、リセットタイミング、権限（ドライバ）を確認してください。

### Q6. GPS の UT 送信は常に有効か
A. 現実装では有効です。無効化したい場合は UT append を条件分岐または設定化してください。

## 10. 今後の推奨アクション

1. 残warningの体系的解消（IMU、LiPo、LoRa 領域）
2. 全環境ビルドの定期実行（Tracker、GS、MissionBus、GOLIDEN、RP2040CoreTest、LoRa）
3. core affinity の運用ポリシー決定（どの処理をどのコアへ固定するか）

---
本レポートは 2026-04-09 時点の作業内容に基づいています。