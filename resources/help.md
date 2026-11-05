<div style="display: flex; align-items: center; gap: 0.75rem; margin: 1.5rem 0 0.5rem;">
  <a href="https://www.unlikelyware.com" target="_blank" rel="noopener noreferrer">
    <img src="Unlikelyware.png" alt="Unlikelyware" style="width: 56px; height: auto; border-radius: 10px; margin: 0; display: block;">
  </a>
  <h1 style="margin: 0;">EDOSpread12</h1>
</div>

<p>
  <em>EDOSpread12 is a CLAP note effect on macOS and Linux. Fine adjustment uses the Control key on both: on the Mac that is Control, not Cmd.</em>
</p>

::: {.callout .callout-note .card}
## On This Page
[Overview](#overview) · [Getting Started](#getting-started) · [The Window](#window)

**The map —** [The Circle](#circle) · [The Piano](#piano) · [The Read-out](#readout)

**The controls —** [EDO](#edo) · [Origin](#origin) · [Fill](#fill) · [Fill All Keys](#fill-all) · [Scale](#scale)

**Playing —** [Hosts and Microtones](#hosts) · [Held Notes and MIDI](#midi) · [Tips](#tips) · [Troubleshooting](#troubleshooting)
:::

::: {.callout .callout-note .card}
## Overview {#overview}
EDOSpread12 is a **note effect**: it makes no sound. It sits in front of a synth and lets an ordinary 12-key keyboard play any **equal division of the octave** (EDO), from 1 to 96 steps.

Each of the 12 keys of an octave is given **its own step** of the EDO, and the pattern repeats every octave. The **Origin** key (A by default) is step 0 and plays at its usual pitch; the octave counts up from it. With an A origin in 19-EDO, the keys A, A#, B … G# are mapped to steps of 19-EDO, and you choose which.

The pitch of a key is its octave's origin plus **12 × step / N** semitones. So with an A origin, a key on step 5 of 19 sounds 316¢ above its octave's A.

**Fill** gives all 12 keys their steps at once: **Nearest** puts each key on the step nearest its usual pitch, **Floor** on the highest step not above it. Then move any key by hand.

The defaults (12-EDO, A origin, Nearest) leave every note where it was: EDOSpread12 does nothing until you change it.
:::

::: {.callout .callout-note .card}
## Getting Started {#getting-started}
1. **Put EDOSpread12 in front of a synth**: in Bitwig, a note FX before the instrument; in REAPER, before the instrument in the track's FX chain; in StageHand, the row's **MIDI FX** menu.
2. **Turn EDO** in the Tuning panel, say to 19. All 12 keys are refilled with the **Fill** choice (Nearest), so the keyboard plays the 19-EDO steps nearest the notes you know.
3. **Play.** Keys you hold light up on the piano and on the circle.
4. **Move a key:** click it on the piano, then drag its marker round the circle to the step you want.
5. **Start again** with **Fill all keys**.

The host saves every setting with its project.
:::

::: {.callout .callout-note .card}
## The Window {#window}
**The header:** the Unlikelyware logo (a click opens unlikelyware.com), **EDOSpread12**, and **Help** at the right (the **?** box that opens this page in your browser).

**Below it:**
- **The map**, on the left: the [read-out](#readout) at the top, the [step circle](#circle), and a [piano octave](#piano) from the origin.
- **Tuning**, on the right: [EDO](#edo), [Origin](#origin), [Fill](#fill) and [Fill all keys](#fill-all).
- **View**, below it: the [Scale](#scale) menu.

**Resizing:** drag the host window's corner. The window keeps its shape (860 × 600) and scales everything in it; the smallest size is 688 × 480.
:::

<details class="disclosure">
<summary class="disclosure-title subtle">Knob, menus and the map</summary>

::: disclosure-content
| Gesture | Result |
|---|---|
| Drag the EDO knob up or down | Change EDO |
| Control-drag | Ten times finer (Control on the Mac too) |
| Scroll over the knob | Change EDO |
| Double-click or Shift-click the knob | Back to 12 |
| Click a menu | Opens its list; click a choice, or click elsewhere to close it |
| Click a piano key, or its mark on the circle | Selects that key |
| Drag the selected key's marker | Moves the key round the circle, a whole step at a time |
| Click the ring away from any mark | Moves the selected key to that step |

Each change is one gesture to the host, so it records as one automation move (a refill, of all 12 keys, as one).
:::

</details>

::: {.callout .callout-note .card}
## The Circle {#circle}
The circle is one octave of the chosen EDO: **N steps** round it, step 0 (the origin) at the top, the next origin (step N) back at the top again.

**Its ring is a piano octave bent round:** 12 arcs, one per key from the origin, **dark for a black key and light for a white one**, each named outside the ring. So you can see at a glance which familiar key a step is near: in 19-EDO with an A origin, step 5 sits in C's light arc, step 2 in A#'s dark one. Where the arcs break follows **Fill**:
- **Nearest:** each arc is its key's usual pitch ±50¢, so a step lies in the arc of the key it's nearest.
- **Floor:** each arc runs from its key's usual pitch up to the next key's, so a step lies in the arc of the key at or below it.

Changing Fill moves the arcs at once, before you refill, so you can see what a refill would do.

**Each key's step is a mark on the circle**, named. The **selected key's** mark is the large one, joined by a curve to its key on the piano: drag it round to give the key another step. It moves in whole steps; dragged past the top from the left, it lands on step N, the octave above.
:::

::: {.callout .callout-note .card}
## The Piano {#piano}
One octave, laid out straight, starting at the origin key.

- **Click a key** to select it: a curve joins it to its marker on the circle.
- **An orange key** is out of order (on a step below the key before it, or above the key after it) or shares a step with another key. Both are allowed: below 12-EDO some keys must share.
- **A lit key** is one you're holding, in any octave and on any channel. Its mark on the circle lights too.
:::

::: {.callout .callout-note .card}
## The Read-out {#readout}
At the top of the map, for the selected key:

**C → step 5 of 19 · 316¢ · +16¢**

The key, its step and the EDO, how far that step is above the origin, and how far it is from the key's usual (12-TET) pitch: here, C plays 16¢ sharp of a normal C.
:::

::: {.callout .callout-note .card}
## EDO {#edo}
**N**, the number of equal steps in an octave: 1 to 96, default 12.

**Turning EDO here refills all 12 keys** with the current [Fill](#fill), as one gesture with the change: your hand edits are replaced. **A host automating EDO doesn't refill**: each key's step is limited to the new 0 … N, and comes back if N rises again. (Refilling from automation would fight the steps you set.)
:::

::: {.callout .callout-note .card}
## Origin {#origin}
The key that's step 0 and plays at its usual pitch: C … B, default A. The octave counts from it, and the piano and the circle start at it.

The 12 steps are kept **from the origin up**, so changing Origin moves the whole pattern, not each key's step: with an E origin, E gets what A had. In the host, the step parameters are named after their keys from the origin (with an A origin, *Step 1* is "Step A", *Step 4* "Step C").
:::

::: {.callout .callout-note .card}
## Fill {#fill}
How a refill gives the 12 keys their steps:
- **Nearest** (default): each key on the step nearest its usual pitch. Closest to the familiar sound; it may put the last key on step N, the octave above. A tie goes up.
- **Floor:** each key on the highest step at or below its usual pitch. Never sharp.

**Choosing a Fill here refills all 12 keys.** Fill also decides where the circle's arcs break.
:::

::: {.callout .callout-note .card}
## Fill All Keys {#fill-all}
Refills all 12 keys with the current Fill, over any hand edits. Use it to start again.
:::

::: {.callout .callout-note .card}
## Scale {#scale}
Lights a scale on the map, starting from the [origin](#origin): **None** (default) or one of 29, the same list as NoteGran Chords and KeyboardEDO:
- **The modes of the major scale:** Major (Ionian), Dorian, Phrygian, Lydian, Mixolydian, Minor (Aeolian), Locrian.
- **Of melodic minor:** Melodic minor, Dorian b2, Lydian augmented, Lydian dominant, Mixolydian b6, Locrian #2, Altered.
- **Of harmonic minor:** Harmonic minor, Locrian #6, Ionian #5, Dorian #4, Phrygian dominant, Lydian #2, Ultralocrian.
- **And:** Harmonic major, Major and Minor pentatonic, Whole tone, the three diminished scales, and Blues.

**The three diminished scales** are the three sets of eight notes the diminished (whole–half) scale can make, each a half step above the last (a fourth would be the first again): **whole-half** from the origin (A B C D D# F F# G# from A), **half-whole** from it (A A# C C# D# E F# G), and **whole-half from the 2nd** (B C# D E F G G# A# from A), which leaves the origin out.

**The scale's keys are lit, and the steps they're mapped to:** their names round the ring and their ticks in the accent, their marks ringed, their piano keys dotted. In 12-EDO with an A origin, Lydian dominant lights A, B, C#, D#, E, F# and G. In another EDO the lit steps are wherever those keys are now, so moving a key's marker moves its lit step too.

**It's a view only:** it changes no step and sends nothing. It's saved with the project, not automated.
:::

::: {.callout .callout-note .card}
## Hosts and Microtones {#hosts}
What EDOSpread12 receives decides what it can send:

| Host | EDOSpread12 gets | The synth hears |
|---|---|---|
| StageHand | MIDI 2.0 | The exact step (Pitch 7.9) |
| Bitwig | CLAP notes | The exact step, from a synth that honours tuning expressions |
| A host that takes no MIDI 2.0 and sends MIDI 1.0 | MIDI 1.0 | The nearest semitone |

- **MIDI 2.0:** a note carries its exact pitch as a Pitch 7.9 attribute; its note number is the whole part, so a receiver that ignores the attribute plays the nearest key below.
- **MIDI 1.0 in, MIDI 2.0 out:** where the host takes MIDI 2.0, a MIDI 1.0 note that lands between semitones goes out as MIDI 2.0 rather than being rounded. One that lands on a semitone stays MIDI 1.0.
- **CLAP notes:** the nearest key, followed straight away by a tuning expression with the rest.

The Unlikelyware synths play Pitch 7.9, and PlainOldSynth, StruckSynth, StruckRoom and StruckGaussian also play CLAP tuning expressions.
:::

::: {.callout .callout-note .card}
## Held Notes and MIDI {#midi}
- **Changing the map while notes are held** moves only the notes you play next. Each sounding note remembers where it went, so its Note Off, pressure and per-note messages follow it, and no note gets stuck.
- **Two keys on one step** are two notes on one key: the second one ends the first.
- **A note that arrives with its own pitch** (MIDI 2.0's Pitch 7.9) keeps its offset from its key.
- **Notes moved below 0 or above 127** (MIDI's range) are dropped, with everything that belongs to them.
- **Everything that isn't a note** passes through unchanged: CCs, channel pitch bend, program changes, SysEx, clock.
:::

::: {.callout .callout-note .card}
## Tips {#tips}
- **Learn an EDO by ear:** pick it, leave Fill on Nearest, and play familiar tunes: the read-out shows how far each key has moved.
- **Below 12-EDO** keys must share steps; the orange keys show which.
- **A scale on the white keys:** with a C origin, put the seven white keys on the steps of your scale and the black keys on whatever is left.
- **Automate EDO safely:** automation clamps rather than refills, so a sweep down and back up returns to your map.
:::

::: {.callout .callout-note .card}
## Troubleshooting {#troubleshooting}
- **It sounds like 12-EDO.** The host sends MIDI 1.0 and takes no MIDI 2.0, or the synth ignores Pitch 7.9 and tuning expressions. See [Hosts and Microtones](#hosts).
- **My hand edits disappeared.** Turning EDO or choosing a Fill in the editor refills all 12 keys.
- **Changing a key didn't change the note I'm holding.** That's the design: only new notes take the new map.
- **Two keys play the same note, and one cuts the other off.** They share a step. Move one.
- **Some notes make no sound.** They were moved outside MIDI's 0–127 range.
- **A rebuilt EDOSpread12 doesn't show its changes.** Quit and restart the host: a running host keeps the plugin it loaded first.
:::
