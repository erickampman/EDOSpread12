# Building EDOSpread12

EDOSpread12 builds the same way on Linux and macOS: CMake + Ninja, git submodules, C++20. The build
makes the core library `edospread12_core`, the plugin `EDOSpread12.clap` (with its editor), two test
programs, `edospread12-core-tests` (the mapping) and `edospread12-clap-tests` (a headless host that
loads the built `.clap`), and on macOS `EDOSpread12 Preview.app`, which opens the editor alone.

## What you need

**macOS:** macOS 26 or later on Apple Silicon (arm64). Intel Macs aren't supported.

- The Xcode Command Line Tools: `xcode-select --install`. They bring clang, the macOS SDK, git and
  Python 3.
- [Homebrew](https://brew.sh), then `brew install cmake ninja`.
- Full Xcode isn't needed.

**Linux:** a C++20 compiler (GCC 11+ or Clang 14+), CMake 3.21+, Ninja, Python 3 and git. On Debian
or Ubuntu this installs them and every library the build needs:

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 git \
  libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxinerama-dev \
  libxcb1-dev libx11-xcb-dev libxcb-util-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libfontconfig1-dev libasound2-dev libjack-jackd2-dev \
  libxfixes-dev libxi-dev libxss-dev libxtst-dev libwayland-dev wayland-protocols \
  libegl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev libdecor-0-dev libudev-dev \
  libdbus-1-dev libibus-1.0-dev
```

It's one list for every Unlikelyware project (the same one CI installs), so it covers more than
EDOSpread12 needs. On other distros, install the same libraries under their own names.

Of those libraries EDOSpread12 uses only the X11 and OpenGL ones, for the editor. It builds uw-core
with `UW_CORE_UMP_ONLY`, which makes only `uw::ump`, plus `uw::clap-notes` against EDOSpread12's own
CLAP headers (no JACK, ALSA, CoreAudio or CoreMIDI). The editor uses DPF (`external/dpf`) for
drawing only: DGL, NanoVG and pugl, as NoteGran does. DPF is the author's fork (`erickampman/DPF`,
which pins the pugl fork `erickampman/pugl`), as every DPF user's is.
Python 3 embeds the logo at build time.

## Get the source

```bash
git clone https://github.com/erickampman/EDOSpread12.git
cd EDOSpread12
tools/fetch_deps.sh
```

`tools/fetch_deps.sh` checks out the git submodules at their pinned commits, with DPF's own pugl
submodule. Run it again after a `git pull` that moves, adds or re-points a submodule: a plain pull
leaves them where they were.

## Build and test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The same three commands on both platforms. Everything goes in `build/` (ignored by git).

The plugin is `build/EDOSpread12.clap`: a shared object on Linux, a bundle directory on macOS.

## Installing the plugin for hosts

A plain `cmake --build build` writes nothing outside `build/`. This target puts the plugin where
hosts look for it:

```bash
cmake --build build --target install-clap
```

| | Linux | macOS |
|---|---|---|
| CLAP | `~/.clap` | `~/Library/Audio/Plug-Ins/CLAP` |

`install-clap` symlinks the plugin in `build/`, so a host picks up a rebuild after a rescan. The
link points into this clone: delete the clone and the host loses the plugin, and running the
target in another build folder points the host there instead.

To keep the plugin without the clone, copy it instead (the `rm` clears a link the target left):

```bash
# Linux
mkdir -p ~/.clap
rm -rf ~/.clap/EDOSpread12.clap
cp build/EDOSpread12.clap ~/.clap/
```

```bash
# macOS
mkdir -p ~/Library/Audio/Plug-Ins/CLAP
rm -rf ~/Library/Audio/Plug-Ins/CLAP/EDOSpread12.clap
cp -R build/EDOSpread12.clap ~/Library/Audio/Plug-Ins/CLAP/
```

For every user instead, the same folders without `~` (`/usr/lib/clap` on Linux,
`/Library/Audio/Plug-Ins/...` on macOS), with `sudo`. Then rescan in the host. A host that is
already running keeps the plugin it loaded first: quit and restart it after a rebuild.

## Options

Pass these to the first command (`cmake -B build ... -DNAME=VALUE`).

| Option | Default | Effect |
|---|---|---|
| `EDOSPREAD12_UW_CORE_DIR` | `external/uw-core` | Build against another uw-core clone (below). |

## Checking a build

```bash
clap-validator validate build/EDOSpread12.clap   # 35 passed, 0 failed, 0 warnings, 9 skipped
```

The skipped tests are for audio ports and presets, which a note effect doesn't have.
[clap-validator](https://github.com/free-audio/clap-validator) is free-audio's CLAP checker.

## Looking at the editor without a DAW (macOS)

`EDOSpread12 Preview.app` (from `tools/preview_editor.mm`) is a minimal CLAP host that opens the
editor alone in a window. Nothing plays, but every control works. Parameters can be set at the
start as `id=value` (0 EDO, 1 Origin, 2 Fill, 3 … 14 the steps from the origin):

```bash
open -n "build/EDOSpread12 Preview.app" --args "$PWD/build/EDOSpread12.clap" 0=19
```

## Building against a local uw-core

To try a uw-core change that isn't pushed and pinned yet, point the build at another clone:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DEDOSPREAD12_UW_CORE_DIR=../uw-core
```

When the change is ready, push uw-core first, then move EDOSpread12's submodule pin
(uw-core's README, "Pushing: uw-core first"). The note remapping EDOSpread12 shares with Retune
(`NoteRemap`, `ClapNoteRemap`) lives there.

## Making a release package

For the maintainer only: a release package is signed with the Developer ID certificate and
published to the [downloads repo](https://github.com/erickampman/unlikelyware), so these steps
need that keychain setup and write access there. Building from source, as above, needs neither.

Once per machine, point `UW_DOWNLOADS_DIR` at a checkout of the downloads repo, in your shell's
startup file (`~/.zprofile` or `~/.zshrc` with zsh, `~/.bashrc` with bash):

```bash
export UW_DOWNLOADS_DIR=~/path/to/unlikelyware
```

Then, in this checkout, after building the release tag as above:

```bash
"$UW_DOWNLOADS_DIR/scripts/package.sh"
```

```bash
"$UW_DOWNLOADS_DIR/scripts/publish.sh" v0.9.0
```

The first makes this platform's package (on macOS, signed and notarized); the second uploads it
to the release. The details are in the downloads repo's
[README](https://github.com/erickampman/unlikelyware#making-a-release-package).

## How this differs from the other projects

Every Unlikelyware project builds the same way: `git clone`, `tools/fetch_deps.sh`, the three
commands under "Build and test", and for a plugin `cmake --build build --target install-clap`. The
same package list, the same `build/` folder, the same option names (`<PROJECT>_UW_CORE_DIR` and so
on). What's different here, and why:

- CLAP only: no VST3, and AUv2 deferred (docs/design.md, "Form"), so no `install-vst3` or
  `install-auv2`, and no clap-wrapper or VST3 SDK submodules.
- A preview app for the editor (macOS), since a note effect's editor is otherwise only seen inside
  a host.
- No `_WERROR` option (the build has no warnings today, on macOS).
- CI (`.github/workflows/ci.yml`) runs on `ubuntu-24.04`, the oldest Ubuntu supported, not `ubuntu-latest`, and on `macos-26`.
