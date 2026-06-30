---
phase: 04-haptics-chimes
verified: 2026-06-30T09:00:00Z
status: passed
score: 8/8 must-haves verified
overrides_applied: 0
---

# Phase 4: Haptics → Chimes — Verification Report

**Phase Goal:** Haptic events become audible chimes on the StickS3 via `M5.Speaker` (ES8311),
while the StickC Plus keeps its LEDC vibration-motor path, and the settings UI reflects the change.

**Verified:** 2026-06-30
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | All chime + volume code confined behind `#if defined(BOARD_STICKS3)` | VERIFIED | `CHIME_WAV`, `CHIME_CH`, freq tables, `_vibFreq`/`_vibFreqArr`, `_chimeSetFreqs`, `VOL_LEVELS`/`VOL_LABELS`, real `applyChimeVolume()` body — all inside `#if defined(BOARD_STICKS3)` blocks in `src/main.cpp` |
| 2 | Five events play square-wave chiptune chimes via `M5.Speaker` on the StickS3 | VERIFIED | `vibrateTick` ON step: `M5.Speaker.tone(freq, _vibPat[_vibStep], CHIME_CH, true, CHIME_WAV, sizeof(CHIME_WAV))` inside `#if defined(BOARD_STICKS3)`; `_chimeSetFreqs` dispatches all five PAT_* pointers to distinct frequency data |
| 3 | approve (659 Hz E5) and deny (220 Hz A3) play clearly different pitches; hardware confirmed audible | VERIFIED | `CHIME_APPROVE_FREQ = 659.0f`, `CHIME_DENY_FREQ = 220.0f` in code; human listening test (04-02) PASSED — approve/deny described as "unmistakable", deny clearly audible at 220 Hz (A4/440 fallback not needed) |
| 4 | UI beep suppressed while chime is active on StickS3 (D-06 guard) | VERIFIED | `beep()`: `#if defined(BOARD_STICKS3)` → `if (!_vibPat) compatBeep(freq, dur)` using `_vibPat != nullptr` (not `M5.Speaker.isPlaying()`), per Pitfall 6; `#else` → bare `compatBeep(freq, dur)` |
| 5 | Settings label "chime" on StickS3, "vibrate" on StickC Plus | VERIFIED | `settingsItems[]` index 2 = `"chime"` under `#if defined(BOARD_STICKS3)`, = `"vibrate"` under `#else`; `applySetting case 2` (`s.vibrate = !s.vibrate`) is board-identical |
| 6 | StickS3-only "volume" entry cycles levels, persists to NVS, drives `M5.Speaker.setVolume()` | VERIFIED | `VOL_LEVELS[] = {0,51,102,153,204,255}`, `VOL_LEVELS_N=6`, default index 3 (60%); `applyChimeVolume()` → `M5.Speaker.setVolume(VOL_LEVELS[settings().volIdx])`; `settingsLoad` reads `s_vol` (clamp `>5` → 3); `settingsSave` writes `s_vol`; volume called in `setup()` after `settingsLoad()` and in `applySetting IDX_VOLUME` case; hardware confirmed mute working and persistence across reboot |
| 7 | StickC Plus LEDC motor path unchanged | VERIFIED | All three `ledcWrite(VIBRATE_PIN, …)` sites (pattern-end, ON step, OFF step) are in `#else` branches; `ledcAttach`/initial `ledcWrite(0)` in `setup()` wrapped in `#if !defined(BOARD_STICKS3)` (Pitfall 5 hygiene); `#else` lines are original code verbatim |
| 8 | `pio run -e m5stack-sticks3` builds with zero errors | VERIFIED | Live build during verification: SUCCESS in 4m05s, RAM 20.2%, Flash 15.3% |

**Score:** 8/8 truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/main.cpp` | Chime engine (CHIME_WAV, freq tables, `_chimeSetFreqs`, forked `vibrateTick`, D-06 beep guard, D-07 label, D-08 volume cycler) | VERIFIED | All elements present and board-gated; modified in commits 3fa4b2c, ea38745, c245d38 |
| `src/stats.h` | `volIdx` in Settings struct; `s_vol` NVS read/write | VERIFIED | `uint8_t volIdx` unconditional in struct; `settingsLoad` reads with default 3 and `>5` clamp; `settingsSave` writes |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `vibratePatternAmp` / `vibratePatternAmpArr` | `_chimeSetFreqs(pat)` | `#if defined(BOARD_STICKS3)` block appended before return | WIRED | Both functions call `_chimeSetFreqs(pat)` at lines 126-128 and 141-143 |
| `vibrateTick` ON step (StickS3) | `M5.Speaker.tone(freq, dur, CHIME_CH, true, CHIME_WAV, sizeof(CHIME_WAV))` | 6-arg square-wave tone overload | WIRED | Confirmed in `vibrateTick` line 166; uses `_vibFreqArr ? _vibFreqArr[step/2] : _vibFreq` for per-event frequency |
| `beep()` | `compatBeep` suppressed while chime plays | `if (!_vibPat)` guard inside `#if defined(BOARD_STICKS3)` | WIRED | Lines 358-365; `_vibPat` stays non-null through rest gaps, reliable through all pattern steps |
| `applySetting IDX_VOLUME` + `setup()` | `M5.Speaker.setVolume()` | `applyChimeVolume()` helper | WIRED | `applyChimeVolume()` calls `M5.Speaker.setVolume(VOL_LEVELS[settings().volIdx])`; called at both sites (lines 475, 1372) |
| `settingsLoad` / `settingsSave` | `s_vol` NVS key | `Preferences::getUChar` / `putUChar` | WIRED | Read at stats.h line 223, write at line 243; `volIdx` roundtrips through NVS on every settings save |

