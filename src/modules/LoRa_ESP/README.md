# LoRa_ESP

ESP32-S3 LoRa基板の通常ファームウェア。`docs/library.md`に従い、`main.cpp`は基板設定とコンポーネント起動を担当し、E220の初期化・送受信・検証は`components/LoRa/dual_lora.h/.cpp`に置く。

- Ground（unit ID `0x63`）：LoRa1 / UART2 / ch10でcommandを送信し、LoRa2 / UART1 / ch3で受信する。
- Flight（unit ID `0x61`）：LoRa1 / UART2 / ch10で受信し、packet ID `'M'` のtelemetryをLoRa2 / UART1 / ch3から送信する。
- UART0はUSBシリアルコンソール用に予約する。

受信パケットにはRSSIの`Ss`を追加する。E220設定時はUART受信バッファに残った応答バイトを取り除いてから読出し要求を送る。

```powershell
pio run -e LoRa_ESP_Ground
pio run -e LoRa_ESP_Flight
pio run -e LoRa_ESP_CAN_Ground
pio run -e LoRa_ESP_CAN_Flight
```
