---
slug: sticks3-battery-reboot
status: awaiting_human_verify
trigger: "M5StickS3 reboots (crashes to the buddy splash) roughly every ~15s while BLE-connected and running on BATTERY power; stable on USB power; unit runs warm (elevated current draw). Surfaced after the Phase 3 API port + Phase 4 chimes, on first extended untethered/on-wrist use."
created: 2026-06-30
updated: 2026-06-30
---

# Debug: StickS3 battery reboot + warm-running

## Symptoms

- **Expected:** On battery, the StickS3 stays running while BLE-connected, cool/cold, and sleeps/throttles when idle (like a watch should).
- **Actual:** On battery + BLE-connected, the device **reboots to the buddy splash roughly every ~15 seconds** in a loop, and runs **warm** (warmer than acceptable for a wrist device — indicates elevated current draw / no low-power state).
- **Error messages:** None capturable. This board's USB-Serial/JTAG does NOT deliver app `Serial` output to the host (confirmed: 40s pyserial capture on COM8 returned zero bytes), and panic/boot logs route to UART0 physical pins (GPIO43/44), not USB — same wall hit during the resolved `sticks3-bootloop` session. **Serial backtrace is not available on this board.**
- **Timeline:** First observed 2026-06-30 during the first extended untethered (battery) + live-bridge-connected session, after the Phase 3 API port and Phase 4 chimes. Earlier phases were only ever exercised on USB power.
- **Reproduction:** Power on battery (no USB), connect the cc-buddy-bridge over BLE, leave idle/connected. Reboot cycle begins (~15s period).

## Evidence (gathered before session handoff)

- timestamp: 2026-06-30
  USB vs battery is the decisive split. **On USB:** a 40s pyserial capture on COM8 saw NO USB re-enumeration → device did NOT reboot in 40s (stable), chimes enabled. **On battery:** bridge daemon log shows the device dropping the BLE link `disconnected after 19.2s` then `disconnected after 14.1s` — i.e. the device is rebooting every ~14-19s (each reboot drops BLE; daemon reconnects; repeat). Stable-on-USB + reboot-on-battery ⇒ POWER/CURRENT related, not pure code logic.
- timestamp: 2026-06-30
  **Speaker/chimes RULED OUT.** User turned BOTH `chime` (s.vibrate) and `sound` (s.snd) OFF in settings (speaker fully idle — no chimes, no UI beeps) and the device STILL rebooted on battery. So the brownout-from-speaker-current hypothesis is eliminated. The new Phase-4 speaker load is not the trigger.
- timestamp: 2026-06-30
  Device runs **warm** (user: "not hot hot, just warm... warmer than I'd want on my wrist"). Warm = elevated steady current draw ⇒ the CPU/rails are likely NOT dropping into the intended low-power state on the StickS3.
- timestamp: 2026-06-30
  Bridge reports `stick battery: 60%` then `59%` (draining) and `stick LittleFS appears unformatted (fsTotal=0)` and `stick link: UNENCRYPTED` after a re-pair. The LittleFS-unformatted + unencrypted-link are likely unrelated side notes, but record them.

## Eliminated

- hypothesis: Speaker/ES8311 chime current spike browns out the battery rail.
  why: Reboots persist on battery with chime AND sound both disabled (speaker idle). Eliminated.
- hypothesis: The Phase-4 clock-rotation change or 12hr/clock code crashes.
  why: The clock view only renders on USB (`_onUsb` gate), and the device is STABLE on USB. The reboot is a battery-only/off-USB phenomenon, so the clock path is not involved.

## Current Focus

