# LoRa (RP2040) 書き込み・ビルド失敗時の復旧手順（ZadigによるUSBドライバ再インストール）

作業者：narir

---

## 目標

`env:LoRa` 環境（RP2040ベース）にて、USB接続でボードへの書き込み（Upload）や実行を行う際に `no accessible RP2040 devices in bootsel mode` や `picotool was unable to connect` などのエラーが発生して失敗する問題の解決。

---

## 事象

`env:LoRa` の Upload または Build 時に、RP2040 デバイスとの通信ができず以下のエラーが出力されて失敗する。

* `no accessible RP2040 devices in bootsel mode`
* `appears to be a RP2040 device in BOOTSEL mode, but picotool was unable to connect`

主な要因:
* Windows側で RP2040 の BOOTSEL モードを認識するための適切なUSBデバイスドライバ（WinUSB）がインストールされていない、または他のドライバが当たっている。

---

## 解決法 (Zadigを使用したドライバインストール)

Windowsに汎用USBドライバをインストールできるソフト「Zadig」を使用して、RP2040 Boot用の適切なドライバ (`WinUSB`) を適用します。

1. **ZP2040をBOOTSELモードで接続**
   Raspberry Pi Pico (LoRaモジュール基板上のRP2040) の `BOOTSEL` ボタンを押したまま、PCのUSBポートに接続します。

2. **Zadigのダウンロードと起動**
   * [Zadig 公式サイト](https://zadig.akeo.ie/) にアクセスし、最新の Zadig をダウンロードして起動します。

3. **対象デバイスの選択**
   * Zadig の画面上部のプルダウンメニューから `RP2 Boot (Interface 1)` を選択します。
   * *(※もしプルダウンに表示されない場合は、メニューの `Options` -> `List All Devices` にチェックを入れて確認してください)*

4. **ドライバのインストール**
   * 「Driver」欄の矢印の右側にあたる適用先ドライバを `WinUSB` に合わせます。
   * 下部の `Install Driver` （すでに別のドライバが入っている場合は `Replace Driver`）をクリックします。

5. **Upload の再実行**
   ドライバのインストールが完了したら VS Code (PlatformIO) に戻り、再度 `Upload (LoRa)` を実行します。

書き込みが最後まで通り、`SUCCESS` と出力されれば復旧完了です。

---

## 参考

* [Raspberry Pi Pico WのLチカでつまづいた話 (Qiita)](https://qiita.com/suzuken6471/items/4dd52f510b64f96a7a5d)