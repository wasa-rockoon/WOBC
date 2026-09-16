# LoRa_ESP

ESP32-S3 LoRa基板の通常ファームウェア。`docs/library.md`に従い、`main.cpp`は基板設定とコンポーネント起動を担当する。LoRa1の双方向コマンド・ACK通信は`components/Uplink/uplink.h/.cpp`、LoRa2のテレメトリ専用通信は`components/LoRa/downlink.h/.cpp`に置く。

- Ground（unit ID `0x64`）：LoRa1 / UART2 / ch10でcommandを送信しACKを受信する。LoRa2 / UART1 / ch12でtelemetryを受信する。
- Flight（unit ID `0x41`）：LoRa1 / UART2 / ch10でcommandを受信しACKを送信する。LoRa2 / UART1 / ch12からtelemetryを送信する。
- UART0はUSBシリアルコンソール用に予約する。

受信パケットにはRSSIの`Ss`を追加する。E220設定時はUART受信バッファに残った応答バイトを取り除いてから読出し要求を送る。

```powershell
pio run -e LoRa_ESP_CAN_Ground
pio run -e LoRa_ESP_CAN_Flight
```