---

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|--------------|--------|-------------------|--------|
| `vibrateTick` (StickS3 branch) | `_vibFreq` / `_vibFreqArr` | Set by `_chimeSetFreqs(pat)` from compile-time const freq tables | Yes — direct float constants per event | FLOWING |
| `drawSettings` volume row | `VOL_LABELS[s.volIdx]` | `settings().volIdx` loaded from NVS at boot | Yes — NVS-backed integer | FLOWING |
| `applyChimeVolume()` | `VOL_LEVELS[settings().volIdx]` → `M5.Speaker.setVolume()` | NVS `s_vol` key → `_settings.volIdx` | Yes — hardware speaker volume set | FLOWING |

---

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| StickS3 env builds zero errors | `pio run -e m5stack-sticks3` | EXIT 0, SUCCESS in 4m05s, 20.2% RAM / 15.3% Flash | PASS |
| CHIME_WAV present and is a 16-byte 50%-duty square wave | grep in `src/main.cpp` | 8×`0`, 8×`255` — confirmed | PASS |
| `M5.Speaker.tone(` with `CHIME_WAV` argument present | grep in `src/main.cpp` | Found in `vibrateTick` ON step under BOARD_STICKS3 | PASS |
| `M5.Speaker.setVolume(` reached via `applyChimeVolume()` | grep in `src/main.cpp` | Found; both call sites (setup + applySetting) confirmed | PASS |
| All `ledcWrite(VIBRATE_PIN,` in `#else` branches only | grep in `src/main.cpp` | 3 sites — all in `#else` of `#if defined(BOARD_STICKS3)`; setup one in `#if !defined(BOARD_STICKS3)` | PASS |

---

### Probe Execution

No probe scripts defined or expected for this phase (hardware-flash + audio listening test is the acceptance gate; covered by the human-verify task in 04-02-PLAN).

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| HAPT-01 | 04-01-PLAN, 04-02-PLAN | StickS3 pattern engine drives M5.Speaker; five events have distinct chimes | SATISFIED | Code: `_chimeSetFreqs` dispatch + `vibrateTick` fork; Hardware: 04-02 listening test PASSED |
| HAPT-02 | 04-01-PLAN | StickC Plus LEDC motor path preserved unchanged | SATISFIED | All `ledcWrite(VIBRATE_PIN,…)` in `#else` branches; setup motor init in `#if !defined(BOARD_STICKS3)`; confirmed by git diff and both-env builds |
| HAPT-03 | 04-01-PLAN, 04-02-PLAN | "vibrate" relabelled to "chime"; beep/chime overlap resolved | SATISFIED | `settingsItems[2]` board-conditional; `beep()` D-06 guard; hardware verified: beep/chime don't clash |

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/main.cpp` | 13 | `"Claude-XXXX"` string literal | Info | Not a debt marker — part of BLE advertisement name format (4-char MAC suffix placeholder in a comment/string) |

No TBD, FIXME, or XXX debt markers found in Phase 4-modified files.

**Pre-existing stray (not a Phase 4 regression):** In `drawSettings`, the `i == 13` brightness value branch on StickC Plus prints `brightLevel` when the "back" item (index 13) is displayed. This predates Phase 4. The Phase 4 fix (`#if !defined(BOARD_STICKS3)` guard) correctly prevents this stray branch from firing on the StickS3 (where index 13 = reset), satisfying the acceptance criterion. The StickC Plus stray display is deferred as pre-existing.

---

### Human Verification Required

No outstanding human verification items. The blocking hardware acceptance gate (04-02-PLAN Task 2: "checkpoint:human-verify") was completed during the live hardware session. The user's verdict was PASS: all five chimes audibly distinct at 60% volume, approve/deny unmistakable, deny audible at 220 Hz (A4/440 fallback not needed), volume cycler changes loudness and persists across reboot, UI beep and event chime do not clash.

---

### Scope Note

Commit `c245d38` includes three user-requested opportunistic defaults applied during the hardware session: octopus as default pet (`src/buddy.cpp`), brightness default 2/4, and 12-hour clock default. These are out of Phase 4 scope per the verification brief but are benign, documented in 04-02-SUMMARY as intentional, and do not affect Phase 4 requirements. Not flagged as violations.

**Known follow-up (deferred, out of Phase 4 scope):** User reported auto clock rotation appears broken during the hardware session. Captured in 04-02-SUMMARY; not addressed in Phase 4.

---

### Gaps Summary

No gaps. All eight must-have truths are verified. The roadmap's four success criteria are all satisfied. Both HAPT-01 and HAPT-03 have hardware evidence (human listening test PASSED). HAPT-02 is satisfied by code-scope per D-09 (StickC Plus device repurposed; motor path verified by diff inspection and green StickC Plus build).

---

_Verified: 2026-06-30T09:00:00Z_
_Verifier: Claude (gsd-verifier)_
