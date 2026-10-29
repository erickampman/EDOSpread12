# EDOSpread12 — plan

A MIDI-in → MIDI-out note effect that **plays an N-EDO from an ordinary 12-key keyboard**. Each of
the 12 keys in an octave is assigned one step of the EDO's octave, and the table repeats every
octave. Retune moves every note by the same interval; EDOSpread12 gives every key its own place.
It is CLAP-first, with the same layout as Retune and NoteGran: a hand-written raw CLAP entry point
over a pure C++ core, and the vendored free-audio **clap-wrapper** for the other formats.

EDOSpread12 makes no sound of its own. It changes the notes going to the synth after it.

**Status (2026-10-29):** phases 1 and 2 done: `SpreadCore` (src/SpreadCore.h) over uw-core's
`NoteRemap` and `ClapNoteRemap`, and the raw CLAP plugin (src/plugin.cpp: clap-validator clean,
headless host test). Heard working in StageHand and Bitwig (2026-10-29). Next: the editor (phase 4).

## Decisions so far

| Question | Answer |
|---|---|
| The map (2026-10-29) | **12 entries**, one per key of the octave, each a step of the N-EDO octave. It repeats every octave: C4 and C5 land on the same step, an octave apart |
| Where the octave starts (2026-10-29) | At **`Origin`**, a pitch class (default **A**), as KeyboardEDO's origin and NoteGran's. The Origin key plays at its 12-TET pitch as step 0; the octave breaks there, not at C |
| Order (2026-10-29) | **Free.** Keys may be out of order (E above F) or share a step. Needed anyway, since N may be below 12. The editor marks it; the plugin doesn't stop it |
| EDO range (2026-10-29) | **1 … 96**, as Retune |
| Filling the table (2026-10-29) | A choice: **Floor** or **Nearest** (below; Even spread dropped 2026-10-29, as it is 12-EDO spacing again). Editing a key by hand afterwards is the normal case |
| Output (2026-10-29) | **As NoteGran Chords:** the output dialect follows the input (MIDI 2.0 Pitch 7.9, CLAP note + tuning expression), except that a MIDI 1.0 note that lands off a 12-TET key goes out as **MIDI 2.0**, and the host translates it down for a synth that takes only MIDI 1.0 |
| Editor (2026-10-29) | A step circle, as Chords' degree circle, for now. It must show plainly where each step sits against the 12-TET keys (below) |
| Form | CLAP note effect, then VST3 and AUv2 via clap-wrapper (MIDI 1.0, rounded), as Retune |
| Deliverables | README.md, BUILDING.md, THIRD_PARTY_NOTICES.md, docs/design.md, MILESTONES.md, as the other plugins |

## The mapping

With origin pitch class `o` (0 = C … 9 = A), EDO `N`, and the table `step[0..11]` indexed from the
origin (index 0 is the Origin key itself):

```
d      = (key − o) mod 12            which key of the octave, counted from the origin
oct    = floor((key − o) / 12)       which octave
pitch  = o + 12 × oct + 12 × step[d] / N     in semitones, as 7.9 (× 512, rounded)
```

So with A origin in 19-EDO, A4 (69) is step 0 and stays at 69.0; a key assigned step 5 in that
octave sounds at 69 + 60/19 = 72.16, 316¢ above A.

- **A step is 0 … N** (decided 2026-10-29). 0 … N−1 are the octave's own steps; **N** is
  the next origin, an octave up. Without it, Nearest couldn't put B on the octave above in small EDOs
  (in 5-EDO, B is 100¢ from the next A and 140¢ from step 4), and you couldn't either.
- **Out of range** (the result below 0 or at or above 128): drop the Note On and its matching Note Off
  and per-note messages, as Retune.

## Filling the table

`Fill`: **Floor** | **Nearest**. With key `d` of the octave at `d / 12` of an
octave in 12-TET:

- **Floor:** `step[d] = floor(d × N / 12)`, the highest step at or below the key's 12-TET pitch. Never
  sharp. Never reaches N.
- **Nearest:** `step[d] = round(d × N / 12)`, the step nearest the key's 12-TET pitch. Closest to the
  familiar sound. A tie (in 24-EDO there are none; in 18-EDO the odd keys) goes up. May reach N.
- **Even spread was dropped** (2026-10-29). Spacing the 12 keys evenly over the octave is 12-EDO,
  which Floor and Nearest already start from; as a formula it came out the same as Floor.

**When the table is filled** (decided 2026-10-29): choosing a Fill or changing `EDO` **in the
editor** refills all 12 keys, as one undoable gesture (the editor sets the 12 parameters, as a
preset would). A change to `EDO` from **host automation** doesn't refill: the stored steps are
clamped to 0 … N, as Retune clamps Steps, and the stored values aren't rewritten (a plugin-initiated
change would fight the automation). Fill is then a reminder of how the table was made, not a live
rule.

