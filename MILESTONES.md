# EDOSpread12 milestones

## Tests
- all: cmake -S . -B build -G Ninja && ninja -C build && ctest --test-dir build --output-on-failure
- validator: clap-validator validate build/EDOSpread12.clap
- install: ninja -C build install-clap install-vst3 install-auv2 (CLAP and VST3 symlinked, the AU copied)
- au: auval -v aumi ESpr nLKL
- vst3: Steinberg's validator (built from external/vst3sdk) on build/EDOSpread12.vst3

## M0: Plan
- [x] docs/design.md: 12-entry map from the origin, repeating by octave; EDO 1–96; free order; output as Retune (2026-10-29)
- [x] MIDI 1.0 in, fraction out: sent as MIDI 2.0, as NoteGran Chords (2026-10-29)
- [x] Fill: Floor | Nearest; Even spread dropped, as it is 12-EDO spacing (2026-10-29)
- [x] Open questions settled: steps 0 … N, refill in the editor only, table from the origin, shared code through uw-core (2026-10-29)
- [ ] AU subtype code (`ESpr` proposed)
- [ ] Private repo erickampman/EDOSpread12

## M1: Core library
- [ ] uw-core: Retune's latch table, out-of-range drop, retrigger and `ClapNotes`, with their own tests; pushed first
- [ ] Retune moves its pin and uses them, its tests still passing
- [ ] CMake skeleton, external/clap and external/uw-core (UMP-only) submodules, ctest
- [ ] Mapping: origin, octave, table lookup, steps 0 … N, Pitch 7.9 out; incoming pitch offset kept
- [ ] Fills: Floor, Nearest (ties up)
- [ ] Latch table, retrigger, two keys on one output key, out-of-range drop, pass-through, All Notes Off / reset
- [ ] MIDI 1.0 (kept on a 12-TET key, else promoted to MIDI 2.0, per-note messages following), MIDI 2.0 (attribute type 3) and CLAP note (key + tuning expression) paths
- [ ] Table-driven tests: N = 1, 5, 12, 19, 31, 96; each fill and origin; out-of-order and shared steps; held-note table change; all three dialects

## M2: Raw CLAP plugin
- [ ] clap_entry, note-effect feature, note ports in + out (MIDI | MIDI2 | CLAP, prefer MIDI2)
- [ ] 15 parameters (EDO, Origin, Fill, Step 1–12, names following the origin), state `EDOSpread12 1`, clap-validator clean
- [ ] Headless host test: MIDI 1.0, MIDI 2.0 and CLAP notes in, output events checked
- [ ] Default state passes every note unchanged

## M3: Heard
- [ ] StageHand: KeyboardEDO (12-EDO piano) → EDOSpread12 → StruckSynth, in 19-EDO and 31-EDO
- [ ] Bitwig, with a synth that honours tuning expressions
- [ ] An EDO below 12 (5-EDO): shared steps play without stuck notes

## M4: Editor
- [ ] DPF for drawing only, NoteGran's widget kit, one accent
- [ ] Step circle of the chosen EDO, step 0 at the top; its ring in 12 arcs, dark for black keys and light for white, breaking where Fill puts them
- [ ] Piano octave laid out straight below the circle, from the origin; click a key to select it
- [ ] Curve from the selected key to its indicator on the circle; drag the indicator in whole steps to reassign
- [ ] Marks for the other keys' steps; out-of-order and shared keys tinted on the piano
- [ ] EDO, Origin, Fill, Fill button; the selected key's step and cents offset
- [ ] Refill in the editor as one gesture; automation clamps, doesn't refill
- [ ] Seen with notes playing in StageHand and Bitwig

## M5: Other formats (clap-wrapper)
- [ ] clap-wrapper and the VST3 SDK as submodules (`tools/fetch_deps.sh`), `EDOSPREAD12_BUILD_VST3`, `EDOSPREAD12_BUILD_AUV2`
- [ ] AUv2 (`aumi`) and VST3, MIDI 1.0 rounded to the nearest key (no promotion under the wrapper); auval and Steinberg's validator pass
- [ ] Checked in Logic (MIDI FX slot), REAPER and Bitwig

## M6: Docs
- [ ] README.md, in Retune's shape: what it does, formats, installing
- [ ] BUILDING.md in the shared layout
- [ ] THIRD_PARTY_NOTICES.md: every bundled component, with its license text

## Later
- [ ] Map files: save and load tables; Scala `.kbm` import and export
- [ ] Wrapper patches to keep the fraction in AU and VST3 (shared with Retune's M5)
