# Phase 4: Haptics → Chimes - Context

**Gathered:** 2026-06-29
**Status:** Ready for planning

<domain>
## Phase Boundary

Make the buddy's haptic feedback **audible on the M5StickS3** via the ES8311
speaker (`M5.Speaker`), while leaving the **M5StickC Plus LEDC vibration-motor
path completely unchanged**. The existing event pattern engine
(`vibratePatternAmp` / `vibratePatternAmpArr` / `vibrateTick` in `src/main.cpp`)
gains a board-conditional StickS3 branch that emits **chiptune-style tones**
instead of driving the motor, so each of the five events
(approve / deny / attention / celebrate / connect) plays an audibly distinct,
recognisable chime. The settings UI is updated to reflect the change, and the
existing UI-beep (`settings().sound` → `compatBeep`) is reconciled with the new
event chimes so the two audio cues don't clash.

**In scope:**
- StickS3-only chime output: the pattern engine drives `M5.Speaker` tones under
  `#if defined(BOARD_STICKS3)` (HAPT-01).
- Exactly **one** chime voice this phase: the **Retro / chiptune** set (square-wave,
  pixel-buddy character). Five distinct event chimes; approve and deny made
  audibly different (they are identical motor blips today, distinguished only by
  the UI beep tone).
- StickC Plus motor path preserved byte-for-byte (HAPT-02) — verified by code-scope, not by a gating build/hardware test (see D-09; device repurposed).
- Settings relabel + UI-beep-vs-chime overlap resolution (HAPT-03).
- A user-configurable **speaker volume** settings entry on the StickS3 (D-08).
- The **StickS3 env builds green** with the board-conditional chime code (primary
  acceptance). The StickC Plus env is kept compiling but is no longer a gating
  ceremony (D-09).

**Out of scope (deferred / other phases):**
- The **selectable multi-theme chime picker** and the other three sound
  personalities (Warm & musical, Minimal & subtle, Bright & arcade-y) — recorded
  below as future work; NOT built this phase.
- BUZZ-01 (Grove Vibrator Unit) — stays v2-deferred; no configurability seam.
- StickC Plus speaker/motor LEDC-channel collision is N/A for the StickS3 (I2S/ES8311
  path); only touch StickC Plus motor code if a build fix demands it.

</domain>

<decisions>
## Implementation Decisions

### Chime identity (discussed)
- **D-01:** Chime output is **StickS3-only**. The StickC Plus keeps its vibration
  motor; no chime/theme UI appears on the StickC Plus.
- **D-02:** Ship **one** chime voice this phase — the **Retro / chiptune** set
  (square-wave tones, playful pixel-buddy/Tamagotchi energy). Chosen over warm,
  minimal, and bright as the starting voice.
