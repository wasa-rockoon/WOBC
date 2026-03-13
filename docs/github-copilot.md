# github-copilotの学生無料で導入するためのマニュアル(適宜更新していきましょう！)

最終更新日：2026/03/14

githubのアカウントはすでにある前提で書いていこうと思います．[URL](https://qiita.com/melonsode/items/3602ea6441ca82e43c5a)この記事にまとまっていることを参考にしつつ，github-copilot君と書いています．時間が経つと色々変わることもあると思うので，適宜修正してください．

---

## 0. このマニュアルの対象

- 対象：**学生向けの無料特典（GitHub Education 経由）で GitHub Copilot を使いたい人**
- 前提：GitHubアカウントをすでに持っている
- 使用環境の例：VS Code（Windows / macOS / Linux）

> ※GitHubの画面UIや申請項目は時期によって変わります。文言が少し違っていても、
> 「GitHub Educationで学生認証 → Copilotを有効化」の流れは同じです。

## 1. 事前に準備するもの（前提の追記）

学生認証で詰まりやすいので、最初に揃えるとスムーズです。

- 学校メールアドレス（`fuji.waseda.jp` などgithubに登録済みのもの）。登録するにはgithubの自分のアカウントの`setting`から`email`を選択して`add email`から追加してください。

![setting-email](/docs/image/github-copilot/setting-email.png)

> `Verified`となっていれば大丈夫です。

- 在学を証明できるもの（例：学生証、在学証明書、履修画面のスクリーンショット）

![student-ID-card](/docs/image/github-copilot/IMG_5870.jpeg)

> 学生証の場合、画像のように学生証の表裏とそれぞれの英訳をつけた画像を用意してください。

- GitHubアカウント情報（2段階認証を有効化しておくと安心）

## 2. GitHub Education の学生認証を行う

#### 2-1 入力画面まで移動
- `GitHub` にログインする
- `GitHub Education`（Student Developer Pack）と検索して一番上の申請ページを開いて`Join GitHub Education`をクリック

![github-education](/docs/image/github-copilot/join-github-education.png)

- 次の画面で`Start an application`をクリック

![start-an-application](/docs/image/github-copilot/start-an-application.png)

#### 2-2 学校情報を入力し、在学証明資料をアップロード
- まず、生徒か教員かを聞かれるので`Student`を選択してください

- 次に自分の大学を検索して選択してください

![emailまで①](/docs/image/github-copilot/gmail.png)

> gmailだと登録できないのでご注意ください。

![emailまで②](/docs/image/github-copilot/waseda-jp.png)

- メールアドレスまで入力が済んだら`Share Location`をクリックして、`Continue`が緑になったらクリックして次に進んでください。

![学生証アップロード](/docs/image/github-copilot/student-id-card.png)

- ここで学生証のアップロードをしてください。

#### 2-3. 申請を送信

![Submitted](/docs/image/github-copilot/submitted.png)

- この画面まで来たらひとまず完了です。

#### 2-4. 審査結果メールを待つ（即時〜数日程度）
- 登録したメールアドレスに連絡メールが来ます。
- もしくは、うまくいっていると`GitHub` の `自分のアイコン -> settings -> Billing and licensing -> Education benefits` にお祝いメッセージ的なものが届きます。

![Congratulations](/docs/image/github-copilot/Congratulations.png)

- 上手くいかなかった場合も、修正点等のメッセージが届きます。

![Rejected](/docs/image/github-copilot/rejected.png)

### 補足

- メールアドレス入力時は手打ちではなくて、githubに登録されているメールアドレスから選択する形になります。なので事前にgithubのご自身のアカウントに学校のメールアドレスを登録してください。
- 申請却下時は、画像の見やすさや氏名・日付が確認できるかを見直して再申請

## 3. Copilot無料特典を有効化する

学生認証が通ったら、GitHub側のCopilot設定へ進みます。

1. GitHubの自分のアイコンをクリックして `Settings` を開く
2. `Copilot -> Features` から設定を確認
3. 学生向け無料特典が表示されていれば有効化する

![有効化後](/docs/image/github-copilot/active.png)

- このページで諸々の設定変更を行うことが出来ます。

> ここで料金表示が通常プランのままなら、Education認証反映待ちの可能性があります。
> 時間をおいて再確認してください。

## 4. VS CodeでCopilotを使えるようにする

### 1. VS Codeを開く
2. 拡張機能 `GitHub Copilot` と `GitHub Copilot Chat` をインストール
3. 右下やコマンドパレットからGitHubアカウントでサインイン
4. 対象リポジトリで補完が出るか確認

## 5. 動作確認（最小チェック）

- 任意のコードファイルを開く
- コメントで「何を作りたいか」を書く
- 補完候補（グレー文字）が出るか確認し、`Tab` で採用

## 6. よくあるトラブル

### Q1. 学生認証が通らない

- 提出画像が不鮮明 / 情報不足のことが多い
- 学校名・所属・有効期限など確認しやすい資料に差し替える

### Q2. Education承認済みなのにCopilotが無料にならない

- 反映に時間がかかる場合あり
- GitHubから一度サインアウト→再ログイン
- `Settings` のプラン表示を再確認

### Q3. VS Codeで補完が出ない

- 拡張機能が有効か確認
- サインイン状態を確認
- 会社ネットワークやプロキシ環境で通信制限がないか確認

## 7. 更新ルール（この文書の運用）

- UI変更があったら、該当セクションのスクショや文言を更新
- 最終更新日を更新
- 「詰まったポイント」が出たらQ&Aへ追記

---

必要なら次回、以下も追加できます。

- 申請画面の実スクリーンショット付き手順
- 学内向け（新入生向け）1ページ版の簡易手順
- VS Code初期設定（推奨拡張・キーバインド）
