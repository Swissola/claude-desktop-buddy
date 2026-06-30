---
phase: "04-haptics-chimes"
plan: "01"
subsystem: "haptics/chimes"
tags: [chime-engine, m5speaker, board-conditional, settings, volume]
dependency_graph:
  requires: ["03-05"]
  provides: ["HAPT-01-impl", "HAPT-02-scope", "HAPT-03-impl", "D-06", "D-07", "D-08"]
  affects: ["src/main.cpp", "src/stats.h"]
tech_stack:
  added: []
  patterns: ["board-conditional #if defined(BOARD_STICKS3)", "M5.Speaker.tone() 6-arg square-wave overload", "pattern-pointer-equality dispatch", "NVS persistence via Preferences"]
key_files:
  created: []
  modified:
    - path: "src/main.cpp"
      role: "Chime engine (CHIME_WAV, CHIME_CH, freq tables, _chimeSetFreqs, vibrateTick fork, beep D-06 guard); volume constants + applyChimeVolume(); D-07 label + D-08 settings entry"
    - path: "src/stats.h"
      role: "volIdx field in Settings struct; s_vol NVS key in settingsLoad/settingsSave"
decisions:
  - "Square-wave buffer (8x0 + 8x255) chosen over sine default for chiptune Retro character (D-02); uses 6-arg M5.Speaker.tone() overload"
  - "Pattern pointer equality dispatch in _chimeSetFreqs() — reliable because PAT_* are file-scope static arrays with unique link-time addresses"
  - "_vibPat != nullptr as chime-active guard in beep() (D-06) rather than M5.Speaker.isPlaying() which returns 0 during rest gaps (Pitfall 6)"
  - "ledcAttach/ledcWrite in setup() guarded with #if !defined(BOARD_STICKS3) (Pitfall 5 hygiene)"
  - "StickC Plus build also passed (BONUS — both envs green)"
metrics:
  duration: "~12 min"
  completed: "2026-06-30"
  tasks_completed: 3
  files_modified: 2
---

# Phase 04 Plan 01: Chime Engine + Settings Summary

**One-liner:** Square-wave chiptune chimes via M5.Speaker (ES8311) on the StickS3, with D-06 beep guard, D-07 "chime" label, and D-08 user-configurable volume cycler persisted via NVS — all confined behind `#if defined(BOARD_STICKS3)`; the StickC Plus ledcWrite motor path is byte-for-verbatim in the `#else` branches.

## Tasks Completed

| Task | Name | Commit | Key Files |
|------|------|--------|-----------|
| 1 | Board-conditional chime engine + D-06 beep guard | 3fa4b2c | src/main.cpp |
| 2 | Settings relabel (D-07) + StickS3 volume cycler (D-08) | ea38745 | src/main.cpp, src/stats.h |
| 3 | Dual-env build gate + HAPT-02 diff check | (verification only — no file changes) | — |

## What Was Built

### Task 1 — Chime engine in vibrateTick

Added inside `#if defined(BOARD_STICKS3)` immediately after the `VIBRATE_CH` declaration:

- **CHIME_WAV[16]**: 16-sample 50%-duty square wave (8× `0`, 8× `255`). Required for chiptune character — the library default `_default_tone_wav` is a sine wave (verified from `Speaker_Class.cpp:73`).
- **CHIME_CH = 0**: Dedicated virtual channel for event chimes. UI beep uses `channel = -1` (auto-select), keeping the two audio paths on separate channels.
- **Retro voice frequency tables** (D-02/D-03):
  - `CHIME_APPROVE_FREQ = 659.0f` (E5 — bright "yes")
  - `CHIME_DENY_FREQ    = 220.0f` (A3 — dark low "no", ~3 octaves apart — unmistakable)
  - `CHIME_ATTN_FREQS[]  = {330, 330, 784}` (E4, E4, G5 — mirrors low-low-HIGH amplitude shape)
  - `CHIME_CELEB_FREQS[] = {523, 659, 784, 1047}` (C5→E5→G5→C6 ascending major arpeggio)
  - `CHIME_CONN_FREQS[]  = {392, 523}` (G4→C5 perfect fourth "ding-dong")
- **`_vibFreq` / `_vibFreqArr`** StickS3-only state variables (after `_vibAmp`).
- **`_chimeSetFreqs(const uint16_t* pat)`** helper: dispatches pattern pointer → frequency data via pointer equality on the file-scope `PAT_*` statics (D-04 theme-ready layout).
- `_chimeSetFreqs(pat)` injected in both `vibratePatternAmp` and `vibratePatternAmpArr` before return, inside `#if defined(BOARD_STICKS3)`.
- **vibrateTick** forked: StickS3 path drives `M5.Speaker.tone(freq, dur, CHIME_CH, true, CHIME_WAV, sizeof(CHIME_WAV))` on ON steps and `M5.Speaker.stop(CHIME_CH)` on OFF steps / pattern end. StickC Plus `#else` branches contain the original `ledcWrite(VIBRATE_PIN, amp)` and `ledcWrite(VIBRATE_PIN, 0)` calls verbatim (HAPT-02).
- **D-06 beep guard**: `beep()` calls `if (!_vibPat) compatBeep(freq, dur)` on StickS3; bare `compatBeep(freq, dur)` on StickC Plus. Uses `_vibPat != nullptr` not `M5.Speaker.isPlaying()` (avoids Pitfall 6 — silent gaps during multi-note patterns).
- **setup() LEDC guard**: `ledcAttach` / `ledcWrite` wrapped in `#if !defined(BOARD_STICKS3)` (Pitfall 5 hygiene).