reasoning_checkpoint:
  hypothesis: "On battery the screen-off idle block (main.cpp:1801-1807) fires at SCREEN_OFF_MS=15000ms idle and calls setCpuFrequencyMhz(40) while the BLE link is STILL connected. On the ESP32-S3 (StickS3) lowering the CPU clock to 40MHz drops the APB clock the active BLE controller depends on, resetting the chip. The block runs ONLY off-USB because keepAwakeOnUsb = _onUsb (BOARD_STICKS3, main.cpp:1797) is true on USB → on USB the block is skipped and setCpuFrequencyMhz(40) is never called → stable. The continuous boot→15s→reset loop keeps the device at 240MHz with display+radio active 100% of the time → never reaches any low-power state → warm."
  confirming_evidence:
    - "Reboot period is exactly SCREEN_OFF_MS=15000ms (bridge log: drops at 14.1s/19.2s, variance = daemon connect-time offset from lastInteractMs). A code-driven 15s event, not random radio-load — refutes the brownout-under-BLE-load framing."
    - "USB/battery split is fully explained by keepAwakeOnUsb=_onUsb (main.cpp:1796-1800): USB skips the screen-off block (no setCpuFrequencyMhz(40)); battery enters it. Matches 'stable on USB, reboots on battery' exactly."
    - "Lowering CPU frequency REDUCES current draw, so setCpuFrequencyMhz(40) cannot cause a brownout — the reset is APB-clock starvation of the live BLE controller, a known ESP32 hazard when changing CPU freq with the radio active."
    - "The bleIdleSleep/compatRailSleep path (main.cpp:1815-1825) requires the screen-off block to complete first; the 40MHz crash fires before it is ever reached, so the rail no-op stub (compat.h:128) is NOT the cause. Eliminated speaker-brownout (chime+sound off still reboots) and clock code (USB-only render) already rule out the other candidates."
  falsification_test: "Route all setCpuFrequencyMhz() calls through a board-conditional compatSetCpuMhz() that is a NO-OP on BOARD_STICKS3 (chip stays at its 240MHz boot freq; deep-idle power still comes from screen-off + esp_light_sleep_start which halts the CPU regardless of clock). Build must stay green. On battery the ~15s reboot loop must stop and the device must run cool. If it STILL reboots at ~15s, the crash is not the frequency change (pivot to compatBacklight(false)/setBrightness(0) at 1803, the only other battery-only call in that block)."
  fix_rationale: "Removes the exact crashing call (CPU throttle while BLE connected) on the S3 without touching the StickC Plus AXP/classic-ESP32 path. The 40MHz throttle is a minor power optimization; the real idle win is screen-off backlight + esp_light_sleep_start (which stops the CPU entirely, making the pre-sleep clock irrelevant). A clean no-op is lower-risk than a connection-state-conditional throttle I cannot validate on hardware."
  blind_spots: "No hardware to confirm. If setBrightness(0) is co-responsible the loop may persist (covered by the falsification pivot). Staying at 240MHz in the connected-screen-off window draws more than 40MHz would, but with backlight off it is far below the reboot-loop draw and transitions to light-sleep (real win) within the sleep timeout when sleepIdx>0."
next_action: Add compatSetCpuMhz() board helper to compat.h; replace the 4 setCpuFrequencyMhz() sites in main.cpp; build m5stack-sticks3 green; commit; document one on-device battery test.

## Investigation leads (code)

- `src/main.cpp:1814` `const bool keepAwakeOnUsb = _onUsb;` (BOARD_STICKS3) vs `:1816` `_onUsb && dataConnected()` (StickC Plus) — board-divergent keep-awake gate. Trace what `keepAwakeOnUsb=false` (battery) actually does downstream (`:1818` block, `setCpuFrequencyMhz(40)` at `:1823`).
- `src/main.cpp:340` `lightSleepUntilEvent()` — `esp_light_sleep_start()`; the deep-idle fast path at `:1425-1432` calls it only when `screenOff && bleIdleSleep`. When BLE-CONNECTED (not idle-sleeping), the device may run the FULL loop at 240MHz forever → warm. Check whether a connected-but-idle device ever throttles/sleeps on battery.
- `src/main.cpp:313-336` `compatRailSleep()`/`compatRailWake()` + `:267` `setCpuFrequencyMhz(240)` — the AXP LDO2/LDO3 rail cuts. Inspect `src/compat.h` for the StickS3 implementations — are they real (ESP32-S3 power equivalents) or no-op stubs? A no-op rail/sleep path on the StickS3 = never-low-power = warm + battery brownout.
- `src/compat.h` — the board-conditional power abstraction (AXP192 vs StickS3). The root cause is most likely here: StickS3 power-down/throttle not implemented.

## Constraints (AUTONOMOUS run — read carefully)

