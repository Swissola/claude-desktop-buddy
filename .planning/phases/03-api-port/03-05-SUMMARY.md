---
phase: 03-api-port
plan: 05
type: execute
status: complete
requirements: [PORT-03]
completed: 2026-06-29
---

# Plan 03-05 Summary — Dual-env build verification + hardware smoke

## What was built

Verified the full Phase-3 API port compiles **zero-error on BOTH PlatformIO
environments from one identical `src/` tree** (PORT-03), then took the StickS3
onto real hardware. The StickS3 hardware run surfaced (and a follow-on debug
session fixed) three runtime defects that only a physical board could expose.

## Dual-env build evidence (PORT-03)

Task 1 reached zero-error on both envs (commit `d9bd961`). Four arduino-esp32
3.3.9 compile blockers were found and fixed during the first StickC-Plus
compile (which had never fully built on this toolchain): variant rename
`m5stick_c → m5stack_stickc_plus`, `esp_read_mac` moved to `<esp_mac.h>`,
GCC-14 `min()` type-deduction, and the Bluedroid(ESP32)/NimBLE(ESP32-S3) BLE
stack split in `ble_bridge.cpp`.

| Env | Compile | Link artifact | Notes |
|-----|---------|---------------|-------|
| `m5stickc-plus` | exit 0 | `.pio/build/m5stickc-plus/firmware.elf` + `.bin` | Re-confirmed after the post-build debug fixes touched shared `data.h` (see below). |
| `m5stack-sticks3` | exit 0 | `.pio/build/m5stack-sticks3/firmware.elf` + `.bin` | Also flashed + run on hardware. |

Both built from the identical `src/` tree — no per-env source forks. Regression
guard holds: zero `<M5StickCPlus.h>` includes and zero live
`M5.Axp`/`M5.Rtc`/`M5.Beep`/`ledcSetup`/`Wire1` references remain under `src/`.

## Hardware verification (D-08)

### StickS3 — VERIFIED on hardware ✅ (the board the user actually uses)
Flashed the `m5stack-sticks3` build to a physical M5StickS3. After fixing three
first-run defects (see "Follow-on debug" below), the device:
- boots cleanly (no bootloop),
- shows the "Hello! / a buddy appears" splash,
- renders the ASCII buddy,
- stays stable (no freeze, no blank-out, no reboot).

Display + buddy render path confirmed working on real S3 hardware.

### StickC Plus — D-08 smoke test DROPPED (won't-do)
| # | Item | Status |
|---|------|--------|
| 1 | Battery % (coulomb→getBatteryLevel, A3) | **Dropped — device repurposed** |
| 2 | Clock/RTC | **Dropped — device repurposed** |
| 3 | LED (GPIO10) | **Dropped — device repurposed** |
| 4 | Idle power draw | **Dropped — device repurposed** |
| 5 | Display restore after idle sleep (A1, 3000mV LDO) | **Dropped — device repurposed** |
| 6 | UI beep (compatBeep/M5.Speaker) | **Dropped — device repurposed** |

**Decision (user, 2026-06-29):** the user repurposed the physical M5StickC Plus,
so the on-device smoke test can no longer be run and is dropped rather than left
dangling as an open `human_needed` item. StickC-Plus correctness for this phase
is therefore established by **compile bar only** (env builds zero-error from the
shared tree), which is sound because every Phase-3 change is either StickS3-env
scoped or, for the one shared change in `data.h`, behavior-neutral on the
StickC Plus (see below). Runtime behavior on StickC Plus hardware is unverified
and would need re-checking if a device becomes available again.

## Follow-on debug (session `sticks3-bootloop`, resolved)

The StickS3 first-boot was a black-screen bootloop; debugging found three nested
StickS3-only defects, all fixed and committed:
- `b75ee13` — **PSRAM**: `CONFIG_SPIRAM=n` + `qio_qspi`, drop `-DBOARD_HAS_PSRAM`. The
  module's 8MB QUAD/3.3V PSRAM crashed arduino-esp32's pre-`setup()` `psramInit`
  hook when probed in octal mode → bootloop before app code.
- `48a0123` — **splash crash**: global `TFT_eSprite spr(&M5.Lcd)` captured a NULL
  parent at static-init (before M5Unified binds `Lcd`) → `pushSprite` LoadProhibited.
  Push to `&M5.Display` explicitly. Plus StickS3 keep-awake-on-USB.
- `8ac97c9` — **splash freeze (the real one)**: `_LineBuf::feed()` did
  `while (s.available())`, but HWCDC (USB-Serial/JTAG) returns signed `-1` when
  its rx queue is uninitialised (app never calls `Serial.begin()`; link is BLE)
  → infinite spin on `loop()`'s first iteration. Fixed to `> 0`. **This is the
  only Phase-3 change that touches the StickC Plus code path; it is behavior-neutral
  there** (UART0's `available()` never returns `-1`, so `> 0` is equivalent).

Full root-cause analysis: `.planning/debug/resolved/sticks3-bootloop.md`.

## Deviations from plan

- **D-08 StickC-Plus hardware smoke test dropped** (device repurposed) instead of
  recorded as outstanding `human_needed` — see decision above.
- **StickS3 hardware test, listed as optional in the plan, was actually run and
  passed** — and drove a debug session that added four StickS3-scoped runtime fixes
  beyond the pure compile port. None of these introduced Phase-4 scope (no event
  chimes, no motor/speaker collision work, no settings relabel).

## human_needed

- StickC-Plus on-device smoke test (battery %, RTC, LED, idle power, display
  restore, UI beep) is **dropped — physical device repurposed by the user**. Not
  outstanding; recorded here for traceability only. Re-run if a StickC Plus is
  available in future (esp. item 5 / A1 LDO-restore voltage, still `[ASSUMED 3000mV]`).
