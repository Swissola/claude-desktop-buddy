---
created: 2026-06-30T08:27:51.829Z
title: Firm up StickS3 connected-idle power saving
area: firmware
files:
  - src/compat.h:142
  - src/main.cpp:1774
  - src/main.cpp:1806
---

## Problem

The battery-reboot fix (commit `9394013`, resolved debug session `sticks3-battery-reboot`)
made `compatSetCpuMhz()` a **no-op on BOARD_STICKS3** to stop the ~15s reboot loop (dropping
the CPU to 40MHz while BLE was connected starved the ESP32-S3 APB clock and reset the chip).
That fix is correct and hardware-verified (no reboot, runs cooler), but it's deliberately
conservative: while BLE-connected + screen-off on battery, the StickS3 now holds **240MHz**
instead of throttling, so it draws more than necessary. User flagged: "power saving will need
firming up in another session." This is an optimization, NOT a regression.

## Solution

On BOARD_STICKS3, throttle to an **APB-safe** clock instead of a flat no-op:
- Drop to **80MHz** (not 40MHz) in the screen-off/idle window — 80MHz keeps the APB clock high
  enough that the live BLE controller doesn't get starved (the reset cause), while cutting
  current vs 240MHz.
- And/or revisit the connected-idle light-sleep path so an idle-but-connected device on battery
  actually enters a low-power state (today `esp_light_sleep_start` / `bleIdleSleep` only triggers
  when BLE is *disconnected* idle — see src/main.cpp ~1425-1432, ~1815-1825).
- Verify on hardware: on battery + bridge-connected, confirm NO reboot loop returns AND the unit
  runs cooler than the 240MHz-hold baseline. (Serial backtrace is unavailable on this board.)
- Keep all changes behind `#if defined(BOARD_STICKS3)`; do not touch the StickC Plus AXP path.
