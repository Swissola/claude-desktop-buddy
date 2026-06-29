---
phase: 03-api-port
plan: "04"
subsystem: buddy-render
tags: [include-swap, forward-decl, compat, port]
dependency_graph:
  requires: ["03-01"]
  provides: ["PORT-02-partial"]
  affects: ["03-05"]
tech_stack:
  added: []
  patterns: ["compat.h transitive include via buddy.h/character.h"]
key_files:
  created: []
  modified:
    - src/buddy.h
    - src/character.h
    - src/buddy.cpp
    - src/character.cpp
    - src/buddies/axolotl.cpp
    - src/buddies/blob.cpp
    - src/buddies/cactus.cpp
    - src/buddies/capybara.cpp
    - src/buddies/cat.cpp
    - src/buddies/chonk.cpp
    - src/buddies/doge.cpp
    - src/buddies/dragon.cpp
    - src/buddies/duck.cpp
    - src/buddies/ghost.cpp
    - src/buddies/goose.cpp
    - src/buddies/llama.cpp
    - src/buddies/mushroom.cpp
    - src/buddies/octopus.cpp
    - src/buddies/owl.cpp
    - src/buddies/penguin.cpp
    - src/buddies/rabbit.cpp
    - src/buddies/robot.cpp
    - src/buddies/snail.cpp
    - src/buddies/turtle.cpp
decisions:
  - "Remove `class TFT_eSPI;` forward decls from buddy.h + character.h and replace with `#include \"compat.h\"` (Pitfall 1 fix via PR #48 technique)"
  - "buddies/*.cpp use `../compat.h` (relative path one directory up); buddy/character use `compat.h` (peer file)"
metrics:
  duration_minutes: 12
  completed_date: "2026-06-29"
  tasks_completed: 2
  files_modified: 24
---

# Phase 03 Plan 04: TFT_eSPI Forward-Decl Fix + Bulk Include Swap Summary

**One-liner:** Removed conflicting `class TFT_eSPI;` forward decls from buddy.h/character.h and swapped `#include <M5StickCPlus.h>` to `compat.h` across all 24 buddy/character/species files (PORT-02).

## Objective

Fix the `class TFT_eSPI;` forward-declaration conflicts (Pitfall 1 from RESEARCH) and swap the remaining `#include <M5StickCPlus.h>` sites across buddy.{h,cpp}, character.{h,cpp}, and all 20 buddies/*.cpp files. With `using TFT_eSPI = lgfx::LGFXBase;` now defined in compat.h, the legacy forward declarations were conflicting redeclarations causing hard compile errors.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Remove TFT_eSPI forward decls + swap headers/cpp | 44dd7a2 | src/buddy.h, src/character.h, src/buddy.cpp, src/character.cpp |
| 2 | Swap include in all 20 buddies/*.cpp | bf68b4c | src/buddies/*.cpp (20 files) |

## What Changed

### Task 1 — buddy.h / character.h / buddy.cpp / character.cpp

**buddy.h:**
- Deleted `class TFT_eSPI;` forward declaration (line 11 in original)
- Added `#include "compat.h"` after `#include <stdint.h>`
- `TFT_eSPI` is now the real `lgfx::LGFXBase` alias via compat.h; `buddyRenderTo(TFT_eSPI* tgt, ...)` resolves cleanly

**character.h:**
- Deleted `class TFT_eSPI;` forward declaration (line 27 in original)
- Added `#include "compat.h"` after `#include <stdint.h>`
- Same resolution as buddy.h

**buddy.cpp / character.cpp:**
- Swapped `#include <M5StickCPlus.h>` → `#include "compat.h"` (no other changes)
- These files only use the sprite/drawing API (TFT_eSprite/TFT_eSPI), which is unchanged (RF-05 confirmed)

### Task 2 — All 20 buddies/*.cpp

Mechanical include swap in each species file:
- `#include <M5StickCPlus.h>` → `#include "../compat.h"` (relative path, since buddies/ is one level down)
- No other changes — these files exclusively use `extern TFT_eSprite spr;` + buddyPrintSprite/buddySetColor/buddySetCursor/buddyPrint helpers
- None reference M5.Axp/M5.Rtc/M5.Beep/power names (verified in RESEARCH)

## Verification Results

All acceptance criteria passed (grep-based):

```
class TFT_eSPI in buddy.h:    0 matches (PASS)
class TFT_eSPI in character.h: 0 matches (PASS)
compat.h in buddy.h:           1 match   (PASS)
compat.h in character.h:       1 match   (PASS)
#include M5StickCPlus.h in src/: 0 files  (PASS)
../compat.h missing from buddies: 0 files  (PASS)
```

Note: `grep -rl 'M5StickCPlus.h' src/` reports 1 hit — `src/compat.h` at line 153 in a comment (`// --- Button GPIOs (were M5StickCPlus.h macros...`). This is not an `#include` directive; the actual include-level check (`grep -rl '#include.*M5StickCPlus\.h' src/`) returns zero files.

## Deviations from Plan

None — plan executed exactly as written. The two tasks were fully mechanical:
- Pitfall 1 fix (forward decl removal + compat.h include) in 4 files
- Bulk include swap in 20 buddy files

## Known Stubs

None. This plan contains no data stubs or placeholders — it is a pure include/type-alias change.

## Threat Flags

None. These are rendering-only files with no network/auth/file-system/trust-boundary surface (confirmed in plan threat model: T-03-04-T1 accepted, no new surface introduced).

## Self-Check: PASSED

- src/buddy.h: exists, contains `#include "compat.h"`, no `class TFT_eSPI;`
- src/character.h: exists, contains `#include "compat.h"`, no `class TFT_eSPI;`
- src/buddy.cpp: exists, contains `#include "compat.h"`, no `M5StickCPlus.h`
- src/character.cpp: exists, contains `#include "compat.h"`, no `M5StickCPlus.h`
- All 20 src/buddies/*.cpp: contain `#include "../compat.h"`, no `M5StickCPlus.h`
- Commits 44dd7a2 and bf68b4c confirmed in git log
- No `#include <M5StickCPlus.h>` anywhere under src/
