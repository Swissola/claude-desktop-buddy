---
slug: sticks3-bootloop
status: resolved
trigger: "M5StickS3 black-screens and bootloops on first hardware boot after Phase 3 API port"
created: 2026-06-29
updated: 2026-06-29
resolution:
  root_cause: "Three independent StickS3-only defects, each masking the next. (1) arduino-esp32's pre-setup() psramInit() hook crashed on the module's 8MB QUAD/3.3V PSRAM probed in octal mode (qio_opi + CONFIG_SPIRAM=y) -> bootloop before app code. (2) Once booting, the global TFT_eSprite spr(&M5.Lcd) captured a NULL _parent at static-init (before M5Unified binds Lcd) -> pushSprite/pushRotated LoadProhibited at first render. (3) The real splash-freeze: _LineBuf::feed() did `while (s.available())` but HWCDC (USB-Serial/JTAG) returns signed -1 when its rx queue is uninitialised (app never calls Serial.begin; link is BLE) -> -1 truthy -> infinite spin on loop()'s first iteration -> frozen on splash. Plus a 15s screen-off blank when daemon-less on USB."
  fix: "(1) platformio.ini: memory_type qio_opi->qio_qspi, custom_sdkconfig=CONFIG_SPIRAM=n, drop -DBOARD_HAS_PSRAM. (2) main.cpp pushFrame(): push to &M5.Display explicitly at both sites. (3) data.h: `while (s.available() > 0)`. (4) main.cpp loop(): keepAwakeOnUsb=_onUsb under #if BOARD_STICKS3. GDMA-spin/waitDMA theories (sessions 7-8) were investigated and REFUTED; no settle/DMA-disable code remains."
  verification: "User confirmed on hardware: StickS3 boots -> 'Hello!' splash -> ASCII buddy -> stable. Earlier production-timed trace telemetry confirmed setup() completes and loop() runs. StickC Plus assessed by code-scope only (S3-scoped changes + board-agnostic data.h guard that is behavior-neutral on UART0) because the user repurposed that device; not hardware-tested."
  files_changed: "platformio.ini, src/main.cpp, src/data.h, .gitignore"
  commits: "b75ee13 (PSRAM), 8ac97c9 (data.h HWCDC), 48a0123 (main.cpp crash+keep-awake), ca6c21c (gitignore)"
---

## ROOT CAUSE FOUND (session 8 — REAL cause: HWCDC available()==-1 infinite loop in dataPoll)

timestamp: 2026-06-29 (session 8 — DECISIVE telemetry via esp_rom_printf)

