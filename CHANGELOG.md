# Changelog

All notable changes to EDOSpread12. Versions follow the git tags; the format is
[Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

## [0.9.1] - 2026-11-26

### Added

- An AUv2 MIDI effect on the Mac, for Logic's MIDI FX slot. Built with Unlikelyware's fork of
  clap-wrapper, it sends MIDI 2.0 with each note's exact pitch, so in Logic the synth after it
  plays the EDO's steps. REAPER doesn't pass notes through an AU MIDI effect; use the CLAP there.

### Changed

- Help opens the help page in a window of the plugin's own, not in the web browser. Nothing is
  written to the temp folder, and on Linux no web server runs in the host any more.

## [0.9.0] - 2026-11-14

First public preview.

### Added

- MIDI note effect that plays any EDO from 1 to 96 from an ordinary 12-key keyboard, each key on the
  step you give it.
- Exact pitch as a MIDI 2.0 Pitch 7.9 attribute, or in the CLAP note dialect for hosts such as
  Bitwig.
- An editor for the key map, and no stuck notes when the map changes while notes sound.
- CLAP plugin for Linux (x86-64) and macOS 26 or later (Apple Silicon).
