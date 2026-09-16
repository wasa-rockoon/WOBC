# GS_AWS

`feature/GSsrc2`のGSへ非同期AWS WebSocket転送を追加したESP32-S3環境です。
CAN受信、SD Logger、SerialBusを維持し、Tracker、MB Launcher、MB Separationの
remote WCPP telemetryを加工せずAWSへ送ります。

`gs_aws_config.example.h`を`gs_aws_config.h`へコピーし、Wi-Fi、接続先、局固有の
`WASA_RECEIVER_ID`を設定します。`gs_aws_config.h`はGit対象外です。

```powershell
pio run -e GS_AWS
pio device list
pio run -e GS_AWS -t upload --upload-port COM11
```

Serialは従来どおり`[WCPP][CRC8][0x00]`、AWSはWCPP本体のみです。AWS切断中も
SerialBus、Logger、CANは継続し、再接続後は新しいパケットから送信します。
WebSocketの診断文字列をUSB Serialへ出すとutil.pyを破損するため禁止です。
ダウンリンクが無い場合も、10秒ごとのJSON heartbeatでAWSへGSの稼働状態を通知します。
