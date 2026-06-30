---
created: 2026-06-30T08:27:51.829Z
title: Format StickS3 LittleFS (fsTotal=0)
area: firmware
files: []
---

## Problem

The bridge daemon reports `stick LittleFS appears unformatted (fsTotal=0). Run factory reset
on the stick to format it; push-character will reject until then.` The StickS3's LittleFS data
partition was never formatted (it's been running ASCII species — e.g. octopus — which are
compiled in, so the empty FS hasn't blocked normal operation). But GIF character packs
(`push-character`) require a formatted FS, so they can't be uploaded until this is fixed.

Note: this is benign for everything done so far — the bootloop debug already established that
the buddy art is compiled-in (SPECIES_TABLE), not read from LittleFS.

## Solution

- Run a **factory reset** on the stick (Settings → reset → factory reset) — this formats
  LittleFS. NOTE: a factory reset also wipes NVS, so the tuned defaults (octopus, 40% volume,
  60% brightness, 12hr) revert to their code defaults — which is fine since those ARE the new
  code defaults, but any user-changed settings would reset.
- Then confirm the bridge no longer reports `fsTotal=0` and `push-character` is accepted.
- Alternatively, investigate auto-formatting LittleFS on first mount in firmware if it's
  unformatted, so a factory reset isn't required to enable GIF packs.
