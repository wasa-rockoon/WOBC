# GS_AWS (feature/packetsend)

This environment preserves the packetsend GS CAN bus, PC SerialBus, SD logger,
pins, module ID and startup order. Only the legacy Telemeter is replaced with
AwsForwarder, avoiding two simultaneous Wi-Fi/WebSocket clients.

It forwards unmodified remote WCPP telemetry from Tracker (`0x61`), MB Launcher
(`0x62`) and MB Separation (`0x41`) and sends a receiver heartbeat every ten
seconds. An AWS outage does not block CAN, SerialBus or logging.

Copy `gs_aws_config.example.h` to `gs_aws_config.h` and enter Wi-Fi and a unique
`WASA_RECEIVER_ID`.

```powershell
pio run -e GS_AWS
pio device list
pio run -e GS_AWS -t upload --upload-port COM11
```

Do not print diagnostics to `Serial`; it carries binary WCPP frames for util.py.
