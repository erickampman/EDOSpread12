# Contributing to EDOSpread12

Bug reports and pull requests are welcome. Building: [BUILDING.md](BUILDING.md); how it works and
why: [docs/design.md](docs/design.md), whose Behaviour and Code sections map the source. This page
is the rules the code depends on.

## Before each commit

```sh
ctest --test-dir build --output-on-failure
clap-validator validate build/EDOSpread12.clap
```

`edospread12-core` tests the mapping (`SpreadCore`: each EDO, fill and origin, out-of-order and
shared steps, all three note dialects); `edospread12-clap` loads the built `.clap` and plays it
headless, so it needs no MIDI hardware. After rebuilding, quit and restart the host: a running host
keeps the plugin it loaded first.

## Rules

- **Parameters: append, never insert.** The CLAP parameter id is the parameter's index, and hosts
  save sessions and automation by id: a new parameter goes at the end, and none is reordered or
  removed. The twelve steps are indexed from the Origin, not from C; only their names follow it.
- **The default leaves every note where it was.** 12-EDO, A origin, Nearest: a new instance changes
  no pitch, and the tests check it.
- **Notes always end.** Every sounding note is latched to where it went (uw-core's `NoteRemap` and
  `ClapNoteRemap`), and its Note Off and per-note messages use the latch, so a map changed while
  notes are held moves only new notes. Two keys on one output key: the second ends the first.
- **Refill only in the editor.** Turning EDO, choosing a Fill or "Fill all keys" in the editor
  refills the table as one gesture; a host's own change to EDO only clamps the steps to 0 … N.
- **Real time.** No allocation, locks or threads on the audio thread. Values reach `process()`
  through atomics; `state_load` stores them and the next block applies them, and an Origin change
  asks for the rescan on the main thread.
- **The core has no CLAP in it** (`src/SpreadCore.*`): EDOSpread12's own code is the mapping, and
  latching, dialects and MIDI 1.0 promotion are uw-core's, shared with Retune.

## Changing uw-core or DPF

UMP code comes from uw-core (`uw::ump`, `uw::clap-notes`); EDOSpread12 writes no UMP parsing of its
own. A change it needs there, or in DPF, is committed and pushed first (pugl, then DPF), then the
pin moves here. `git config push.recurseSubmodules check` makes git refuse a pin the other repo's
remote doesn't have yet.

## Third-party code and licenses

EDOSpread12 is MIT licensed, and contributions are taken under the same license: a pull request is
offered as MIT.

- **What ships.** [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) lists every third-party
  component in the builds, with its version, license and full license text, and every binary
  package includes it. A change that adds a component, or moves one to a new version, updates its
  entry in the same commit.
- **Upstream code stays as it came.** Submodules aren't edited in place: a fix goes upstream, to
  Unlikelyware's DPF or pugl fork, or into a patch file that `tools/fetch_deps.sh` applies.
  Copied-in files keep their copyright and license headers.
- **A new dependency** must allow MIT-licensed source and binary distribution with nothing more
  than keeping its notice: MIT, BSD, ISC, zlib, Apache-2.0 (with its NOTICE file, if it has one) or
  public domain, and the SIL Open Font License for fonts. No GPL or LGPL code.
- **Trademarks.** Name hosts and other products plainly, to say what works with what, never so it
  reads as an endorsement.
