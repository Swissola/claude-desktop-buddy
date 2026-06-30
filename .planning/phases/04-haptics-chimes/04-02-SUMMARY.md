---
phase: "04-haptics-chimes"
plan: "02"
subsystem: "haptics/chimes"
tags: [hardware-verify, flash, listening-test, volume, defaults]
dependency_graph:
  requires: ["04-01"]
  provides: ["HAPT-01-verified", "HAPT-03-verified", "D-03-verified", "D-08-verified"]
  affects: ["src/main.cpp", "src/stats.h", "src/buddy.cpp"]
tech_stack:
  added: []
  patterns: ["esptool native-USB flash (--before usb-reset)", "NVS-default-when-unset", "board-conditional volume table"]
key_files:
  created: []
  modified:
    - path: "src/main.cpp"
      role: "Volume cycler widened to 6 steps (VOL_LEVELS/VOL_LABELS/VOL_LEVELS_N); brightness global default 2"
    - path: "src/stats.h"
      role: "volIdx default 3 (60%) + clamp ceiling 5; s_bright default 2; ampm default true (12hr)"
    - path: "src/buddy.cpp"
      role: "currentSpeciesIdx default 6 (octopus) when species NVS unset"
decisions:
  - "All five chimes verified audibly distinct on hardware at 60% volume — approve (E5) vs deny (A3) unmistakable; deny clearly audible at 220Hz so the A4/440 fallback was NOT needed (RESEARCH Open Question 3 resolved: 220Hz stands)"
  - "Volume cycler widened from 5 to 6 entries per user request: mute + five even steps (20/40/60/80/100%), default 60% — louder than the original 50% which was barely audible"
  - "Opportunistic default tweaks requested during the live session: octopus pet, brightness 2/4, 12hr clock (all apply only when their NVS key is unset; reflash does not wipe NVS)"
  - "Temporary demo-mode chime-test harness (BtnA-steps-through-5-chimes) was added to drive the listening test without the live bridge, then stripped after sign-off"
metrics:
  duration: "~hardware session (interactive)"
  completed: "2026-06-30"
  tasks_completed: 2
  files_modified: 3
---

# Phase 04 Plan 02: Flash + On-Device Listening Test Summary

**One-liner:** Flashed the chime build to the StickS3 and ran the human listening test (the Phase 4 acceptance gate) — all five event chimes confirmed audibly distinct at 60% volume, approve/deny unmistakable, deny audible at 220Hz (no fallback needed); volume cycler widened to 6 steps and several boot defaults tuned per user request.

## Tasks Completed

| Task | Name | Commit | Notes |
|------|------|--------|-------|
| 1 | Build and flash the StickS3 firmware | (04-01 source) | esptool write_flash @0x0, hash verified, COM8, no bootloop |
| 2 | On-device chime + volume listening test (human-verify gate) | c245d38 | PASSED — all 5 chimes distinct at 60%; tuning applied + re-flashed |

## What Happened

### Task 1 — Flash

Built the StickS3 env incrementally (green) and flashed `firmware.factory.bin` to COM8 via the native-USB esptool method from `sticks3-bootloop.md` (`--before usb-reset --after hard-reset`, `cc-buddy-bridge.exe` killed first). `Hash of data verified`; device booted cleanly to the buddy with no bootloop.

### Task 2 — Listening test (acceptance gate)

The five chimes can't be triggered by plain button presses — they're driven by live Claude-session events (prompt approve/deny, session waiting, level-up, link connect) that can't be staged on a bare device. A **temporary demo-mode chime-test harness** was added (while demo mode is on, BtnA steps through approve → deny → attention → celebrate → connect) to drive the test without the bridge, then **stripped after sign-off** (net-zero diff).

**Verdict: PASS.** User confirmed all five chimes sound fine and audibly distinct at 60% volume. Approve (E5 659Hz) vs deny (A3 220Hz) unmistakable. Deny clearly audible at 220Hz — the documented A4/440 fallback (RESEARCH Open Question 3) was **not needed**; 220Hz stands.

### Adjustments made during the session (commit c245d38)

- **Volume cycler widened 5→6 steps** per user request: `VOL_LEVELS = {0,51,102,153,204,255}`, labels `{mute,20%,40%,60%,80%,100%}`, default index 3 = **60%** (the original 50% mid was barely audible). Mute confirmed working. Persists via the same `settingsSave()` NVS path as brightness — survives battery-dead.
- **Default pet → octopus** (`currentSpeciesIdx = 6` when `species` NVS unset).
- **Default brightness → 2/4** (~60%) — `brightLevel` global init + `s_bright` load default.
- **Default clock → 12-hour** (`ampm` default `true`).

All NVS-default-when-unset, so they apply on a fresh device without forcing a value over a user's saved choice.

## Build Results

| Environment | Status |
|-------------|--------|
| m5stack-sticks3 (PRIMARY) | SUCCESS |
| m5stickc-plus (non-blocking sanity) | SUCCESS |

Both envs green after the shared-code default changes; StickC Plus motor `#else` path untouched (HAPT-02 preserved).

## Deviations from Plan

- The plan anticipated triggering chimes via real events; in practice a temporary demo-mode test harness was needed (and then removed) because the events require the live bridge. Outcome unchanged: human verdict obtained.
- Scope grew slightly beyond chimes: user requested a 6-step volume cycler and three boot-default tweaks (octopus / brightness / 12hr) during the session. All committed under 04-02.

## Known Issues / Follow-ups

- **Auto clock rotation appears broken** (user-reported during the session). Deferred to end-of-GSD-session per user; NOT addressed in Phase 4. Should be captured as a follow-up bug.

## Threat Flags

No new threat surface. Flash is local physical USB (T-04-03: accepted). No package installs.

## Self-Check

### Created files exist
- `.planning/phases/04-haptics-chimes/04-02-SUMMARY.md` — this file

### Commits exist
- `c245d38` — hardware-verified chime/volume tuning + default tweaks

### Acceptance gate
- Human verdict: PASS (all five chimes distinct at 60%, approve/deny unmistakable, deny audible at 220Hz, volume cycler works incl. mute, beep/chime don't clash)

## Self-Check: PASSED
