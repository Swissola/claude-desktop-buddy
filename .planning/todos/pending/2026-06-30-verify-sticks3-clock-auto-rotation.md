---
created: 2026-06-30T08:27:51.829Z
title: Eyeball StickS3 clock auto-rotation on USB
area: firmware
files:
  - src/main.cpp:766
---

## Problem

The StickS3 clock auto-rotation fix is **committed (`28644cf`) but never visually confirmed
on-device**. The bug: in auto mode the clock rendered landscape when the device was held
portrait and vice versa — the StickS3 IMU is mounted 90° rotated vs the StickC Plus the
orientation logic was tuned on. The fix swaps the X/Y accel axes in `clockUpdateOrient()`
under `#if defined(BOARD_STICKS3)`. It builds clean and the logic is symptom-backed, but we
got pulled into the battery-reboot bug before eyeballing it.

The clock view only renders when on USB power + bridge-connected + idle (RTC synced) — and the
device is stable on USB, so this is a safe ~30-second check.

## Solution

On USB, bridge-connected, leave idle until the clock face appears. Rotate the device:
- Upright (portrait) → should show the **portrait** clock layout.
- Sideways (landscape) → should show the **landscape** layout.
PASS = orientation now tracks correctly. If landscape ever comes up 180° wrong, that's the
separate 1↔3 facing (should self-correct from gravity) — note it if seen. No code change
expected; this is verification only.
