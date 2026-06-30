# Roadmap: Claude Desktop Buddy — StickS3 + Chimes Migration

## Milestones

- ✅ **v1.0 StickS3 + Chimes** — Phases 1–4 (shipped 2026-06-30) — full details: [milestones/v1.0-ROADMAP.md](milestones/v1.0-ROADMAP.md)

## Phases

<details>
<summary>✅ v1.0 StickS3 + Chimes (Phases 1–4) — SHIPPED 2026-06-30</summary>

- [x] Phase 1: Build System (1/1 plans) — completed 2026-06-28
- [x] Phase 2: Compatibility Shim (1/1 plans) — completed 2026-06-28
- [x] Phase 3: API Port (5/5 plans) — completed 2026-06-29
- [x] Phase 4: Haptics → Chimes (2/2 plans) — completed 2026-06-30

One codebase, two boards: the StickC Plus firmware was ported to M5Unified + M5GFX behind a
`compat.h` shim and made to build + run on the M5StickS3 (ESP32-S3), with vibration replaced by
ES8311 chiptune chimes on the StickS3 while the StickC Plus LEDC motor path stayed intact.

Full phase details, decisions, and verification: [milestones/v1.0-ROADMAP.md](milestones/v1.0-ROADMAP.md)

</details>

### 🚧 v1.1 (Planned)

No phases planned yet — start with `/gsd-new-milestone`. Carried-forward follow-ups
(captured in `.planning/todos/pending/`):

- Firm up StickS3 connected-idle power saving (APB-safe ~80MHz throttle / light-sleep)
- Restore encrypted BLE bond (came back unencrypted after a re-pair)
- Format StickS3 LittleFS (`fsTotal=0`; enables GIF character packs)
- Visually confirm the StickS3 clock auto-rotation fix on USB

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Build System | v1.0 | 1/1 | Complete | 2026-06-28 |
| 2. Compatibility Shim | v1.0 | 1/1 | Complete | 2026-06-28 |
| 3. API Port | v1.0 | 5/5 | Complete | 2026-06-29 |
| 4. Haptics → Chimes | v1.0 | 2/2 | Complete | 2026-06-30 |