### Task 2 — Settings relabel + volume cycler

**D-07 label** (`src/main.cpp`): `settingsItems[]` index 2 is board-conditional — `"chime"` on StickS3, `"vibrate"` on StickC Plus. `applySetting` `case 2` (`s.vibrate = !s.vibrate`) is unchanged.

**D-08 volume entry** (`src/main.cpp`): "volume" inserted at index 12 on StickS3 only. Board-aware index symbols:
- StickS3: `IDX_VOLUME=12`, `IDX_RESET=13`, `IDX_BACK=14`, `SETTINGS_N=15`
- StickC Plus: `IDX_RESET=12`, `IDX_BACK=13`, `SETTINGS_N=14`

`VOL_LEVELS[] = {0, 64, 128, 192, 255}`, `VOL_LABELS[] = {"mute","low","med","high","max"}`, default index 2 = 128.

`applyChimeVolume()` calls `M5.Speaker.setVolume(VOL_LEVELS[settings().volIdx])` on StickS3 (no-op stub on StickC Plus). Called from `setup()` after `settingsLoad()` and from `applySetting` `IDX_VOLUME` case.

`drawSettings()`: StickS3-only `IDX_VOLUME` row shows `VOL_LABELS[s.volIdx]`. Stray `i == 13` brightness branch guarded with `#if !defined(BOARD_STICKS3)` so it does not fire on the StickS3 reset row (index 13 = reset, not brightness).

**Persistence** (`src/stats.h`): `uint8_t volIdx` added to `Settings` struct; `settingsLoad` reads `s_vol` NVS key (default 2, bounds-clamped to 0–4); `settingsSave` writes `s_vol`.

### Task 3 — Build gate + HAPT-02 diff check

**StickS3 build**: SUCCESS (primary gate — D-09).

**StickC Plus build**: SUCCESS (BONUS — non-blocking sanity check per D-09, device repurposed; confirms the `#else` branches produce a valid ESP32 binary).

**HAPT-02 diff confirmation**: `git diff HEAD~2 HEAD -- src/main.cpp | grep -A5 -B5 "ledcWrite"` shows every `ledcWrite(VIBRATE_PIN, ...)` call sits inside an `#else` branch or `#if !defined(BOARD_STICKS3)` block:
- Pattern-end `ledcWrite(VIBRATE_PIN, 0)` → `#else` of `#if defined(BOARD_STICKS3)` (M5.Speaker.stop)
- ON-step `ledcWrite(VIBRATE_PIN, amp)` → `#else` of `#if defined(BOARD_STICKS3)` (M5.Speaker.tone)
- OFF-step `ledcWrite(VIBRATE_PIN, 0)` → `#else` of `#if defined(BOARD_STICKS3)` (M5.Speaker.stop)
- setup() `ledcAttach`/`ledcWrite` → `#if !defined(BOARD_STICKS3)`

No `ledcWrite` call is visible to the StickS3 translation unit. All `M5.Speaker.*` calls are invisible to the StickC Plus translation unit. The `#else` ledcWrite lines are the original code verbatim.

## Build Results

| Environment | Status | RAM | Flash |
|-------------|--------|-----|-------|
| m5stack-sticks3 (PRIMARY) | SUCCESS | 20.2% | 15.3% |
| m5stickc-plus (non-blocking) | SUCCESS | 24.7% | 73.0% |

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None. All features are fully wired:
- Chime tones play on actual M5.Speaker calls
- Volume cycles through real `VOL_LEVELS[]` values and calls `M5.Speaker.setVolume()`
- Settings persist through `s_vol` NVS key

## Threat Flags

No new threat surface introduced. All changes confirmed against the plan's threat model:
- Audio chime output is non-security-relevant (T-04-01: accepted)
- `_vibPat` guard in `beep()` reads a flag set only by internal `vibratePattern*` calls (T-04-02: accepted)

## Self-Check

### Created files exist
- `.planning/phases/04-haptics-chimes/04-01-SUMMARY.md` — this file

### Commits exist
- `3fa4b2c` — Task 1 chime engine + beep guard
- `ea38745` — Task 2 settings relabel + volume cycler

## Self-Check: PASSED
