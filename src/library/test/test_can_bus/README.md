# CAN host regression test

This test runs the production CANBus fragmentation/reassembly and WCPP code.
Only the task, queue, and hardware CAN boundaries are replaced with small fakes.

From the repository root, with a native GCC or Clang C++ compiler installed:

```sh
g++ -std=c++14 -Wall -Wextra -Isrc/library/test/test_can_bus/stubs -Isrc src/library/test/test_can_bus/test_can_bus.cpp -o test_can_bus
./test_can_bus
```

Use `test_can_bus.exe` as the output name on Windows.

Coverage includes a multi-frame Tracker GPS packet with latitude, longitude,
negative altitude, and RSSI, a single-frame local telemetry packet, and a remote
command. It checks byte-for-byte round trips and command/telemetry classification.
The telemetry assertions fail if CANBus encodes `packet_id()` instead of
`type_and_id()`. Physical CAN wiring and timing require a hardware check.
