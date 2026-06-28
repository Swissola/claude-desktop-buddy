# Handover — StickS3 + Chimes migration (resume for Phase 3 execution)

**Date:** 2026-06-28 (refreshed)
**Repo:** `C:\Projects\claude-desktop-buddy`
**Branch:** `feat/sticks3-chimes` — 25 commits, **pushed to origin** (Swissola fork; note the GitHub repo was renamed to `claude-desktop-buddy-cardputer`, a redirect — push worked).

## Where we are

| Phase | State |
|-------|-------|
| 1 — Build System | ✅ Complete & verified (dual-env platformio.ini, M5Unified+M5GFX, partitions_8mb.csv, +`-DBOARD_HAS_PSRAM`) |
| 2 — Compatibility Shim | ✅ Complete & verified (`src/compat.h` compiles+links standalone; WR-01/WR-02 fixes applied) |
| 3 — API Port | ⏭ **Planned + verified + ready to execute** (5 plans, 3 waves) — START HERE |
| 4 — Haptics → Chimes | ⬜ Not yet planned |

## How to start tomorrow

```
/gsd-execute-phase 3
```

(Works from a fresh session — it reads STATE + plans from disk. A brand-new terminal has `pio` on PATH now; an already-running shell needs `PATH="$HOME/.platformio/penv/Scripts:$PATH" pio …`.)

## Phase 3 plan (5 plans, 3 waves)

- **Wave 1 — 03-01:** amend `compat.h` — board-agnostic `M5.Power` power block (RF-04), rail-cut idle-sleep bodies (RF-03/D-08), `compatBeep` (M5.Beep→M5.Speaker, D-10), `BUTTON_A/B_PIN` macros.
- **Wave 2 (parallel by design, runs sequentially on the main tree):**
  - **03-02** port `main.cpp` (includes, `M5.begin(cfg)` w/ speaker enabled D-09, core-3.x LEDC, all Axp/Rtc/Beep swaps)
  - **03-03** port `stats.h` / `xfer.h` / `data.h`
  - **03-04** forward-decl fix (`class TFT_eSPI;` at buddy.h:11 / character.h:27) + the 24 `#include` swaps across buddy/character/20 buddies
- **Wave 3 — 03-05:** dual-env build (`m5stickc-plus` FIRST) to zero-error + **blocking human-verify checkpoint** (`autonomous: false`).

## Execution gotchas (read before running)

1. **`m5stickc-plus` env has NEVER fully compiled** on this toolchain (Phase 1 only built the S3 env). Expect first-time errors beyond the mapped ones (research anticipates ~2–4: datum names, incidental legacy symbols). The executor is told to iterate.
2. **First build is slow** (~minutes, compiles M5Unified/M5GFX). Not a hang — use a generous timeout. Toolchain is cached.
3. **Use the Bash tool (MINGW64 / POSIX) for verify steps, not PowerShell.** Confirmed working.
4. **03-05 is a BLOCKING human-verify checkpoint** — you need the **StickC Plus device on hand** to run the 6-item smoke test (battery %, clock/RTC, LED, idle power draw, UI beep, button-wake). The phase will land in `human_needed`; the automated bar is "both envs compile zero-error." StickS3 hardware test only if you have the device.
5. **IDE Problems tab shows tons of errors right now — EXPECTED, not build errors.** 24 `src/` files still `#include <M5StickCPlus.h>` (removed in Phase 1); IntelliSense can't resolve it → cascade. Phase 3 is literally the fix; they clear after the port + a rebuild. (IntelliSense currently indexes the stale deleted `compat-probe` env; optional fix not yet applied: add `default_envs = m5stack-sticks3` under a `[platformio]` section + rebuild IntelliSense index.)

## Key Phase-3 facts (already in 03-RESEARCH.md / 03-CONTEXT.md)

- **RF-04:** `M5.Axp` exists on NEITHER board now → `M5.Power` collapses most helpers board-agnostic; only rail-sleep + LED stay board-specific. Direct `M5.Axp.*` also in `stats.h`/`xfer.h`.
- **2 new blockers:** core 3.3.9 deleted `ledcSetup`/`ledcAttachPin` → migrate motor to `ledcAttach`/`ledcWrite(pin,…)` (compile only, behavior = Phase 4); `BUTTON_A/B_PIN` macros undefined → compat.h provides them.
- **O1 RESOLVED:** StickS3 buttons are **GPIO11 (BtnA) / GPIO12 (BtnB)** `[VERIFIED]` from M5Unified `board_M5StickS3` — the old 37/39 were classic-ESP32 ADC pins, WRONG for the S3.
- **O2:** StickC-Plus coulomb gauge dropped; `getBatteryLevel()` on both boards (flagged for the D-08 hardware check).
- Decisions: **D-08** preserve StickC Plus behavior + HW verify · **D-09** unified `M5.begin` enabling speaker now · **D-10** shim M5.Beep→M5.Speaker.

## Scope guard — Phase 3 EXCLUDES (these are Phase 4)
Event chimes / haptic tone sequences · motor-LEDC vs speaker-channel collision (just make the motor compile, keep behavior) · "vibrate"→"chime" settings relabel. v2 (RECON-01/02, BUZZ-01) stay fully deferred.

---
*After Phase 3: Phase 4 — Haptics → Chimes (discuss → plan → execute).*