- **Hardware is in the user's hand and is NOT available to this session.** Do NOT attempt to flash, capture serial, or observe the device. Work from CODE ANALYSIS + the documented evidence only.
- **Serial backtrace is impossible on this board** (USB-Serial/JTAG delivers no app output; panic→UART0 pins). Do not propose serial capture as a self-verification step.
- **Do NOT ask the user questions.** Run solo to a root-cause diagnosis and a candidate fix.
- **Build is the only automated verification available:** `PATH="$HOME/.platformio/penv/Scripts:$PATH" pio run -e m5stack-sticks3` must stay green (use the Bash/POSIX tool, not PowerShell). The StickC Plus env (`m5stickc-plus`) is non-blocking per project decision D-09 but should also compile if the power path is shared.
- Goal: produce a clear Root Cause + apply the most-likely-correct, LOW-RISK code fix (board-conditional under `#if defined(BOARD_STICKS3)`; do NOT alter the StickC Plus AXP path), keep the build green, and document EXACTLY ONE on-device test for the user to run on battery afterward (e.g. "on battery + connected, confirm it no longer reboots and runs cool").
- Reference the resolved `.planning/debug/resolved/sticks3-bootloop.md` for board facts (ESP32-S3-PICO-1-N8R8, native USB, no AXP, flash method).

## Resolution

root_cause: |
  On battery, the screen-off idle block (main.cpp:1801-1807) fires at SCREEN_OFF_MS=15000ms
  of idle and calls setCpuFrequencyMhz(40) WHILE THE BLE LINK IS STILL CONNECTED. On the
  ESP32-S3 (StickS3), dropping the CPU clock to 40MHz starves the APB clock the live BLE
  controller depends on and RESETS the chip — a clean ~15s reboot loop (bridge log: link
  drops at 14.1s/19.2s; variance = daemon-connect offset from lastInteractMs). It is
  battery-ONLY because the StickS3 keep-awake gate is keepAwakeOnUsb = _onUsb (main.cpp:1797):
  on USB the screen-off block is skipped so setCpuFrequencyMhz(40) is never called (stable);
  on battery it runs (reset). The continuous boot→15s→reset loop keeps the device at 240MHz
  with display + radio active 100% of the time, never reaching any low-power state → runs WARM.
  Brownout framing REFUTED: lowering CPU frequency reduces current and cannot brown out the
  rail; the precise 15s periodicity is a code-driven event, not random radio-load. The
  compatRailSleep no-op stub is NOT involved — the 40MHz reset fires before the bleIdleSleep
  rail/light-sleep path is ever reached.
fix: |
  Added a board-conditional compatSetCpuMhz() helper (src/compat.h): on BOARD_STICKS3 it is a
  NO-OP (the chip stays at its 240MHz boot frequency, so the CPU clock is never lowered while
  the radio is live → no APB-starvation reset); on StickC Plus it calls setCpuFrequencyMhz(mhz)
  exactly as before (AXP/classic-ESP32 path unchanged). Replaced all four setCpuFrequencyMhz()
  call sites in src/main.cpp (wake-restore :267, nap-start :1774, nap-end :1777, screen-off
  throttle :1806) with compatSetCpuMhz(). The deep-idle power win is unaffected: it comes from
  screen-off backlight + esp_light_sleep_start (which halts the CPU regardless of clock), not
  from the 40MHz throttle.
  REJECTED ALTERNATIVES: (a) throttle only when BLE disconnected — more correct for warmth but
  adds connection-state branching I cannot validate without hardware; (b) raise the brownout
  detector threshold — wrong mechanism (not a brownout); (c) change keepAwakeOnUsb — would keep
  the screen lit on battery (defeats power saving) and does not fix the underlying unsafe call.
verification: |
  SELF-VERIFIED (build only — hardware is in the user's hand, not available this session):
    - pio run -e m5stack-sticks3 → SUCCESS (17.6s), RAM 20.2%, Flash 15.3%, zero errors.
    - pio run -e m5stickc-plus → SUCCESS (shared power code still compiles; AXP path untouched).
  Serial backtrace is impossible on this board (USB-Serial/JTAG yields no app output; panic→UART0
  pins), so runtime confirmation is the single on-device test below.
  PENDING ON-DEVICE TEST (user, on battery): power on with NO USB, connect the cc-buddy-bridge
  over BLE, leave it idle/connected past 30s. PASS = the BLE link stays up (no ~15s reboot loop
  in the bridge daemon log) AND the unit runs cool/cooler than before. FAIL (still reboots ~15s)
  = the reset is not the CPU-frequency change; pivot to compatBacklight(false)/setBrightness(0)
  at main.cpp:1803, the only other battery-only call in that block.
files_changed:
  - src/compat.h: added board-conditional compatSetCpuMhz() (no-op on BOARD_STICKS3, native setCpuFrequencyMhz on StickC Plus)
  - src/main.cpp: routed all four setCpuFrequencyMhz() calls through compatSetCpuMhz()
