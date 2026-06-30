# Claude Desktop Buddy — StickS3 + Chimes Migration

## What This Is

A migration of this fork's Claude Desktop Buddy firmware (currently M5StickC Plus only)
so it also builds and runs on the **M5StickS3 (ESP32-S3)**, from a single shared source
tree. Because the StickS3 cannot drive the existing physical Vibration HAT, haptic feedback
is replaced by **audio chimes** through the StickS3's built-in ES8311 speaker. The original
M5StickC Plus build must keep working unchanged.

## Core Value

The buddy runs on the M5StickS3 with meaningful event feedback (chimes), **without breaking
the M5StickC Plus build** — one codebase, two boards.

## Requirements

### Validated

<!-- Inferred from existing fork code (brownfield). -->

- ✓ BLE bonding to Claude Desktop/Code + live session-state display — existing
- ✓ Procedural / GIF pet rendering on the 135×240 LCD (incl. doge + llama buddies) — existing
- ✓ Approve/deny approval prompts with countdown — existing
- ✓ Pattern-based haptics via Vibration HAT on GPIO26/LEDC ch2 (StickC Plus) — existing
- ✓ Settings menu (brightness, sound, vibrate, bt, wifi, led, transcript, clock, …) — existing
- ✓ Multi-host BLE bonding, up to 7 hosts — existing (`multi-host-bonding` branch)
- ✓ Firmware builds for M5StickS3 (ESP32-S3) from the same `src/` tree — v1.0
- ✓ M5StickCPlus library replaced by M5Unified + M5GFX behind a `compat.h` shim — v1.0
- ✓ M5StickC Plus build remains green from the identical source — v1.0
- ✓ Haptic events replaced by audio chimes via `M5.Speaker` (ES8311) on StickS3 — v1.0 (hardware-verified)
- ✓ StickC Plus retains its LEDC vibration-motor path (board-conditional) — v1.0
- ✓ "vibrate" setting relabelled to "chime" + a persisted speaker-volume cycler added — v1.0

### Active

_v1.0 shipped — all migration requirements validated. Next-milestone candidates (carried-forward
follow-ups, captured in `.planning/todos/pending/`):_

- [ ] Firm up StickS3 connected-idle power saving (APB-safe ~80MHz throttle / light-sleep)
- [ ] Restore encrypted BLE bond (came back unencrypted after a re-pair)
- [ ] Format StickS3 LittleFS (`fsTotal=0`; enables GIF character packs)
- [ ] Visually confirm the StickS3 clock auto-rotation fix on USB

### Out of Scope

- **Emotion system** (`feat/emotion-system` branch) — user dislikes the visuals; stays unmerged.
  Connection-badge / countdown / velocity enhancements are cherry-picked into that branch;
  lift them individually later only if asked.
- **Rewiring / adapting the physical Vibration HAT to the StickS3** — user has ruled out any
  hardware modification. (The 8-pin HAT fits neither the StickS3 Hat2-Bus 16P nor its Grove port.)
- **Cherry-picking PR #48's commits** — this fork's `main` has diverged; we replicate the
  *technique*, not the commits (they would conflict in `main.cpp`/`platformio.ini`).
- **Grove Vibrator Unit purchase** — a possible future way to restore physical buzz with no
  soldering (plug into Port.A / G9, set `VIBRATE_PIN=9`); not part of this migration.

## Context

- **Reference:** upstream PR #48 (`anthropics/claude-desktop-buddy`, branch
  `feat/m5sticks3-port` by yiduo) — and ~5 other independent StickS3 ports — all converge on
  the same recipe: drop the board-specific `M5StickCPlus` library, build on `M5Unified` +
  `M5GFX` (auto-detects the board at runtime), two PlatformIO envs sharing a `[common]`
  section, and a thin `src/compat.h` shim re-creating legacy names
  (`TFT_eSprite`→`M5Canvas`, `TFT_eSPI`→`lgfx::LGFXBase`, software RTC,
  `compatOnUsb`/`compatLed`/`compatChipTempC`) so ~3k lines of UI code stay untouched.
  Upstream refuses feature PRs, so #48 etc. are reference-only.
