---
phase: 03-api-port
type: verification
verdict: PASS
verified: 2026-06-29
requirements: [PORT-01, PORT-02, PORT-03]
---

# Phase 3 (API Port) — Verification

**Verdict: PASS** — the phase goal is achieved: the Phase-2 `compat.h` shim is
wired into the application sources so **both** PlatformIO environments compile
zero-error from one identical `src/` tree, and the StickS3 is confirmed running
on real hardware.

## Goal-backward check

Phase goal: replace direct M5StickCPlus API usage across `src/` with the unified
M5Unified API + compat helpers, resolve the `TFT_eSPI` forward-decl conflicts,
and have **both** envs build from one tree (PORT-01/02/03).

| Requirement | Evidence | Status |
|-------------|----------|--------|
| PORT-01 (main.cpp port) | `main.cpp` includes `compat.h`; unified `M5.begin(cfg)` (D-09); all `M5.Axp/Rtc/Beep` → compat helpers; core-3.x LEDC. 0 live legacy calls. | ✅ |
| PORT-02 (buddy/character/headers/stats/xfer/data) | `TFT_eSPI` forward decls removed; 24 files swapped to `compat.h`; `stats.h`/`xfer.h`/`data.h` ported. 0 `<M5StickCPlus.h>` includes remain. | ✅ |
| PORT-03 (both envs compile zero-error, one tree) | `pio run -e m5stickc-plus` → SUCCESS; `pio run -e m5stack-sticks3` → SUCCESS; both link artifacts present; no per-env source forks. | ✅ |

## Regression guard (whole `src/` tree)
- `<M5StickCPlus.h>` includes: **0**
- live `M5.Axp` / `M5.Rtc` / `M5.Beep`: **0**
- `ledcSetup` / `ledcAttachPin` / `Wire1`: **0**

## Hardware
- **StickS3: VERIFIED on hardware** — boots → splash → ASCII buddy → stable.
  Required four StickS3-scoped runtime fixes (PSRAM disable, NULL sprite parent,
  HWCDC `available()==-1` splash freeze, USB keep-awake) found via debug session
  `sticks3-bootloop` (resolved). PSRAM intentionally disabled — the app doesn't
  use it (runs PSRAM-free on StickC Plus; ~20% internal SRAM used). User decision.
- **StickC Plus: compile-verified only** — D-08 on-device smoke test **dropped**
  because the user repurposed the physical device. The single shared change
  (`data.h` `available() > 0`) is behavior-neutral on the StickC Plus (UART0's
  `available()` never returns `-1`); all other Phase-3 changes are StickS3-env scoped.

## Outstanding / carried forward
- StickC-Plus runtime smoke test (D-08) — **dropped, device repurposed.** Not
  blocking; re-run if a device returns (esp. A1 LDO-restore voltage `[ASSUMED 3000mV]`).
- Phase-4 scope (event chimes, motor/speaker LEDC collision, settings relabel) —
  not introduced this phase, as intended.
