# GSD Debug Knowledge Base

Resolved debug sessions. Used by `gsd-debugger` to surface known-pattern hypotheses at the start of new investigations.

---

## sticks3-bootloop — M5StickS3 black-screen bootloop then splash-freeze on first hardware boot
- **Date:** 2026-06-29
- **Error patterns:** bootloop, black screen, ESP32-S3, ESP32-S3-PICO-1, PSRAM, qio_opi, qio_qspi, AP_3v3, octal vs quad, CONFIG_SPIRAM, LoadProhibited, Guru Meditation, EXCVADDR 0x00000074, psramInit, init_psram_new, HWCDC, USB-Serial/JTAG, available() returns -1, splash freeze, frozen on Hello!, static-init-order fiasco, TFT_eSprite null parent, M5.Lcd vs M5.Display, screen blank 15s, ARDUINO_USB_MODE=1
- **Root cause:** Three independent StickS3-only defects, each masking the next. (1) arduino-esp32's pre-setup() `ESP_SYSTEM_INIT_FN(init_psram_new) -> psramInit()` hook crashed probing the module's 8MB QUAD/3.3V (AP_3v3) PSRAM in octal mode (`qio_opi` + `CONFIG_SPIRAM=y`) -> memory corruption + LoadProhibited bootloop before app code. (2) Global `TFT_eSprite spr(&M5.Lcd)` captured a NULL `_parent` at static-init (before M5Unified binds its `Lcd` reference) -> `pushSprite`/`pushRotated` deref null+0x74 (LoadProhibited) at first render. (3) The real splash-freeze: `_LineBuf::feed()` used `while (s.available())`, but HWCDC (USB-Serial/JTAG) returns signed **-1** when its rx queue is uninitialised (app never calls `Serial.begin()`; the buddy link is BLE) -> -1 is truthy -> infinite spin on loop()'s first iteration inside `dataPoll()` -> device frozen on the "Hello!" splash. Plus a 15s screen-off blank when daemon-less on USB. Sessions 7-8 GDMA-first-push / waitDMA-settle theories were investigated and REFUTED — they were serial artifacts of HWCDC silently dropping breadcrumbs.
- **Fix:** (1) platformio.ini: `memory_type` qio_opi -> qio_qspi, `custom_sdkconfig=CONFIG_SPIRAM=n`, drop `-DBOARD_HAS_PSRAM`. (2) src/main.cpp `pushFrame()`: push to `&M5.Display` explicitly at both sites (bypass null static-init parent). (3) src/data.h: `while (s.available() > 0)` (signed-int guard; behavior-neutral on UART0, so StickC Plus unaffected). (4) src/main.cpp `loop()`: `keepAwakeOnUsb = _onUsb` under `#if defined(BOARD_STICKS3)`. No GDMA/waitDMA settle code remains. StickC Plus regression assessed by code-scope only (S3-scoped + board-agnostic data.h guard), NOT by build, because the device was repurposed.
- **Files changed:** platformio.ini, src/main.cpp, src/data.h, .gitignore
---
