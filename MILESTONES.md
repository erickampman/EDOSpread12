# EDOSpread12 milestones

## Tests
- all: cmake -S . -B build -G Ninja && ninja -C build && ctest --test-dir build --output-on-failure
- validator: clap-validator validate build/EDOSpread12.clap (35 pass, 9 skipped: no presets or audio ports)
- install: ninja -C build install-clap (symlinked)
- preview: open -n "build/EDOSpread12 Preview.app" --args "$PWD/build/EDOSpread12.clap" [id=value ...] (the editor alone, macOS)

## M0: Plan
- [x] docs/design.md: 12-entry map from the origin, repeating by octave; EDO 1–96; free order; output as Retune (2026-10-29)
- [x] MIDI 1.0 in, fraction out: sent as MIDI 2.0, as NoteGran Chords (2026-10-29)
- [x] Fill: Floor | Nearest; Even spread dropped, as it is 12-EDO spacing (2026-10-29)
- [x] Open questions settled: steps 0 … N, refill in the editor only, table from the origin, shared code through uw-core (2026-10-29)
- [x] CLAP only for now: no VST3, AUv2 deferred (2026-10-29; docs/design.md, Form)
- [x] Private repo erickampman/EDOSpread12

## M1: Core library
- [x] uw-core: Retune's latch, out-of-range drop, retrigger and CLAP note dialect, now a shift per key, with MIDI 1.0 promotion: `NoteRemap` (uw::ump) and `ClapNoteRemap` (uw::clap-notes), tested there; pushed first (uw-core ab028fb, 2026-10-29)
- [x] uw-core: two keys on one output key: the second ends the first's note, whose Note Off is dropped (a33f7ce)
- [x] Retune moved its pin and uses them; its tests and clap-validator unchanged (Retune b63211d, 6b50850)
- [x] CMake skeleton, external/clap (1.2.10) and external/uw-core (UMP-only, plus uw::clap-notes) submodules, ctest
- [x] Mapping (`SpreadCore`): origin, octave, table lookup, steps 0 … N, Pitch 7.9 out; incoming pitch offset kept
- [x] Fills: Floor, Nearest (ties up)
- [x] Latch, retrigger, two keys on one output key, out-of-range drop, pass-through, All Notes Off / reset (uw-core's)
- [x] MIDI 1.0 (kept on a 12-TET key, else promoted to MIDI 2.0, per-note messages following), MIDI 2.0 (attribute type 3) and CLAP note (key + tuning expression) paths
- [x] Table-driven tests: N = 1, 5, 12, 19, 24, 31, 96; each fill and origin; out-of-order and shared steps; held-note table change; all three dialects (2026-10-29)

## M2: Raw CLAP plugin
- [x] clap_entry, note-effect feature, note ports in + out (MIDI | MIDI2 | CLAP, prefer MIDI2; MIDI under clap-wrapper)
- [x] 15 parameters (EDO, Origin, Fill, a step per key named for it from the origin: "Step A" …), state `EDOSpread12 1`, clap-validator clean (2026-10-29)
- [x] Origin renames the steps: request_callback, then `CLAP_PARAM_RESCAN_INFO` on the main thread
- [x] MIDI 1.0 promoted to MIDI 2.0 only where the host takes MIDI2 and not under clap-wrapper; rounded otherwise
- [x] Headless host test (test/ClapTests.cpp): MIDI 1.0, MIDI 2.0 and CLAP notes in, output events checked; params, names, state
- [x] Default state passes every note unchanged (a MIDI 2.0 note gains a Pitch 7.9 attribute equal to its key: the same pitch)

## M3: Heard
- [x] Heard in StageHand (2026-10-29)
- [ ] StageHand in 19-EDO and 31-EDO specifically: KeyboardEDO (12-EDO piano) → EDOSpread12 → StruckSynth
- [x] Heard in Bitwig (2026-10-29)
- [ ] An EDO below 12 (5-EDO): shared steps play without stuck notes

## M4: Editor
- [x] DPF for drawing only (external/dpf, NoteGran's pin), NoteGran's widget kit, a sea-green accent (2026-10-29)
- [x] Step circle of the chosen EDO, step 0 at the top; its ring in 12 arcs, dark for black keys and light for white, breaking where Fill puts them
- [x] Piano octave laid out straight below the circle, from the origin; click a key to select it
- [x] Curve from the selected key to its indicator on the circle; drag the indicator in whole steps to reassign
- [x] Marks for the other keys' steps (names stacked on a shared step); out-of-order and shared keys tinted on the piano
- [x] EDO, Origin, Fill, "Fill all keys"; the selected key's step and cents offset, and what's odd about it
- [x] Refill in the editor as one gesture (EDO turned, Fill chosen, the button); automation clamps, doesn't refill
- [x] Held keys lit on the piano and the circle (plugin_gui.h's es_held_keys)
- [x] Tried in tools/preview_editor.mm ("EDOSpread12 Preview.app"): select, drag, Fill, EDO, Origin (2026-10-29)
- [x] Seen working in a host with notes playing (2026-10-29)
- [ ] A host's own change to EDO (an automation lane, or its parameter list) moves the knob and leaves the steps alone (clamped to 0 … N), as docs/design.md says
- [ ] StageHand: host-side parameter control (automation, or mapping a controller to a plugin parameter); StageHand's work

## M5: Other formats: not now (2026-10-29)
- CLAP only. VST3 isn't planned: no MIDI-effect category, and every note would be rounded to a semitone.
- AUv2 deferred: it would load in Logic's MIDI FX slot but play semitones only (see Later: MPE output).

## M6: Docs
- [ ] README.md, in Retune's shape: what it does, formats, installing
- [ ] BUILDING.md in the shared layout
- [ ] THIRD_PARTY_NOTICES.md: every bundled component, with its license text

## Later
- [ ] Map files: save and load tables; Scala `.kbm` import and export
- [ ] AUv2 (`aumi`, `ESpr`/`nLKL`) via clap-wrapper, if wanted: Logic and MainStage, semitones only
- [ ] MPE output (a channel per note, bent to pitch): microtones where MIDI 2.0 can't go, as in Logic
