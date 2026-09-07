# E220 host regression tests

The fake UART supplies E220 register acknowledgements and receive frames. These
tests exercise the real `src/components/LoRa/e220.cpp`, without ESP32 hardware.
The test includes that implementation directly, so no test build configuration
change is needed.

From the repository root, with a native GCC or Clang C++ compiler installed:

```sh
g++ -std=c++14 -Wall -Wextra -Werror src/library/test/test_e220/test_e220.cpp -o test_e220
./test_e220
```

Use `test_e220.exe` as the output name on Windows. Where supported, add
`-fsanitize=address,undefined -fno-omit-frame-pointer` to detect the previous
eight-register default-command stack overflow as well as bounds regressions.
It can also run with `pio test -e native -f test_e220` when the native toolchain
and PlatformIO test dependencies are installed.

Coverage includes eight-byte default configuration and failed acknowledgements,
register-size guards, 255/256-byte transmit limits, a 257-byte UART receive frame
with RSSI, destination-buffer bounds, same-length frames after a long idle period,
fragmented-frame recovery, and yielding during an AUX timeout. This does not
verify physical wiring, UART timing, or radio interoperability.