THE BUG: src/data.h `_LineBuf::feed()` used `while (s.available()) { c = s.read(); ... }`.
On StickS3 `Serial` is HWCDC (USB-Serial/JTAG, ARDUINO_USB_MODE=1). The app never
calls Serial.begin(), so HWCDC's rx_queue is NULL. HWCDC::available() returns **-1**
(not 0) when rx_queue==NULL (HWCDC.cpp:683-688). `while (-1)` is truthy => read()
also returns -1 => `char c = 0xFF` => buffer fills to N-1 then the loop spins
FOREVER. This hangs loop()'s FIRST iteration inside dataPoll(&tama), so the buddy
render block is never reached and the device stays frozen on the "Hello!" splash
(exactly the user's report). StickC Plus uses UART0 (available() never -1) so it
never hit this. FIX: `while (s.available() > 0)` — correct for all Streams.

EVERY PRIOR "first-push hang" CONCLUSION (sessions 5-7) WAS A SERIAL ARTIFACT:
Arduino Serial (HWCDC) silently DROPS output unless the host marks the CDC
connected and gives the drain task time — so [TR] setup-return/loop breadcrumbs
via Serial.println never reached the host, making a HEALTHY boot look hung at the
splash push. Re-routing breadcrumbs through esp_rom_printf (synchronous, direct to
the USB-JTAG FIFO — the same path the littlefs ESP_LOGE used) revealed the truth:
setup COMPLETES, the splash push RETURNS, loop STARTS, and it hangs in dataPoll.

disableDisplayDma() (session-8 GDMA hypothesis) was REFUTED and REMOVED: with DMA
fully enabled and only the feed() fix applied, telemetry shows setup-return, the
splash push returning, and the loop rendering the buddy every frame (sprNZ=114,
dispNZ=50) continuously past 33s, off=0, no panic/reset. The GDMA push path works
fine; it was never the problem.

VERIFICATION (sticks3-trace, production-timed, feed() fix, DMA ENABLED):
  [TR] setup-return @2.85s; loop n=1..751 continuous to t=33074ms (>30s);
  pre-push+post-push every frame; sprNZ=114 dispNZ=50 (buddy animating: 107-118/42-56);
  nap=0 off=0 onUsb=1 throughout (keep-awake holds past the 15s screen-off);
  ZERO panic / Guru / rst: markers.

FINAL REAL FIX SET (StickS3): (1) CONFIG_SPIRAM=n; (2) pushFrame->&M5.Display
(static-init null-parent SIOF); (3) StickS3 keep-awake-on-USB; (4) data.h feed()
`s.available() > 0` (THE boot-freeze fix). All scaffolding stripped.

## Current Focus (session 8 — session-7 settle FAILED; disable GDMA on the S3 display bus — REFUTED)

hypothesis: "The first sprite push spins inside M5GFX's ESP32-S3 GDMA path — specifically the `while (*_spi_dma_outstatus_reg & DMA_OUTFIFO_EMPTY_CH0) {}` wait (Bus_SPI.cpp:734) — because the GDMA out-FIFO never loads on the first transfer. The session-7 fix (delay(120)+waitDMA BEFORE the push) could NEVER work: a settle before the push cannot affect a spin that happens INSIDE the push after the transfer is kicked off; and waitDMA returned fine (bus idle pre-push) so the delay theory was already falsified by production still hanging. KEY source finding: in Bus_SPI::writeBytes, even when use_dma==false, if `_cfg.dma_channel` is set and length<1024 the data is copied to a flip buffer and DMA is used ANYWAY (lines 693-700). A full 135x240 frame is pushed as ~270-byte row spans, all <1024, so EVERY span takes the GDMA path. The ONLY way to avoid the GDMA spin is `_cfg.dma_channel == 0` on the bus (the `if (_cfg.dma_channel)` gate at line 691 then skips the whole DMA branch → CPU/polling path). SPI_USR is NOT permanently stuck (diag loop DMA pushes cleared it fine), so the CPU path's SPI_USR waits will resolve; only the GDMA-FIFO wait spins."
test: "After M5.begin(), on StickS3 only, grab the display SPI bus (M5.Display.getPanel()->getBus(), cast to lgfx::v1::Bus_SPI) and set config().dma_channel=0 (re-apply via config()). This flips writeBytes to the CPU path for ALL pushes. Remove the failed session-7 settle from pushFrame(). Verify on the production-timed trace build via my own telemetry (reboot via port-open), require [TR] setup-return + >=30s loop breadcrumbs + pre-push/post-push + sprNZ>0."
expecting: "setup-return prints; loop runs continuously >=30s; pre/post-push both fire; sprNZ non-zero. If setup-return now prints => the GDMA-FIFO spin was the hang and disabling DMA fixed it."
next_action: "Edit main.cpp: remove session-7 settle in pushFrame; add disableDisplayDma() helper + call after M5.begin (BOARD_STICKS3); add sprNZ/dispNZ to trace loop print. Build sticks3-trace, flash via esptool, capture COM8."

reasoning_checkpoint (FIX — session 8):
  hypothesis: "The first GDMA-backed sprite push spins forever on the GDMA out-FIFO-empty busy-wait (Bus_SPI.cpp:734, DMA_OUTFIFO_EMPTY_CH0) because the GDMA out-FIFO never loads on the very first transfer after M5.begin. Disabling the bus DMA channel (dma_channel=0) routes every writeBytes through the CPU/polling path, which has no GDMA-FIFO wait, eliminating the spin."
  confirming_evidence:
    - "Bus_SPI::writeBytes (lines 691-781): the entire DMA branch incl. the spinning FIFO-empty wait is gated on `if (_cfg.dma_channel)`. dma_channel=0 => branch skipped => CPU path (line 784+), which has memcpy-to-FIFO-register + exec_spi + SPI_USR poll only, NO GDMA-FIFO wait."
    - "Lines 693-700: even use_dma==false spans <1024B are copied to a flip buffer and sent via DMA when dma_channel!=0 — so a full frame's ~270B row spans ALL use GDMA; you cannot avoid it per-call, only by zeroing dma_channel."
    - "Session-7 delay(120)+waitDMA() BEFORE the push left production frozen => a pre-push settle does not affect the in-push GDMA wait; the settle/time hypothesis is falsified."
    - "Diag-build loop DMA pushes rendered fine (sprNZ=114/dispNZ=50) => SPI_USR clears normally and GDMA works once warm; the hang is specifically the FIRST GDMA transfer's FIFO load. Forcing CPU path sidesteps it entirely with acceptable cost (buddy is low-fps; ~32ms/frame CPU push is fine)."
  falsification_test: "Flash trace build with DMA disabled. If [TR] setup-return prints and loop runs >=30s with sprNZ>0 and pre/post-push firing, the GDMA-FIFO spin was the hang and the fix holds. If setup-return STILL never prints, the hang is elsewhere (CPU path SPI_USR wait, or not in the push at all) — pivot using the new pre/post-push trace at the splash."
  fix_rationale: "Removes the GDMA path that contains the spinning wait, at the bus-config level, so NO push can hit it. Scoped to #if defined(BOARD_STICKS3) so StickC Plus (classic ESP32, separate SPI-DMA path, field-proven) is byte-for-byte unchanged. config(dma_channel=0) only flips the runtime branch gate; register pointers recompute identically and no re-init is needed."
  blind_spots: "If the CPU path's own `while(*spi_cmd_reg & SPI_USR){}` (dc_control) is the real stuck wait, this won't help — but diag loop DMA pushes prove SPI_USR clears. dispNZ readback (M5.Display.readPixel) now uses the non-DMA read path; if that misbehaves I rely on sprNZ + post-push as the core proof. Losing DMA means slower pushes, acceptable for this device."

## Current Focus (session 7 — MECHANISM theory: first GDMA push spins on bus-busy wait — settle FIX FAILED)

hypothesis: "The splash pushFrame()@1308 hangs inside M5GFX's first sprite push. With PSRAM off (CONFIG_SPIRAM=n) the 135x240x2 sprite buffer is allocated in DMA-capable INTERNAL RAM, so LGFX_Sprite::push_sprite (LGFX_Sprite.hpp:423) passes use_dma()==true and the push takes the ESP32-S3 GDMA path. Bus_SPI::writeBytes (Bus_SPI.cpp ~700-735) busy-waits on `while (*cmd & SPI_USR){}` (prior-transfer idle) and `while (*_spi_dma_outstatus_reg & DMA_OUTFIFO_EMPTY_CH0){}` (GDMA FIFO load). On the very FIRST push after M5.begin, the bus is not yet idle/settled for a GRAM write under the generic esp32-s3-devkitc-1 + M5GFX auto-detect config, so one of these waits spins forever. Splash text is visible (blit started) but pushFrame never returns. The diag build's delay(60) right before the push let the bus reach idle, which is why diag rendered every boot and the clean (no-delay) build hangs. Per-frame LOOP pushes work without any delay => this is a ONE-TIME first-push condition, not a per-push defect."
test: "Add a one-time, BOARD_STICKS3-scoped settle + bus-idle flush (delay then M5.Display.waitDMA()) gated by a static first-push guard at the TOP of pushFrame() so it covers BOTH push sites and fires exactly once. Rebuild CLEAN m5stack-sticks3, flash, power-cycle."
expecting: "First push's bus waits resolve; pushFrame returns; setup completes; loop runs; buddy renders and STAYS stable. If it still hangs, the spin is a permanently-misbound GDMA channel (would contradict diag rendering) — revisit."
next_action: "Apply one-time settle+waitDMA guard in pushFrame() under #if defined(BOARD_STICKS3); rebuild m5stack-sticks3; flash via esptool; ONE human-verify checkpoint."

reasoning_checkpoint (FIX — session 7):
  hypothesis: "The first GDMA-backed sprite push to the ST7789 on the ESP32-S3 spins forever in Bus_SPI's SPI-busy / DMA-FIFO-empty wait because the SPI/GDMA bus is not idle when the first GRAM write begins; a one-time bus-idle settle before the first push removes the race."
  confirming_evidence:
    - "Trace build: boot reaches characterInit@1269 (LittleFS mount-fail ~0.87s), then the unconditional Serial.printf@1314 NEVER prints and [TR] setup-return NEVER prints => setup hangs in 1270-1313; user reports splash TEXT visible => pushFrame@1308 blitted but never returned => spin inside the push."
    - "Source: with CONFIG_SPIRAM=n the sprite buffer is internal DMA-capable RAM => SpriteBuffer::use_dma()==true (SpriteBuffer.cpp:156) => push_sprite passes use_dma=true (LGFX_Sprite.hpp:423-424) => Bus_SPI::writeBytes DMA branch with the S3 GDMA `while(FIFO_EMPTY){}` + `while(SPI_USR){}` busy-waits (Bus_SPI.cpp:709,733)."
    - "DIAG build (identical fixes, but ~13 Serial.flush()+delay(60) through setup incl. one @1305 immediately before the splash push) rendered the buddy on EVERY boot (sprNZ=114/dispNZ=50 readback). Removing those delays (clean/trace build) => hang. The delta is the pre-push settle => time-to-bus-idle is the operative variable."
    - "The DIAG loop pushFrame@1652 ran every frame with NO delay and rendered fine => only the FIRST push needs the settle => one-time condition, consistent with a bus not-yet-idle race rather than a structural DMA fault."
  falsification_test: "Flash the clean build with the one-time S3 settle+waitDMA before the first push. If setup now returns (buddy renders and survives past the splash, loop runs) the hypothesis holds. If it STILL freezes on the splash, the spin is not a transient bus-idle race (e.g. a permanently misbound GDMA channel) — but that would contradict the diag build rendering via the same channel, so it is ruled unlikely."
  fix_rationale: "Addresses the exact mechanism: the first push's bus-busy/DMA-FIFO waits cannot resolve until the SPI/GDMA bus is idle for the first GRAM write. waitDMA() deterministically waits for the bus (SPI_USR) to clear; a brief leading delay covers any panel-side settle and matches the proven-working diag remedy. Scoped to a one-time static guard => zero per-frame cost; the loop's per-frame pushes (proven fine without it) are untouched. #if defined(BOARD_STICKS3) => StickC Plus (classic ESP32, different non-GDMA SPI-DMA path, already field-proven) is byte-for-byte unchanged."
  blind_spots: "If the true cause were a permanently-misbound GDMA out channel, the leading delay then waitDMA could itself spin — but the diag build rendering via the same init-time channel binding rules this out. If 120ms is insufficient settle on some unit, margin can be increased; the diag proved 60ms sufficient, 120ms is 2x margin. The waitDMA is placed AFTER the delay so that even in the worst case the proven time-based remedy fires first."

## Session 7 fix + flash

- timestamp: 2026-06-29 (session 7 — fix applied + clean production flashed)
  checked: added one-time BOARD_STICKS3-scoped first-push settle in pushFrame() (main.cpp:578, static s_firstPush guard: delay(120) + M5.Display.waitDMA() before the first blit, covering BOTH push sites). Rebuilt CLEAN m5stack-sticks3 (no scaffolding): SUCCESS, zero errors, RAM 20.2% (66264 B internal SRAM), Flash 15.3%. Flashed firmware.factory.bin @0x0 via esptool (COM8, 921600, usb-reset/hard-reset). Hash of data VERIFIED.
  found: build clean; flash verified. Production carries all FOUR real fixes (CONFIG_SPIRAM=n, pushFrame->&M5.Display, StickS3 keep-awake, this first-push settle). No diag scaffolding compiled into m5stack-sticks3.
  implication: Awaiting physical power-cycle to confirm setup() now returns past the splash push and the buddy renders + stays stable.

## Session 7 evidence (mechanism via M5GFX source)

- timestamp: 2026-06-29 (session 7 — static analysis of M5GFX push/DMA path)
  checked: LGFX_Sprite.hpp:420-425 (push_sprite), misc/SpriteBuffer.cpp:116-156 (allocation source + use_dma), platforms/esp32/Bus_SPI.cpp:660-790 (writeBytes DMA branch), Panel.hpp:87-88 (startWrite/endWrite transaction counter), Panel_Device.cpp waitDMA (= _bus->wait() = `while(*spi_cmd_reg & SPI_USR)`).
  found: (1) push_sprite passes `getSpriteBuffer()->use_dma()` as the DMA flag. (2) use_dma() = `_source==Dma || heap_capable_dma(_buffer)`; with PSRAM off the buffer is internal DMA-capable RAM => true. (3) On the S3 (SOC_GDMA_SUPPORTED) the DMA writeBytes path busy-waits on `while(*cmd & SPI_USR){}` and `while(*_spi_dma_outstatus_reg & DMA_OUTFIFO_EMPTY_CH0){}` with NO timeout. (4) pushImage self-brackets startWrite/endWrite, so pushFrame BLOCKS until the transfer's waits resolve. (5) endWrite is guarded by `if(_start_count)` so it is safe to call when no transaction is open. (6) StickC Plus is a classic ESP32 => the `#elif CONFIG_IDF_TARGET_ESP32` SPI-DMA branch (different registers), not the GDMA branch => explains why StickC Plus never hit this.
  implication: The hang is the first GDMA push's bus-busy/FIFO wait spinning because the bus is not idle for the first GRAM write. A one-time, S3-scoped bus-idle settle before the first push is the targeted root-cause fix. The loop's per-frame pushes already run after the bus is warm, so they need nothing.

## Current Focus (session 6 — TRACE CAPTURED: setup() hangs, loop NEVER runs)

hypothesis: "The clean production-timed boot HANGS INSIDE setup(), in the window between characterInit(nullptr) at main.cpp:1269 and the unconditional Serial.printf at 1314. The loop() never runs. This is bucket (a) hang-location + flavored by (e) minimal-tracing/production-timing-unmasks-it: the session-4 diag build that reached setup-done had CORE_DEBUG_LEVEL=5 + ~13 Serial.flush()+delay(60) breadcrumbs through setup (notably a flush+delay(60) at 1305, immediately before the splash pushFrame at 1308). The production-timed trace build removed all those delays and now hangs."
test: "Live HWCDC capture (COM8, 303A:1001). My port-open triggers rst:0x15 (USB_UART_CHIP_RESET) => fresh boot every connect, so I already get BOOT breadcrumbs without a user power-cycle."
expecting: "Pinpoint which call between 1269 and 1314 blocks. Screen state disambiguates: BLACK => hang at/before splash draw (characterInit / applyDisplayMode); 'Hello!'/'a buddy appears' visible => splash pushFrame(1308) blitted but pushFrame did not RETURN (DMA/SPI wait spin)."
next_action: "Got screen state from user. Then add finer DIAG_TRACE points (after characterInit return; after applyDisplayMode; before/after splash pushFrame) OR a 60ms settle before the splash push, rebuild, ONE batched reflash+power-cycle to confirm fix or pinpoint."

## Session 6 evidence (DECISIVE — boot trace captured from frozen device)

- timestamp: 2026-06-29 (session 6 — two live HWCDC captures off COM8, trace build already flashed)
  checked: pio device list (COM8 @ 303A:1001, only port holder; killed cc-buddy-bridge.exe). Two pyserial captures (22s, then 30s w/ timestamps), DTR/RTS deasserted, PYTHONUTF8. Opening the port triggers rst:0x15 (USB_UART_CHIP_RESET) => each capture is a FRESH cold boot.
  found: IDENTICAL deterministic sequence both boots:
    - ROM banner -> rst:0x15 USB_UART_CHIP_RESET -> load/entry (t=0.00-0.02)
    - t~0.87s: "E esp_littlefs: Corrupted dir pair {0x0,0x1}" / "mount failed (-84)" / "Failed to initialize LittleFS" (these come from characterInit(nullptr) at main.cpp:1269, which mounts LittleFS; expected non-fatal, same as session 4)
    - THEN TOTAL SILENCE for the remaining ~21-29s. NO "[TR] setup-return" (1318). NO "[TR] loop-first" (1329). NO per-second "[TR]" loop flags. CRUCIALLY: the UNCONDITIONAL Serial.printf("buddy: %s", ...) at main.cpp:1314 NEVER PRINTS.
  implication: setup() HANGS before line 1314 — i.e. somewhere in lines 1269-1313. The loop() never executes (so the entire prior session-4/5 "loop runs, screen-off at 15s" model is moot for the clean build; that was diag-build-only timing). Buckets resolved: NOT loop-hung, NOT reaches-pushFrame-but-no-render-in-loop. It is an early SETUP hang. Candidate calls in 1269-1313: characterInit (after the mount-fail print, did it return?), applyDisplayMode (1277, benign code-read), the splash render block fillSprite/drawString (1282-1295), or pushFrame()/splash at 1308 (pushSprite/pushRotated to &M5.Display -> could spin on a display DMA/SPI-busy wait with no timeout). delay(1800) at 1310 cannot hang permanently. NOTE: BC() is a NO-OP in the trace build (DIAG_TRACE without DIAG_BREADCRUMBS), so there are zero breadcrumbs inside this window — that is the instrumentation gap that prevents pinpointing from serial alone.

## Current Focus (session 5 — NEW observation contradicts session-4 conclusion)

hypothesis: "The CLEAN production build freezes on the 'Hello!' splash; the buddy NEVER appears (no reboot/flashing => a YIELDING hang or a non-visible render, NOT a crash). Prior sprNZ=114/dispNZ=50 render evidence is INVALID for the clean build — it came from the DIAG build, whose scaffolding (CORE_DEBUG_LEVEL=5 verbose logging + ~13 Serial.flush()+delay(60) breadcrumbs in setup + a 300ms readPixel probe + 14s self-reboot) materially changed timing/serial behavior and evidently MADE the buddy render. By code reading the clean loop SHOULD reach pushFrame (setup reaches splash push 1308 -> delay(1800) -> unconditional Serial.printf 1314 is HWCDC-bounded not an infinite block -> setup returns -> loop: napping/screenOff/landscapeClock all false on first boot -> buddyTick draws ticked=true on first call -> pushFrame 1652 self-commits). Contradiction => divergence is runtime-only. Build a MINIMAL production-TIMED instrument (CORE_DEBUG_LEVEL=0, NO delays, NO self-reboot, NO readPixel probe) tracing setup-return, loop-first, per-second loop flags, pre-push, post-push."
test: "Add -DDIAG_TRACE points (no delay, throttled 1s, single flush) on a new [env:sticks3-trace] env that otherwise EXACTLY matches production flags. Build, flash via esptool @0x0, user power-cycles, capture COMx stream."
expecting: "(a) no 'setup-return' => hang in setup tail; (b) 'setup-return' + no loop traces => hang at setup->loop; (c) loop traces stop at 'pre-push' w/o 'post-push' => pushFrame hangs; (d) 'post-push' flows but screen frozen => push-without-visible-effect (display commit/DMA); (e) loop traces flow AND buddy now appears => the minimal instrument unmasked it => hyper timing/serial race."
next_action: "Add DIAG_TRACE instrument to src/main.cpp (setup-return after 1314; loop-first; throttled per-second loop flags before render block; pre-push/post-push around 1652). Add [env:sticks3-trace] to platformio.ini = exact production flags + -DDIAG_TRACE. Build pio run -e sticks3-trace. Then ONE human-action checkpoint: user power-cycle while host capture runs."

reasoning_checkpoint (FIX — session 5):
  hypothesis: "PRODUCTION already renders the buddy and is crash-free/reboot-free (proven by the diag-30s capture: identical src, 0 panics 0-30s, sprNZ/dispNZ nonzero). The ONLY reason production looks broken is the SCREEN_OFF_MS=15000 idle timeout firing at ~15s because the keep-awake guard `_onUsb && dataConnected()` is false on a daemon-less boot (dataConn=0). DIAG masked this by self-rebooting at 14s, before the 15s screen-off. For the StickS3 (a USB-powered DESKTOP buddy that sits plugged into the desk), keeping the screen awake whenever on USB power keeps the buddy visible."
  confirming_evidence:
    - "Runtime telemetry: screenOff flips 0->1 exactly when idle (millis - lastInteractMs) crosses 15000; onUsb=1, dataConn=0 the whole time."
    - "data.h:51 dataConnected() returns _lastLiveMs != 0 && within 30s => false until the bridge sends data => guard `_onUsb && dataConnected()` is false on a fresh daemon-less boot."
    - "main.cpp:1611 skips all sprite render when screenOff; compatBacklight(false) at 1687 cuts the panel => dark screen, device still running (no reboot)."
    - "diag-30s capture: ZERO Guru/panic/abort across full 0-30s cycles; the only reset is the diag's own ESP.restart (rst:0xc) at the threshold => production render path is healthy."
  falsification_test: "Build production-equivalent diag with the keep-awake-on-USB fix + 30s self-reboot; capture past 15s. If off STAYS 0 (no screen-off) while onUsb=1 and the buddy keeps rendering (dispNZ>0) to 30s, the fix holds. If off still flips to 1, the guard change did not take."
  fix_rationale: "Addresses the exact mechanism (the daemon-data-gated keep-awake) for the StickS3 desktop form factor, where USB power = 'on the desk, stay visible.' Scoped with #if defined(BOARD_STICKS3) so StickC Plus (portable, battery) keeps its existing `_onUsb && dataConnected()` power-saving behavior unchanged. The BLE idle-sleep path already requires !_onUsb (main.cpp:1699), so never runs on USB anyway — keeping the screen on while on USB is consistent with existing power management."
  blind_spots: "If the user actually WANTS the StickS3 to sleep its screen on USB after idle, this changes that. The human-verify checkpoint is where they confirm/reject. Also: on a wall charger (USB power, no PC/daemon) the StickS3 would now stay lit — acceptable for a desktop buddy, and the user can change SCREEN_OFF behavior later if undesired."

## Session 5 fix verification

- timestamp: 2026-06-29 (session 5 — keep-awake fix falsification test PASSED)
  checked: applied StickS3-scoped keep-awake-on-USB fix (main.cpp:1685, #if defined(BOARD_STICKS3) keepAwakeOnUsb=_onUsb), built diag (temporarily @30s self-reboot + extra probe fields), flashed, captured 32s.
  found: idle climbed past SCREEN_OFF_MS (15033 -> 18104) and `off` STAYED 0 the entire time; buddy kept rendering dispNZ=50-56 to the 30s self-reboot. NO off=1, NO Guru/panic. Reverted the temporary 30s->14s + probe-field edits afterward.
  implication: Fix confirmed at runtime — on USB power the StickS3 no longer blanks at 15s; the buddy stays visible. StickC Plus path (#else _onUsb && dataConnected()) unchanged.

- timestamp: 2026-06-29 (session 5 — PRODUCTION flashed)
  checked: built m5stack-sticks3 (production: CONFIG_SPIRAM=n, pushFrame->&M5.Display, keep-awake fix, CORE_DEBUG_LEVEL=0, no breadcrumbs), flashed firmware.factory.bin @0x0, hash verified.
  implication: Production now carries all three fixes. Awaiting physical power-cycle + visual confirm: buddy visible AND stays visible/stable (no blank-out at 15s, no reboot/flashing).

## Session 5 evidence

- timestamp: 2026-06-29 (session 5 start — capture returned 0 lines)
  checked: pio device list (COM8 @ 303A:1001, no port holders), two timestamped pyserial captures (35s + 20s, DTR/RTS deasserted, 115200).
  found: ZERO lines streamed. Device present + stable at 303A:1001 but silent.
  implication: The prior session's last action was "flashing clean PRODUCTION firmware.factory.bin." Production = CORE_DEBUG_LEVEL=0 + no DIAG_BREADCRUMBS => emits NOTHING on HWCDC even when running healthy. So the objective's premise that "diag is streaming right now" is stale — production is flashed. To get ground-truth telemetry I must reflash the diag build. (HWCDC flush would drain to my open port if diag were running, so 0 lines conclusively = diag NOT running.)

- timestamp: 2026-06-29 (session 5 — diag reflashed @14s, captured)
  checked: reflashed [env:sticks3-diag] (firmware.factory.bin @0x0, hash verified), captured COM8 30s with timestamps.
  found: Device streams cleanly. Boot markers: setup-enter -> rst:0xc (RTC_SW_CPU_RST) at device-uptime ~13.9s -> setup-enter (repeat). Buddy renders every cycle: nap=0 off=0 buddyMode=1 bright=255 sprNZ=114 dispNZ=50. ZERO Guru/panic/abort. The reset cause is RTC_SW_CPU_RST = software ESP.restart() = the diag's own `if (millis() > 14000) ... ESP.restart()`.
  implication: UNKNOWN #2 ANSWERED — the post-buddy reboot is the INTENTIONAL 14s diag self-reboot, NOT a crash. rst:0xc (software reset) + exact-14s timing + no panic dump = the scaffold, not a defect.

- timestamp: 2026-06-29 (session 5 — DECISIVE: bumped self-reboot to 30s to observe the 15s boundary)
  checked: changed diag self-reboot threshold 14000 -> 30000ms, added onUsb/dataConn/idle fields to the loop probe, rebuilt, flashed, captured 40s.
  found: Buddy renders healthy (off=0, dispNZ=50) until idle crosses SCREEN_OFF_MS=15000:
    - t=16546 ... off=0 ... onUsb=1 dataConn=0 idle=15031
    - t=17169 ... off=1 ... onUsb=1 dataConn=0 idle=15654
    screenOff flips 0->1 exactly when (millis - lastInteractMs) > 15000. Throughout: onUsb=1, dataConn=0. NO reboot, NO crash between 14-30s (only the timed self-reboot at 30s). After off=1 the loop skips render (main.cpp:1611) and compatBacklight(false) was called => physical backlight off => screen dark, device still alive.
  implication: UNKNOWN #1 ANSWERED. The screen-off guard is `!(_onUsb && dataConnected())` (main.cpp:1685). dataConnected() (data.h:51) is true ONLY if the buddy daemon/bridge has sent data within 30s (_lastLiveMs != 0). On a daemon-less first boot dataConn=0, so `_onUsb && dataConnected()` = false, so the SCREEN_OFF_MS=15000 idle timeout FIRES at ~15s: backlight cut, render stops, device stays up (no reboot). The DIAG build self-reboots at 14s — ~1-2s BEFORE this 15s screen-off ever fires — so diag always shows a fresh buddy each cycle; PRODUCTION (no self-reboot) runs past 15s and goes dark, which the user read as "buddy doesn't render." It is intended power-saving, mis-triggered during daemon-less bring-up. NOT an init-order/settle window and NOT a crash.

## LED interpretation (user hardware report, session 4)

- "screen off + green LED flashing while plugged in" = device was OFF/asleep; the green LED by the power button is the AXP/PMIC charge+power indicator blinking in the off/charging state. NOT running the app.
- "hold button -> LED stops flashing momentarily" = button press registered by the PMIC (long-hold ~6s = power OFF; short tap = power ON / wake).
- "tapped button -> green LED now continually lit" = device powered ON and booted the app. CONFIRMED by capture: COM8 was streaming live diag telemetry with a clean boot + self-reboot loop. So the user IS in the running state.

## Session 3 evidence

- timestamp: 2026-06-29 (session 3)
  checked: code-read of the loop buddy render path vs the splash render path (main.cpp loop 1611-1653, pushFrame 577-595, buddy.cpp buddyTick 182-205, capybara.cpp, applyDisplayMode 302-312).
  found: The splash (setup, spr.drawString -> pushFrame -> &M5.Display) and the loop buddy (buddyTick draws species art into spr via _tgt=&spr, then the SAME pushFrame) use an IDENTICAL sprite + push mechanism. createSprite succeeded (splash visible => 63KB internal-RAM sprite OK). ASCII buddy art is COMPILED-IN (SPECIES_TABLE), not read from LittleFS; speciesIdx comes from NVS Preferences (clamped, default capybara idx 0) — so empty-LittleFS clue (a) and createSprite-fail clue (b) and wrong-parent clue (c) are all eliminated by the splash working. _tgt=&spr is a link-time constant (address-of global), not subject to the static-init-order fiasco. Render gates: napping needs ~15 face-down IMU frames; screenOff needs 15s idle; clocking needs RTC valid — none fire on a fresh first-boot for the first ~15s, yet user says buddy NEVER shows.
  implication: By reading, the render SHOULD work. Remaining live hypotheses: (A) loop render branch skipped (StickS3 IMU misread -> napping immediately), (B) buddyTick draws nothing visible into spr, (C) app hangs in loop before pushFrame (but then splash would freeze, not blank). Need runtime observation of sprNZ + flags.

- timestamp: 2026-06-29 (session 3 — capture-channel limitation CONFIRMED)
  checked: flashed sticks3-diag, attempted 3 autonomous COM8 captures (post-write_flash, post flash_id reset, post hard-reset). Then `esptool --before no_reset chip_id`.
  found: ALL captures returned 0 lines. `--before no_reset` CONNECTED successfully (read eFuse/MAC) => the chip is parked in ROM download mode, i.e. the APP IS NOT RUNNING after any esptool reset. The prior session only captured serial because the BOOTLOOP re-ran boot code on its own; now that boot is fixed the app boots once and esptool's reset (RTS/USB-JTAG) does not start it.
  implication: Autonomous clean-cold-boot capture is impossible on this native-USB board. Mitigation: diag build now SELF-REBOOTS (ESP.restart()) ~14s after boot, so ONE physical power-cycle starts a self-sustaining boot->probe->reboot loop that the host can capture from autonomously thereafter. Self-rebooting diag flashed and verified.

# Debug Session: sticks3-bootloop

## Symptoms

- **Expected:** M5StickS3 boots to the buddy UI after flashing the `m5stack-sticks3` Phase-3 firmware.
- **Actual:** Black screen, continuous bootloop (resets in a tight cycle; USB-Serial/JTAG port flaps on/off each loop).
- **Error messages:** None capturable. Crash happens before the USB-Serial/JTAG console (HWCDC) comes up; arduino-esp32 routes panic/boot logs to UART0 (GPIO43/44 physical pins), not USB. 5 raw pyserial reads of COM8 returned empty.
- **Timeline:** First-ever StickS3 *hardware* run. The S3 path was only ever compile-verified before now (Phase 3 PORT-03). Immediately after the Phase 3 API port + first flash.
- **Reproduction:** Flash `firmware.factory.bin` to the StickS3 at 0x0 (hash verified OK), power on → black screen + bootloop. Deterministic.

## Environment / Facts

- Board: M5StickS3 (ESP32-S3-PICO-1-N8R8, 8MB QIO flash + 8MB OPI PSRAM @3.3V). Chip confirmed alive — esptool reads it on COM8 via `--before usb-reset` (USB mode = USB-Serial/JTAG, VID:PID 303A:1001).
- Flash succeeded and verified (not a corrupt-flash issue).
- Both PlatformIO envs compile zero-error from one source tree (PORT-03 met): `m5stickc-plus` AND `m5stack-sticks3`.
- platformio.ini `[env:m5stack-sticks3]`: board=esp32-s3-devkitc-1 (no official M5StickS3 board id), board_build.arduino.memory_type=qio_opi, board_upload.flash_size=8MB, board_build.partitions=partitions_8mb.csv.
- build_flags: -DBOARD_STICKS3 -DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1 -DBOARD_HAS_PSRAM.
- Libs: M5Unified 0.2.17, M5GFX 0.2.24.
- src/main.cpp setup(): `auto cfg = M5.config(); ... ; M5.begin(cfg)` with internal speaker + IMU enabled, mic disabled (Phase 3 decision D-09). compat.h holds the board abstraction.
- StickC Plus build was NOT tested on hardware (different device; user only has/cares about the StickS3 right now). Do NOT require StickC Plus hardware.

## Prime Hypotheses (initial, to test/eliminate)

1. **M5.begin(cfg) speaker init (D-09):** `internal_spk` enabled panics because M5Unified may not auto-detect/init the StickS3 (ES8311 over I2S) correctly when built against the generic `esp32-s3-devkitc-1` board definition.
2. **OPI-PSRAM init mismatch:** `qio_opi` + `-DBOARD_HAS_PSRAM` against the prebuilt arduino-esp32 bootloader — early "failed to init external RAM" abort.
3. **Board mis-identification by M5Unified** leading to wrong display/power pin config (could explain black screen) — but bootloop implies a panic/reset, not just a dark panel.

## Investigation Constraints

- No backtrace over USB (logs go to UART0 pins). To get a backtrace the debugger must find an alternative: route ESP-IDF console to USB-Serial/JTAG, add early `Serial`/HWCDC flush + delay before M5.begin, build with a serial-over-USB-JTAG console flag, or bisect M5.begin config by reflashing variants and observing boot/no-boot.
- Each iteration = rebuild (cached, ~seconds-to-minutes) + reflash via the WORKING method: `esptool --chip esp32s3 --port <COMx> --before usb-reset --after hard-reset write_flash -z 0x0 .pio/build/m5stack-sticks3/firmware.factory.bin` with `PYTHONUTF8=1` set (the default `pio run -t upload` HANGS on this board — native-USB needs usb-reset, and the cc-buddy-bridge.exe host process must not be holding the port).
- Device port re-enumerates between the running-app CDC (303A:832B) and the USB-Serial/JTAG / ROM downloader (303A:1001). Manual download mode: hold BtnA while tapping reset.

## Current Focus

reasoning_checkpoint:
  hypothesis: "memory_type=qio_opi builds the bootloader + IDF PSRAM controller config for OCTAL (OPI, 1.8V) PSRAM, but the ESP32-S3-PICO-1 module has 8MB QUAD PSRAM at 3.3V. Octal-mode PSRAM init on quad/3.3V silicon fails in early boot, causing a reset loop before the app's USB-OTG CDC (832B) ever enumerates -> deterministic black screen + bootloop."
  confirming_evidence:
    - "esptool eFuse readout: 'Embedded PSRAM 8MB (AP_3v3)' — 3.3V PSRAM. On ESP32-S3, octal/OPI PSRAM is 1.8V; 3.3V is the QUAD family (AP Memory APS6404-class)."
    - "esptool: 'Flash type set in eFuse: quad (4 data lines)', 'Flash voltage set by eFuse: 3.3V' — module is a quad-PSRAM 3.3V part."
    - "platformio.ini [env:m5stack-sticks3] sets memory_type=qio_opi (octal PSRAM) + -DBOARD_HAS_PSRAM — a direct config/hardware mismatch."
    - "Symptom is the textbook PSRAM-init-fail signature: deterministic, black screen, resets before app/USB console comes up."
  falsification_test: "Rebuild with memory_type=qio_qspi and flash. If it STILL bootloops, the PSRAM mode was not the cause."
  fix_rationale: "qio_qspi selects QIO flash + QUAD-SPI PSRAM, matching the AP_3v3 8MB quad hardware. PSRAM then inits in the correct mode and early boot proceeds. Addresses the root mismatch, not a symptom."
  blind_spots: "0-byte USB-JTAG capture means the PSRAM-fail message was not directly observed (console is on UART0). A residual chance the bootloop is M5.begin speaker init and PSRAM is incidental — but the config/hardware mismatch is real and must be corrected regardless; verification (does it boot?) will settle it."

- next_action: HUMAN-VERIFY checkpoint issued. Awaiting user to power-cycle the StickS3 and report screen contents. Continuation agent: see "Continuation notes" in Resolution below.

## Evidence

- timestamp: 2026-06-29 (session 2 start)
  checked: `pio device list` + tasklist for port holders
  found: Device present at COM8, VID:PID 303A:1001 (USB-Serial/JTAG / ROM downloader mode), SER=70:04:1D:D5:E0:70. STABLE at 1001, not flapping right now → device is parked (not actively bootlooping) — likely sitting post-reset in ROM, or app dies so early USB-OTG (832B) never enumerates. No cc-buddy-bridge.exe / esptool / python holding the port — free to flash/read.
  implication: USB-Serial/JTAG is the live console channel. ROM + 2nd-stage bootloader logs are capturable here even though the app panic logs go to UART0. Capturing this is the cheapest possible evidence.

- timestamp: 2026-06-29
  checked: partitions_8mb.csv
  found: Has a dedicated `coredump` partition at 0x7F0000 (64KB). app0 single OTA at 0x10000 size 0x640000, spiffs at 0x650000. Layout fits in 8MB.
  implication: A coredump partition exists → IF arduino-esp32's sdkconfig has ESP_COREDUMP_ENABLE_TO_FLASH, a panic would write a dump we can extract via `esptool read_flash` + `espcoredump` even with no live console. Backup diagnostic path. (arduino-esp32 default usually has coredump DISABLED — to confirm.)

- timestamp: 2026-06-29
  checked: src/main.cpp setup() (lines 1215-1269) + src/compat.h
  found: setup() order: M5.config{imu=on, spk=ON (D-09), mic=off, clear_display=on} -> M5.begin(cfg) -> M5.Lcd.setRotation(0) -> startBt() [esp_read_mac + bleInit] -> compatLedInit() [no-op on S3] -> ledcAttach(VIBRATE_PIN=26...) -> statsLoad/settingsLoad -> batteryInit -> buddyInit -> spr.createSprite(135,240) [PSRAM] -> characterInit -> 1.8s splash -> loop. compat.h M5.Power/M5.Speaker/M5.Imu are board-AGNOSTIC (rely on M5Unified RUNTIME board autodetect, independent of the platformio board=esp32-s3-devkitc-1 setting).
  implication: Candidate early-panic sites in order: M5.begin speaker init (ES8311/I2S on S3), createSprite(135*240*2=63KB, may target PSRAM), ledcAttach on GPIO26. M5Unified must autodetect StickS3 at runtime for speaker/IMU/display pins to be right.

- timestamp: 2026-06-29 (ROOT CAUSE)
  checked: `esptool --chip esp32s3 --before usb-reset flash-id` hardware/eFuse readout on COM8
  found: Chip = ESP32-S3-PICO-1 (LGA56) rev v0.2. "Features: ... Embedded Flash 8MB (GD), Embedded PSRAM 8MB (AP_3v3)". "Flash type set in eFuse: quad (4 data lines)". "Flash voltage set by eFuse: 3.3V". => PSRAM is 8MB AP Memory QUAD PSRAM at 3.3V (APS6404-class), NOT octal. ESP32-S3 OPI/octal PSRAM is 1.8V; 3.3V => quad.
  implication: platformio.ini memory_type=qio_opi (octal PSRAM) is WRONG for this silicon. Must be qio_qspi (quad PSRAM). The "@3.3V" detail recorded in the original Environment facts alongside "OPI PSRAM" was itself a contradiction (OPI is never 3.3V) — the overlooked smoking gun. Hard-reset + 7s USB-JTAG capture returned 0 bytes (console is on UART0, as previously established — neither confirms nor refutes).

## Resolution

root_cause: platformio.ini [env:m5stack-sticks3] set board_build.arduino.memory_type=qio_opi, configuring the bootloader/IDF for OCTAL (1.8V) PSRAM. The ESP32-S3-PICO-1 module actually has 8MB QUAD PSRAM at 3.3V (esptool eFuse: "Embedded PSRAM 8MB (AP_3v3)"). Octal-mode PSRAM init on quad/3.3V hardware fails in early boot -> deterministic reset loop before the app's USB-OTG console enumerates (black screen, no capturable backtrace).
fix: changed platformio.ini [env:m5stack-sticks3] board_build.arduino.memory_type qio_opi -> qio_qspi (with explanatory comment). Rebuilt (zero errors, new bootloader.bin) and flashed firmware.factory.bin @0x0 (hash verified).
verification:
  - SELF-VERIFIED: build succeeds zero-error with qio_qspi.
  - SELF-VERIFIED (partial/positive): after flashing the qio_qspi build, the USB-Serial/JTAG port (303A:1001, COM8) is now STABLE for >10s with NO flapping. Original symptom was the port flapping on/off every bootloop cycle. Stable = the tight early-boot reset loop appears to have stopped.
  - LIMITATION: could NOT software-trigger a clean app cold-boot to confirm the running-app CDC (303A:832B) or screen render. esptool --after hard-reset (RTS) and --after watchdog-reset both leave the chip parked in ROM/JTAG (1001) on this native-USB board (same root limitation that makes `pio run -t upload` hang). A physical power-cycle is required to cold-boot the app.
  - PENDING HUMAN: physical power-cycle + confirm screen shows the buddy UI (Hello! / a buddy appears, or owner+pet name), not black.
files_changed:
  - platformio.ini: memory_type qio_opi -> qio_qspi

## Continuation notes (for the agent resuming after the human-verify checkpoint)

If user confirms the screen now shows the buddy UI (not black) and no bootloop:
  1. status -> resolved.
  2. mkdir -p .planning/debug/resolved && move this file there.
  3. Commit code: `git add platformio.ini` then commit (message below). Then commit the resolved debug doc + knowledge-base via `gsd-sdk query commit`.
     Suggested commit subject: "fix(sticks3): use qio_qspi PSRAM mode to stop early-boot bootloop"
     Body root cause: ESP32-S3-PICO-1 has 8MB QUAD PSRAM @3.3V (eFuse AP_3v3); qio_opi (octal/1.8V) failed PSRAM init -> bootloop. qio_qspi matches the silicon.
  4. Append a knowledge-base entry (.planning/debug/knowledge-base.md). Error patterns: bootloop, black screen, ESP32-S3, PSRAM, qio_opi, AP_3v3, octal vs quad.
  5. NOTE: this is the OPTIONAL Phase-3 StickS3 hardware run; PORT-03 compile acceptance was already met — do not expand scope to StickC Plus or Phase 4 chimes.

If user reports STILL black / still bootlooping:
  - Re-poll `pio device list`: if it advanced to 303A:832B the app IS running and the issue is display-only (M5GFX panel/pin autodetect for StickS3 against board=esp32-s3-devkitc-1) — pivot hypothesis to display init, NOT PSRAM.
  - If still flapping 1001<->832B: PSRAM may not be the (only) cause; next step is the early-Serial breadcrumb build (Serial.begin + delay(3000) + per-init-step prints with flush before/after M5.begin, createSprite, etc.) to localize the panic, since console-on-UART0 blocks normal backtrace.

## Second root cause (post-PSRAM-fix) — CONFIRMED

- timestamp: 2026-06-29 (Option A applied + verified on diag)
  checked: built diag with CONFIG_SPIRAM=n (HWCDC console, ARDUINO_USB_MODE=1), flashed, captured COM8.
  found: PSRAM crash GONE — "Embedded PSRAM: No", boot runs through ALL setup() breadcrumbs (setup-enter -> ... -> before pushFrame(splash)). But a NEW deterministic crash: Guru Meditation LoadProhibited, Core 1, EXCVADDR=0x00000074, PC=0x4202dbee. addr2line (reliable now, stable address): LGFX_Sprite::push_sprite (LGFX_Sprite.hpp:423) <- pushSprite(337) <- pushFrame (main.cpp:586) <- setup(1292). Never reaches setup-done.
  implication: crash is in spr.pushSprite(0,0) -> push_sprite(_parent, ...). Probe added.

- timestamp: 2026-06-29 (display-state probe)
  checked: printed M5.Display.width/height/depth/getPanel + spr depth/w/h before pushFrame.
  found: disp w=135 h=240 depth=16 panel=0x3fced104 (VALID), sprite depth=16 w=135 h=240 (VALID). Display + panel fully initialized (M5GFX autodetect logged board_M5StickS3, created Panel_ST7789). So null-panel hypothesis ELIMINATED.
  implication: crash is not a missing display; must be the _parent pointer push_sprite derefs.

- timestamp: 2026-06-29 (parent probe — ROOT CAUSE #2 CONFIRMED)
  checked: printed spr.getParent() vs &M5.Display vs &M5.Lcd.
  found: sprParent=0x0 (NULL); &M5.Display=0x3fca07b0; &M5.Lcd=0x3fca07b0 (valid, equal). The sprite's cached _parent is null while M5.Lcd resolves fine at runtime.
  implication: STATIC INITIALIZATION ORDER FIASCO. `TFT_eSprite spr = TFT_eSprite(&M5.Lcd);` (main.cpp:11, global) is constructed before the M5Unified library global `M5` binds its `M5GFX& Lcd = Display` reference, so &M5.Lcd evaluated to NULL and was stored as _parent. pushSprite(0,0) -> push_sprite(null,...) -> null->getColorDepth() reads null+0x74 (_write_conv.depth offset) => LoadProhibited EXCVADDR=0x74. Exact match. Never hit before because the board was never run on HW past early boot (StickC Plus untested; StickS3's PSRAM crash fired earlier). Fix: pass &M5.Display explicitly at the two push sites in pushFrame() (only call sites), bypassing the null cached _parent. Correct for both boards; M5.Display is the primary display on each by the time pushFrame runs.

## Session 4 evidence (DECISIVE — render telemetry captured)

- timestamp: 2026-06-29 (session 4 — diag telemetry capture off COM8)
  checked: `pio device list` (COM8 @ 303A:1001, SER 70:04:1D:D5:E0:70, no port holders), then 16s pyserial read of COM8 @115200 with DTR/RTS deasserted. NOTE: diag uses ARDUINO_USB_MODE=1 = HWCDC, which enumerates as 303A:1001 — SAME VID:PID as the ROM downloader, so running-vs-parked is NOT distinguishable by VID:PID; only a live read settles it.
  found: Device was ALREADY running (user's power-button tap booted it). Captured a FULL clean cycle + self-reboot:
    - Clean boot: "Embedded PSRAM: No" (CONFIG_SPIRAM=n took), runs ALL breadcrumbs setup-enter -> after M5.begin -> after createSprite ptr=0x3fcc2efc -> characterInit (LittleFS not formatted, non-fatal) -> setup-done -> "buddy: ASCII mode" -> loop-first-iter. ZERO Guru/panic. Self-reboot at ~t=14s, then re-boots identically.
    - Loop render probe (every ~300ms): `nap=0 off=0 buddyMode=1 charLd=0 state=1 disp=0 bright=255 sprNZ=114 dispNZ=50` (steady; sprNZ briefly 110/dispNZ 42 on one frame = animation, still non-zero).
    - Also observed: sprParent=0x0 while &M5.Display=&M5.Lcd=0x3fca07b0 (re-confirms the static-init-order null _parent — exactly the bug the pushFrame(&M5.Display) fix bypasses; render now succeeds because the explicit-parent push ignores the null cached _parent).
  implication: DECISIVE. The render pipeline is fully healthy with both fixes:
    - nap=0, off=0 => render gate NOT skipped (IMU/screenOff hypothesis A ELIMINATED).
    - sprNZ=114 (non-zero) => buddyTick DOES draw visible species art into the sprite (blank-sprite hypothesis B ELIMINATED).
    - dispNZ=50 (non-zero) => those pixels reach the display GRAM (read back from panel) => buddy IS pushed to screen (display-blank/dim hypothesis C ELIMINATED).
    - bright=255 => not dimmed.
  The original "post-splash buddy doesn't appear" symptom is resolved by the two landed fixes (CONFIG_SPIRAM=n + pushFrame->&M5.Display). Production firmware (CORE_DEBUG_LEVEL=0, no breadcrumbs) shares the identical src/ + sdkconfig, so it renders identically. Raw capture: tool-results/bvxn84dcn.txt.

## Eliminated

- hypothesis: "post-splash buddy is blanked because the loop render branch is skipped (StickS3 IMU misread -> napping) OR buddyTick draws nothing OR display is dimmed"
  evidence: session-4 diag telemetry: nap=0, off=0, buddyMode=1, bright=255, sprNZ=114 (art in sprite), dispNZ=50 (art on panel GRAM). All three sub-hypotheses contradicted by direct runtime observation.
  timestamp: 2026-06-29 (session 4)

- hypothesis: "the splash pushSprite crash is a null/uninitialized LCD panel (M5GFX didn't detect StickS3 display)"
  evidence: probe shows M5.Display w=135 h=240 panel=0x3fced104 (valid), autodetect logged board_M5StickS3 + created Panel_ST7789. Display is fully initialized. The null was the SPRITE's _parent (0x0), not the panel.
  timestamp: 2026-06-29

- hypothesis: "qio_opi (octal PSRAM) was the SOLE cause of the bootloop"
  evidence: After fixing memory_type qio_opi -> qio_qspi (correct for the AP_3v3 quad PSRAM), rebuilding + flashing (hash OK), the board STILL bootloops black — but the user reports the loop is now "a lot faster". Faster loop + port behaviour (below) = boot now gets PAST the PSRAM stage and hits a new, earlier-firing crash. So qio_opi was A real misconfiguration (KEEP the qio_qspi fix — eFuse proves quad/3.3V) but NOT the whole story.
  timestamp: 2026-06-29 (session 2, post-flash verify)

## More Evidence

- timestamp: 2026-06-29 (verify of qio_qspi fix — FAILED)
  checked: user power-cycled; reported "bootlooping blackscreen again still, seems to be a lot faster than before". Then I polled `pio device list` 14x @0.5s.
  found: 303A:832B (running-app OTG CDC) NEVER appears. 303A:1001 (USB-Serial/JTAG) is MOSTLY ABSENT, blipping in only occasionally.
  implication: With ARDUINO_USB_MODE=1 the app, very early, switches the physical USB lines from USB-Serial/JTAG to the OTG (TinyUSB) controller, then crashes BEFORE TinyUSB enumerates as 832B -> port disappears -> reset -> ROM re-shows 1001 briefly -> repeat. Crash is before the app's USB-OTG enumeration completes (i.e. in/around M5.begin or earlier). ALSO explains the total blackout on logs: the IDF panic handler writes to the USB-Serial/JTAG peripheral, but ARDUINO_USB_MODE=1 has handed the USB pins to OTG, so that output is never physically connected. The earlier "panic goes to UART0" assumption is incomplete — it's really "panic goes to USB-Serial/JTAG peripheral, which OTG mode disconnects from the pins."

## Current diagnostic plan

- hypothesis: a new early-boot panic (post-PSRAM) — prime suspect M5.begin(cfg) speaker init (internal_spk=true, D-09) or IMU/display init under M5Unified runtime board autodetect against board=esp32-s3-devkitc-1.
- test: build a DIAGNOSTIC firmware identical to sticks3 EXCEPT ARDUINO_USB_MODE=0 (HWCDC / USB-Serial/JTAG console). Then the USB-Serial/JTAG (1001) stays physically connected across the bootloop and the IDF panic backtrace prints there -> capturable with no power-cycle. Env: [env:sticks3-diag].
- expecting: a Guru Meditation / abort() / watchdog / brownout message + backtrace on COM8, pinpointing the crash site.
- next_action: DONE — diag env captured the bootloop. See evidence below. Next: breadcrumb-log setup() to pinpoint the crash.

## Diag capture evidence (the breakthrough)

- timestamp: 2026-06-29 (diag firmware, HWCDC console)
  checked: built [env:sticks3-diag] (qio_qspi + ARDUINO_USB_MODE=0 + CORE_DEBUG_LEVEL=5), flashed, captured COM8. The USB-Serial/JTAG stayed connected across the bootloop and printed everything.
  found: Deterministic per-loop sequence (identical every reboot):
    1. `E (208) quad_psram: PSRAM chip is not connected, or wrong PSRAM line mode` — PSRAM init FAILS even in QUAD mode. Boot CONTINUES past it (non-fatal / IGNORE_NOTFOUND).
    2. `E esp_littlefs: Corrupted dir pair at {0x0, 0x1}` / `mount failed (-84)` / `Failed to initialize LittleFS`.
    3. `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)`, EXCVADDR=0x00000074 (near-null deref), PC=0x42026266. rst:0xc (RTC_SW_CPU_RST) -> reboot loop.
  implication: The reset cause is the LoadProhibited crash. PSRAM-fail and LittleFS-fail are logged but boot proceeds past both, so the crash is downstream. EXCVADDR 0x74 = reading offset 0x74 off a NULL/garbage base pointer.

- timestamp: 2026-06-29 (eFuse + version facts)
  checked: espefuse summary; pio pkg versions
  found: PSRAM_CAP=8M, PSRAM_VENDOR=AP_3v3 (AP Memory 3.3V => QUAD family), FLASH_TYPE=4 data lines (quad), all SPI_PAD_CONFIG_*=0 (default in-package PICO pins). Platform espressif32@55.3.39, arduino-esp32 core 3.3.9 (IDF 5.5). So qio_qspi is the CORRECT mode and the toolchain is new enough for AP_3v3 — yet quad_psram still can't enumerate the chip (likely a PSRAM clock/calibration setting baked into the precompiled IDF libs; not reconfigurable without rebuilding IDF).
  implication: Making PSRAM work is likely blocked by precompiled-libs. Pragmatic in-scope fix = make firmware boot/run WITHOUT PSRAM (app uses only ~66KB of 327KB internal SRAM). Must eliminate the downstream null-deref crash.

- timestamp: 2026-06-29 (backtrace decode — UNRELIABLE)
  checked: xtensa-esp32s3-elf-addr2line on PC + backtrace vs diag ELF
  found: PC->cactus::doAttention (cactus.cpp:118, a RENDER fn), then frames into ArduinoJson parseNumber/parseQuotedString, then spinlock. NOT a coherent call chain.
  implication: Stack/memory is corrupted -> backtrace walker following garbage. Cannot trust addr2line. Need breadcrumb logging in setup() to localize deterministically.

- timestamp: 2026-06-29 (source scan)
  checked: grep src for ps_malloc/MALLOC_CAP_SPIRAM/psramFound/createSprite
  found: NO explicit PSRAM API use in app. Only spr.createSprite(135,240)=63KB at main.cpp:1236, and characterInit() (character.h: "Mounts LittleFS, reads...") at main.cpp:1238. LittleFS partition (0x650000) was never written (only firmware.factory.bin @0x0 flashed) -> garbage -> mount fail is expected, NOT a hardware fault.
  implication: Crash is most likely in setup()'s display/sprite path or characterInit() reacting to the failed LittleFS mount and/or a null sprite buffer. Breadcrumbs will settle which.

## CONFIRMED ROOT CAUSE (breadcrumb + decoded backtrace)

- timestamp: 2026-06-29 (breadcrumb diag build)
  checked: added -DDIAG_BREADCRUMBS BC() prints across setup(); built/flashed/captured.
  found: NO breadcrumb printed at all (not even "setup-enter"). Crash now Core 0, EXCVADDR=0x00000004, at ~232ms right after the quad_psram error — i.e. BEFORE setup() runs. Decoded backtrace (reliable this time):
    0x42038b06 psramInit (cores/esp32/esp32-hal-psram.c:73 -> the esp_psram_init() call)
    0x42037de7 __esp_system_init_fn_init_psram_new (cores/esp32/esp32-hal-misc.c:301)
    0x42044123 do_system_init_fn (esp_system/startup.c:132)
    0x4204415d do_core_init / start_cpu0_default
  Also note: between the two diag builds the crash site MOVED (Core1/0x74 -> Core0/0x04). A crash whose address shifts with binary layout = memory corruption, not a fixed logic bug.
  implication: ROOT CAUSE = arduino-esp32's core PSRAM init. esp32-hal-misc.c registers `ESP_SYSTEM_INIT_FN(init_psram_new ...) { psramInit(); }` guarded by `#if CONFIG_SPIRAM`. This runs as a CORE system-init hook BEFORE app_main/setup(). On this board the AP_3v3 8MB QUAD PSRAM does not enumerate with the prebuilt IDF quad driver/config (`quad_psram: chip not connected or wrong line mode`), and the failing probe path corrupts memory / faults in the systimer/esp_timer call inside esp_psram_init -> reset loop before any app code. The earlier (PSRAM-on) downstream crashes (LittleFS path, cactus PC) were all the same corruption manifesting at layout-dependent addresses.

- timestamp: 2026-06-29 (framework constraint)
  checked: arduino-esp32 prebuilt libs (esp32s3 variants) + pioarduino builder
  found: ALL S3 memory variants (dio_opi, dio_qspi, opi_opi, qio_opi, qio_qspi) ship CONFIG_SPIRAM=y; top sdkconfig has CONFIG_SPIRAM_MODE_QUAD=y, CONFIG_SPIRAM_CLK_IO=30, CONFIG_SPIRAM_CS_IO=26. So `memory_type` alone CANNOT turn PSRAM off. BUT pioarduino (espressif32@55.3.39, arduino 3.3.9 / IDF 5.5.4) supports a `custom_sdkconfig` platformio.ini option (builder/frameworks/arduino.py:280) that triggers a framework RECOMPILE with overridden config — this CAN set CONFIG_SPIRAM=n.
  implication: To stop the crash we must compile arduino core WITHOUT CONFIG_SPIRAM (removes the init_psram_new hook). Mechanism = custom_sdkconfig (a heavier, first-time-slow IDF recompile).

## Resolution

root_cause: TWO independent root causes, both required for the bootloop.
  (1) PSRAM init hook: arduino-esp32 registers a pre-setup() system-init hook (ESP_SYSTEM_INIT_FN init_psram_new -> psramInit -> esp_psram_init) whenever CONFIG_SPIRAM is enabled. The ESP32-S3-PICO-1 has 8MB AP_3v3 (3.3V QUAD) PSRAM that does NOT enumerate with the prebuilt IDF quad-PSRAM driver; the failing probe corrupts memory and panics (LoadProhibited) before app code runs.
  (2) Sprite parent SIOF (exposed only once #1 was fixed and boot reached render): `TFT_eSprite spr = TFT_eSprite(&M5.Lcd);` (main.cpp:11, global) captures &M5.Lcd at static-init time, BEFORE the M5Unified library global `M5` binds its `M5GFX& Lcd = Display` reference -> stored _parent is NULL. The splash spr.pushSprite(0,0) -> push_sprite(null,...) -> null->getColorDepth() reads null+0x74 -> LoadProhibited. (Static-init-order fiasco; never hit before because the board never ran far enough on HW.)
fix (Option A, applied):
  (1) platformio.ini [env:m5stack-sticks3]: custom_sdkconfig = CONFIG_SPIRAM=n (removes the init_psram_new hook via a from-source framework recompile); dropped now-contradictory -DBOARD_HAS_PSRAM. Kept memory_type=qio_qspi.
  (2) src/main.cpp pushFrame(): push to &M5.Display explicitly (spr.pushSprite(&M5.Display,0,0) / spr.pushRotated(&M5.Display,180); M5.Lcd->M5.Display for the pivot/rotation), bypassing the null static-init _parent. Only two push call sites, both here.
verification:
  - SELF-VERIFIED (diag, HWCDC console): with both fixes, boot runs the FULL breadcrumb sequence setup-enter -> after pushFrame(splash) -> setup-done -> loop-first-iter, prints "buddy: ASCII mode", ZERO crash markers. PSRAM crash gone ("Embedded PSRAM: No"); splash render no longer faults.
  - SELF-VERIFIED (production firmware, CONFIG_SPIRAM=n + USB_MODE=1, CORE_DEBUG_LEVEL=0): flashed; HWCDC capture shows a single clean boot banner, the expected non-fatal LittleFS-not-formatted notice, and NO Guru/panic/Rebooting and NO repeating reset loop. (Broken state previously showed a panic+reboot every cycle.)
  - PENDING HUMAN: physical power-cycle + confirm the screen shows the buddy UI (first boot, no owner => "Hello!" / "a buddy appears" splash, then an ASCII buddy), not black.
files_changed:
  - platformio.ini: memory_type qio_opi -> qio_qspi (earlier); custom_sdkconfig = CONFIG_SPIRAM=n + dropped -DBOARD_HAS_PSRAM (this fix); temporary [env:sticks3-diag] (to be removed)
  - src/main.cpp: pushFrame() pushes to &M5.Display explicitly (permanent fix); guarded -DDIAG_BREADCRUMBS instrumentation + display/parent probe (to be removed)

## Decision / next focus

- DECISION RECEIVED: Option A — disable PSRAM via custom_sdkconfig CONFIG_SPIRAM=n. Scope strictly to the StickS3 env; do NOT touch m5stickc-plus or shared src/.
- Applying Option A REVEALED a second latent bug (sprite-parent SIOF) that also had to be fixed for the board to boot. The pushFrame fix lives in shared src/ but cannot regress StickC Plus: it only replaces a null/UB cached parent with the live primary display, which is correct on both boards.
- next_action: production firmware flashed + self-verified (no panic). Issue human-action checkpoint: power-cycle, report screen. On "confirmed": remove [env:sticks3-diag] + DIAG_BREADCRUMBS scaffolding (keep only the two real fixes), commit atomically, archive + knowledge-base, resolve.

reasoning_checkpoint (FIX):
  hypothesis: "The pre-setup() ESP_SYSTEM_INIT_FN init_psram_new -> psramInit() crash only fires when CONFIG_SPIRAM is compiled in. Setting CONFIG_SPIRAM=n (via pioarduino custom_sdkconfig, framework recompile) removes the init hook entirely, so the failing quad-PSRAM probe never runs and early boot proceeds to app_main/setup()."
  confirming_evidence:
    - "Decoded reliable backtrace: 0x42038b06 psramInit (esp32-hal-psram.c:73) <- __esp_system_init_fn_init_psram_new (esp32-hal-misc.c:301) <- do_system_init_fn (startup.c:132). Crash is in the CONFIG_SPIRAM-gated init hook, before setup()."
    - "esp32-hal-misc.c registers init_psram_new under #if CONFIG_SPIRAM. CONFIG_SPIRAM=n => the hook is not compiled => psramInit() never called."
    - "Source scan found NO explicit PSRAM API use (no ps_malloc/MALLOC_CAP_SPIRAM/psramFound). App uses ~66KB of 327KB internal SRAM. M5StickC Plus (classic ESP32, ZERO PSRAM) runs the same app incl. GIF pets -> app is provably PSRAM-free."
    - "pioarduino (arduino 3.3.9 / IDF 5.5) supports custom_sdkconfig (builder/frameworks/arduino.py:280) to override prebuilt CONFIG_SPIRAM=y and trigger a framework recompile."
  falsification_test: "Build diag env (HWCDC console) with CONFIG_SPIRAM=n + breadcrumbs, flash, capture COM8. If the 'quad_psram: chip not connected' line and the pre-setup psramInit panic are GONE and breadcrumbs reach 'setup-done', the hypothesis holds. If it still crashes pre-setup, CONFIG_SPIRAM=n did not take / a different hook fires -> wrong."
  fix_rationale: "Removes the root-cause init hook at compile time rather than papering over the symptom. PSRAM is genuinely unused by the app, so disabling it is correct, not a degradation."
  blind_spots: "createSprite(135x240=63KB) and any future GIF buffers now allocate from internal SRAM; need to confirm no OOM at runtime. custom_sdkconfig triggers a one-time slow IDF/framework recompile that must succeed cleanly."

- next_action: rebuild diag env (custom_sdkconfig forces a from-source IDF build), flash, capture COM8 for setup-done breadcrumb.

## Fix-build evidence

- timestamp: 2026-06-29 (applying Option A)
  checked: added `custom_sdkconfig = CONFIG_SPIRAM=n` to [env:m5stack-sticks3] + [env:sticks3-diag]; dropped -DBOARD_HAS_PSRAM from production (contradictory with SPIRAM=n). First diag build.
  found: (a) custom_sdkconfig switches pioarduino from prebuilt-libs to a FULL from-source ESP-IDF build (pulls managed components incl. esp-sr). First attempt failed with WinError 32 unpacking esp-sr cache — a stale cmake.exe (PID 22748) held the file; killed it, retry got past it. (b) Second attempt failed compiling pioarduino's `.dummy/sketch.cpp`: `'USBSerial' was not declared`. Root: diag had ARDUINO_USB_MODE=0 (Native USB); USBSerial (USBCDC.cpp:478) is gated on `CONFIG_TINYUSB_CDC_ENABLED`, which the regenerated from-source sdkconfig does not enable.
  implication: GROUND TRUTH from cores/esp32/HardwareSerial.h:439-446 — ARDUINO_USB_MODE=1 => Serial=HWCDCSerial (USB-Serial/JTAG); =0 => Serial=USBSerial (Native/TinyUSB). The earlier debug notes (and platformio.ini diag comment) had these SWAPPED. Fix: set diag to ARDUINO_USB_MODE=1 (HWCDC) — needs no TinyUSB so it compiles under from-source IDF, mirrors production's USB mode, and HWCDC/USB-Serial-JTAG (303A:1001) stays connected across boot once the pre-setup crash is gone, carrying breadcrumbs + any panic backtrace.