- **D-03:** All five events must be **audibly distinct and recognisable**, and
  **approve must differ clearly from deny** (today they're the same motor blip).
  Intended shapes (planner/researcher to finalise exact pitches): approve =
  bright rising cue; deny = lower/descending cue; attention = soft-soft-accent
  rising alert; celebrate = upbeat multi-note flourish; connect = clean
  two-note "linked up". Keep them short and non-annoying for a desk companion.
- **D-04:** Write the chime engine **theme-ready** — structure the tone tables so
  additional voices can be added later without re-architecting — even though only
  the Retro set ships now.

### Added decisions (user, 2026-06-29 — post-research)
- **D-08 (volume IS a settings entry):** Speaker volume is **user-configurable in the
  settings menu** on the StickS3 (a level cycler — e.g. a few steps with mute at the
  bottom), NOT a hardcoded init default. StickS3-only (the StickC Plus motor has no
  speaker volume). Persisted with the other settings. Supersedes the research's
  "volume as a fixed init default" suggestion.
- **D-09 (StickS3 is primary; dual-env de-prioritized):** The StickS3 is now the
  primary target. The StickC Plus is kept **compiling** (chime code stays behind
  `#if defined(BOARD_STICKS3)`; the motor `#else` path is left untouched), but the
  **dual-env build is no longer a gating ceremony** — Phase 4 does NOT block on the
  StickC Plus env and there is NO StickC Plus hardware test (device repurposed).
  HAPT-02 is satisfied by **code-scope** (the motor branch is unchanged) rather than
  by an enforced dual-env gate. Dropping StickC Plus entirely was considered and
  declined for now (reversible). Phase verification centers on the StickS3.

### Claude's Discretion (user delegated — flagged for review, overridable)
- **D-05 (pattern reuse):** Reuse each event's **existing rhythm/shape** from the
  current vibration patterns (approve/deny blip, attention low-low-HIGH, celebrate
  HIGH-low-HIGH-low siren, connect two-pulse) as the structural skeleton, rendered
  as chiptune tones with distinct per-event pitches — i.e. preserve the event
  "rhythm" identity, author tonal character on top. Not a blind buzz-to-beep
  amplitude translation.
- **D-06 (beep ↔ chime overlap, HAPT-03):** Keep `settings().sound` (UI button
  beep) and the event-chime feedback as **separate toggles**, but give **event
  chimes precedence** — when an event chime plays, suppress the simultaneous UI
  beep so the two cues don't stack/clash. UI beep still fires for plain button
  feedback when no event chime is active.
- **D-07 (settings label, HAPT-03):** **Board-conditional label** — the feedback
  on/off entry reads **"chime"** on the StickS3 (it chimes) and stays **"vibrate"**
  on the StickC Plus (it really vibrates). No theme-picker settings entry this
  phase (single voice).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` §"Phase 4: Haptics → Chimes" — goal + 4 success criteria.
- `.planning/REQUIREMENTS.md` §"Haptics → Chimes" — HAPT-01, HAPT-02, HAPT-03 (and
  §"Optional Physical Haptics" BUZZ-01 = deferred v2).

### Prior-phase decisions carried in
- `.planning/phases/03-api-port/03-CONTEXT.md` — D-09 (speaker enabled at `M5.begin`),
  D-10 (`M5.Beep`→`M5.Speaker` shim for the UI beep), and the "event chimes / motor
  collision are Phase 4" deferrals.
- `.planning/PROJECT.md` — Key Decisions (haptics→chimes via ES8311; keep StickC Plus
  LEDC motor board-conditional; emotion system dropped).

### Code to modify (this repo)
- `src/main.cpp` — the pattern engine: `VIBRATE_PIN`/`VIBRATE_CH` (lines ~31-34),
  the event pattern + amplitude arrays (~37-67), `vibratePatternAmp` /
  `vibratePatternAmpArr` (gated on `settings().vibrate`, ~74-95), `vibrateTick`
  (`ledcWrite(VIBRATE_PIN, amp)`, ~97-112); the UI beep `if (settings().sound)
  compatBeep(freq, dur)` (~289); `settingsItems[]` (~319) + the settings toggle
  handlers (~336+) for the relabel.
- `src/compat.h` — `compatBeep` (M5.Speaker tone shim from Phase 3); the place a
  `compatChime`-style helper would live if one is added.

No external specs beyond the planning docs above.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Pattern engine** (`vibratePatternAmp`, `vibratePatternAmpArr`, `vibrateTick`):
  already models events as on/off timing patterns + per-step amplitude arrays and
  a non-blocking tick player. The StickS3 branch replaces the `ledcWrite` output
  with `M5.Speaker.tone(...)` while reusing this timing/sequencing structure.
- **`compatBeep`** (compat.h, from D-10): already wraps `M5.Speaker.tone(freq, dur)`
  board-safely — the chime path can build on the same speaker API.
- **`settings().vibrate` / `settings().sound`**: existing toggles + persisted
  settings struct; relabel + the precedence rule (D-06/D-07) hook in here.

### Established Patterns
- Board-conditional compilation via `#if defined(BOARD_STICKS3)` (Phase 1 build flag)
  — the established way to fork StickS3 vs StickC Plus behavior from one tree.
- Speaker is enabled at `M5.begin` (D-09) and stays idle until used — chimes only
  need `tone()` calls, no init work.

### Integration Points
- The five events call `vibratePatternAmp*` from their state/transition handlers;
  those call sites stay the same — only the engine's output backend changes per board.
- The UI-beep path (`settings().sound` → `compatBeep`) and the event-chime path must
  coordinate (D-06 precedence) — both now drive the same speaker on the StickS3.

</code_context>

<specifics>
## Specific Ideas

- **Retro / chiptune voice**: square-wave 8-bit beeps with character, matching the
  pixel-buddy aesthetic (think Tamagotchi/arcade), not polished sine tones.
- **approve vs deny must be unmistakable** — the single thing the motor couldn't do
  (identical blips); the speaker should make these obviously different.
- User's broader vision: a **selectable chime theme** with multiple sound
  personalities — captured as deferred, starting with this one voice.

</specifics>

<deferred>
## Deferred Ideas

- **Selectable chime-theme picker** (StickS3 settings entry to choose the voice) —
  user wants it eventually; deferred until there's more than one voice. Build the
  engine theme-ready (D-04) so it slots in cleanly.
- **Additional chime voices**: **Warm & musical**, **Minimal & subtle**,
  **Bright & arcade-y** — the other three personalities from discussion. Record and
  add after the Retro set ships. (User: "keep a record of the other suggestions.")
- **BUZZ-01** (Grove Vibrator Unit, Port.A G9) — v2-deferred; do NOT add a
  configurability seam for it in this phase.

</deferred>

---

*Phase: 4-haptics-chimes*
*Context gathered: 2026-06-29*