- **Fork divergence:** this fork's `main` has inline vibration code in `src/main.cpp`
  (GPIO26 / LEDC ch2), doge + llama buddies, `buddy_common.h`, `stats.h`, plus `fix/*`
  backports. `compat.h` and `partitions_8mb.csv` from #48 are board-agnostic and reusable
  verbatim; everything else is re-applied as edits to the fork's (larger) files.
- **Haptics → chimes mapping:** the existing pattern engine
  (`PAT_APPROVE`/`DENY`/`ATTENTION`/`CELEBRATE`/`CONNECT`, on/off duration arrays +
  amplitude arrays) maps 1:1 onto tone sequences (durations → note/gap lengths, amplitudes →
  pitch/volume). Distinct pitches let approve ≠ deny, which the motor couldn't express.
- **Bonus bug fix:** the firmware comment at `main.cpp:31-35` documents months of "one buzz
  then nothing" caused by the motor's LEDC channel colliding with `M5.Beep`'s channel 0. On
  the StickS3, `M5.Speaker` uses the I2S/ES8311 path, not LEDC PWM — that collision class
  disappears.
- **StickS3 I/O:** ESP32-S3-PICO-1-N8R8 (8MB flash + 8MB PSRAM); ES8311 mono codec + AW8737
  amp (8Ω/1W speaker) + mic; **no RTC chip** (software RTC in `compat.h`); top = Hat2-Bus 16P,
  bottom = Grove Port.A (G9 signal / G10 / 5V / GND); native USB-CDC.

## Constraints

- **Tech stack**: PlatformIO + Arduino framework; libs `m5stack/M5Unified @ ^0.2.x`,
  `bitbank2/AnimatedGIF`, `bblanchon/ArduinoJson`. — vendor SDK, actively maintained.
- **Compatibility**: M5StickC Plus build (`pio run -e m5stickc-plus`) must stay green from the
  same source. — don't regress the working device.
- **Hardware**: no rewiring/soldering permitted. — user constraint; drives the chimes decision.
- **Process**: work on branch `feat/sticks3-chimes`; do not cherry-pick #48 commits. — avoid
  conflicts with diverged fork.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Replicate PR #48's M5Unified approach, don't merge its commits | Fork `main` diverged; commits would conflict in `main.cpp`/`platformio.ini` | ✓ Good — both envs build clean from one tree |
| Haptics → audio chimes via ES8311, not physical buzz | HAT can't connect to StickS3 without rewiring, which is ruled out; StickS3 has a real speaker | ✓ Good — five distinct chimes, hardware-verified |
| Keep StickC Plus LEDC motor path (board-conditional `#if BOARD_STICKS3`) | Preserve the working device; one tree, two boards | ✓ Good — motor `#else` branch byte-for-byte unchanged |
| Drop the emotion system | User dislikes the visuals | ✓ Good |
| Coarse granularity, 4 phases, research skipped | Technical groundwork already done in scoping conversation | ✓ Good — though Phase 3 needed a 3-bug bootloop debug + Phase 4 a battery-reboot debug |
| StickS3 CPU-throttle is a no-op (battery-reboot fix) | Dropping to 40MHz with BLE live starves the APB clock and resets the chip | ⚠️ Revisit — safe but holds 240MHz; firm up power saving next milestone |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

## Current State (v1.0 shipped 2026-06-30)

The migration is **shipped and merged** (PR #3 → `main`). Both `m5stack-sticks3` and
`m5stickc-plus` envs build clean from one tree. The StickS3 boots an octopus buddy, plays five
distinct chiptune chimes via the ES8311 speaker (hardware-verified at 40% volume default), has a
persisted 6-step volume cycler, and 12-hour-clock / 60%-brightness defaults. Two bugs found and
fixed during bring-up: a 3-bug StickS3 bootloop (Phase 3) and a battery-only ~15s reboot loop
caused by CPU-throttling while BLE was live (post-Phase-4 debug, hardware-verified fixed).

Known follow-ups for next milestone: firm up connected-idle power saving, restore the encrypted
BLE bond, format LittleFS, and eyeball the clock auto-rotation fix (see `.planning/todos/pending/`).

---
*Last updated: 2026-06-30 after v1.0 (StickS3 + Chimes) milestone*