## Behaviour

- **Parameters** (appended, never inserted, as in the other plugins):
  - 0 `EDO`: N, integer 1 … 96, default 12.
  - 1 `Origin`: pitch class C … B, default A.
  - 2 `Fill`: Floor | Nearest, default Nearest.
  - 3 … 14 `Step 1` … `Step 12`: the table, from the origin up. Integer; CLAP ranges are fixed, so the
    host sees 0 … 96 and EDOSpread12 uses `clamp(step, 0, N)`. Names follow the origin (with A
    origin, `Step 1` is "Step A", `Step 4` "Step C"); changing Origin asks the host to rescan the
    parameter info (`CLAP_PARAM_RESCAN_INFO`, allowed while active). Indexed from the origin, not from C, so
    changing Origin moves the whole pattern, not each key's step (decided 2026-10-29).
  - Default state: 12-EDO, A origin, Nearest, so every key plays its own pitch: EDOSpread12 does
    nothing until you change it. (A MIDI 2.0 note does come out with a Pitch 7.9 attribute equal to its
    own key: uw-core's `transposeNote79` always writes one, as in Retune at Steps 0. Same pitch.)
- **Output dialect follows the input**, as Retune, with Chords' one exception for MIDI 1.0 (`ClapNotes`,
  the latch table and the 7.9 maths are Retune's, reused, see Code):
  - **MIDI 2.0:** Note On with attribute type 3, Pitch 7.9. The note number is the integer part, so a
    receiver that ignores the attribute plays the nearest key below.
  - **CLAP notes:** the nearest whole key, plus `CLAP_NOTE_EXPRESSION_TUNING` with the signed fraction
    (same note id, right after). A host's own tuning expression on the note gets the fraction added.
    Note Off, choke and the other note expressions (volume, pan, vibrato, brightness, pressure …)
    go to the latched key and note id, as Retune's M3. This is the path that keeps the spread in
    Bitwig, which sends plugins CLAP notes and no MIDI 2.0.
  - **MIDI 1.0:** a note that lands on a 12-TET key (always, in 12-EDO) stays MIDI 1.0. A note with a
    fraction is promoted (below) only where MIDI 2.0 can leave the plugin: the host's note dialects
    include MIDI2, and it isn't clap-wrapper (as built, 2026-10-29). Elsewhere it's rounded to the
    nearest key, as Retune; a host like Bitwig, which takes no MIDI 2.0, sends CLAP notes anyway.
    Promotion: a note with a
    fraction goes out as **MIDI 2.0** with Pitch 7.9 (decided 2026-10-29, as NoteGran Chords' question
    10): MIDI 1.0 can't carry it, and rounding would undo the spread. Its Note Off, poly pressure and
    other per-note messages follow it in MIDI 2.0, from the latch. Where this pays: hosts that pass
    MIDI 2.0 on to the synth (StageHand). A host that converts it down to MIDI 1.0 for the synth plays
    it as 12-EDO anyway (NoteGran found REAPER on Linux does, 2026-10-24); nothing is lost there
    against rounding.
  - **Under clap-wrapper** (VST3, AUv2) MIDI 1.0 stays rounded to the nearest key, as Retune's M4: the
    AUv2 wrapper's MIDI 2.0 → 1.0 conversion keeps only the key, which rounds down, and the VST3
    wrapper sends CLAP notes, which `ClapNotes` already rounds.
- **An incoming pitch moves with the key.** A MIDI 2.0 Note On that already carries Pitch 7.9 keeps
  its offset from its key: `out = mapped(key) + (pitch − key)`. A host's Pitch 7.25 or per-note pitch
  bend on a held note passes through, relative, as in Retune.
- **Latching, as Retune's:** each sounding note remembers its output key and 7.9 pitch, keyed by
  group/port + channel + input key (and CLAP note id). Note Off, poly pressure and every per-note
  message use the latched key. A table change while notes are held can't leave a stuck note; only new
  notes take it. Retrigger and All Notes Off / reset as Retune.
- **Two keys on one output key.** Keys that share a step (or round to one key under clap-wrapper) are two
  notes on one key. As Retune's retrigger rule: the second Note On first ends the first, and the first
  key's Note Off, latched to a note that's already gone, is dropped.
- **Everything else** (CCs, channel pitch bend, program change, SysEx, clock) passes through
  unchanged, at its sample offset.

## Editor

DPF for drawing only, with NoteGran's widget kit (`src/UI`: Knob, Dropdown, GroupBox, Logo) and one
accent colour. One window:

Laid out top to bottom (2026-10-29):

- **The step circle** is the chosen EDO: N steps round it, step 0 (the origin) at the top, as Chords'
  degree circle. Changing EDO redraws it.
- **The 12-TET reference is in the circle itself** (2026-10-29): the ring is cut into 12 arcs, one
  per piano key of the octave from the origin, and an arc is drawn **dark** where that key is black
  and **light** where it's white, like a piano octave bent round. So a step's place against the
  familiar keys shows at a glance: step 5 of 19 sits in C's light arc, step 2 in A#'s dark one.
  **Where the arcs break follows `Fill`** (2026-10-29), so a step lies in the arc of the key that fill
  would give it to:
  - *Nearest:* each arc is its key's 12-TET pitch ±50¢; a step is in the arc of the key it's nearest.
  - *Floor:* each arc runs from its key's 12-TET pitch up to the next key's; a step is in the arc of
    the key at or below it.

  Changing Fill moves the arcs at once, even before the Fill button is pressed, so you can see what a
  refill would do.
- **The piano octave**, laid out straight below the circle, one octave starting at the origin key.
  Click a key to select it.
- **The selected key's curve:** a line or Bézier curve from the selected key up to an **indicator**
  on the circle, at the step it's mapped to. Drag the indicator round the circle to reassign the key;
  it moves in whole steps (snaps to the nearest step as it's dragged) and sets the key's `Step`
  parameter as one gesture. Dragging past the top lands on step N, the next origin.
- **The other keys** (proposed): a small mark on the circle at each one's step, so the whole map is
  visible; keys out of order or sharing a step are tinted on the piano.
- **Read-out** for the selected key: its step and its offset in cents from the key's 12-TET pitch
  ("C → step 5 of 19, 316¢, +16¢").
- **Controls:** EDO knob, Origin dropdown, Fill dropdown and a Fill button (refills with the current
  choice).

## Code

- **uw-core** (submodule, UMP-only): `uw::ump` for Pitch 7.9 and the transposes, as Retune and
  NoteGran.
- **Retune's core, shared through uw-core** (decided 2026-10-29): the latch table, out-of-range drop,
  retrigger and `ClapNotes` are the same here; only "how far does this key move" differs (a shift in
  Retune, a table lookup here). Done 2026-10-29: uw-core's `NoteRemap` (uw::ump) and `ClapNoteRemap`
  (uw::clap-notes) take a shift per key, and NoteRemap does the MIDI 1.0 promotion (off for Retune).
  Retune uses them since b63211d. EDOSpread12's own code is the mapping alone (`SpreadCore`).
  NoteGran's own copies (its OwnedNotes and CLAP output) could follow later; not part of this plan.
- **clap-wrapper** and the VST3 SDK as submodules, as Retune's M4: `EDOSPREAD12_BUILD_VST3`,
  `EDOSPREAD12_BUILD_AUV2`, `install-clap`, `install-vst3`, `install-auv2`.
- **Identity:** CLAP id `com.unlikelyware.edospread12`, vendor Unlikelyware; AUv2 type `aumi`,
  subtype `ESpr` under `nLKL` (open question 8; `ESpr` unless you say otherwise). State header `EDOSpread12 1`.
- **Repo:** private, `erickampman/EDOSpread12`, as Retune.

## Phases

1. **Core library + tests.** The mapping (origin, octave, table lookup, 0 … N), the two fills, the
   latch and dialects from Retune. Table-driven tests: several N (1, 5, 12, 19, 31, 96), each fill,
   each origin, out-of-order and shared steps, held-note table change, MIDI 1.0 / 2.0 / CLAP.
2. **Raw CLAP plugin.** Note-effect feature, note ports in and out (MIDI | MIDI2 | CLAP, prefer
   MIDI2), 15 parameters, state, clap-validator clean, headless host test.
3. **Heard.** StageHand → EDOSpread12 → StruckSynth (plays Pitch 7.9); Bitwig with a synth that honours
   tuning expressions.
4. **Editor**, as above.
5. **VST3 and AUv2** via clap-wrapper, MIDI 1.0 (rounded), auval and Steinberg's validator.
6. **Docs:** README, BUILDING (the shared layout), THIRD_PARTY_NOTICES.

## Open questions

All settled 2026-10-29 except the AU code:

1. **Step N allowed: yes,** 0 … N; N is the origin an octave up.
2. **Even spread: dropped.** It was 12-EDO spacing, the same as Floor.
3. **Refill on EDO change: in the editor only.** Automation of EDO clamps the stored steps.
4. **Table from the origin,** not from C.
5. **MIDI 1.0 input in a non-12 EDO: as Chords,** a note with a fraction goes out as MIDI 2.0.
6. **The black and white arcs break where Fill puts them.**
7. **Retune's latch and dialect code move into uw-core** first, and both plugins use them.
8. **AU subtype code** `ESpr`, or another? Still open.
