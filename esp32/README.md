# ESP32 BMS Gateway — local dashboard

This firmware keeps persistent BLE links to the two configured JBD BMS units, polls Basic Info
(`0x03`) and Cell Info (`0x04`), calculates the combined parallel-battery state, and renders a
240x320 portrait dashboard plus two detail pages. It also maintains Wi-Fi, Firebase Authentication,
an authenticated RTDB status heartbeat, and viewer-gated live telemetry without blocking local
monitoring.

The E32R28T-1 display configuration uses the documented ST7789P3 wiring. Its XPT2046 controller
is initialized, but button hit-testing remains disabled until the installed panel is physically
calibrated. Pages auto-rotate every 10 seconds; Serial keys `0`, `1`, and `2` select dashboard,
BMS 1 detail, and BMS 2 detail.

The dashboard indicators are `W` (Wi-Fi), `C` (cloud/RTDB), and `R` (remote
viewer). `W` and `C` are green when ready. `R` is green only while a fresh
viewer heartbeat exists.

Copy `include/config/Secrets.example.h` to `include/config/Secrets.h` and fill
in the local credentials. The real secrets file is ignored by Git. Firebase
Console setup is documented in `docs/FIREBASE_SETUP.md`.

## Build / upload / monitor

The selected generic board is `esp32dev`. Serial speed is 115200 baud.

```powershell
platformio run
platformio run --target upload --upload-port COM9
platformio device monitor --port COM9 --baud 115200
```

Close the phone's **JBM BMS** app before testing. Some Bluetooth BMS modules accept only one BLE
client, so a phone connection can prevent the ESP32 from connecting.
