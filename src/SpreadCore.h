#pragma once

// SpreadCore: EDOSpread12's rules, with no CLAP in it. The 12 keys of each octave, counted from
// the origin, are each assigned a step of the N-EDO octave; the table repeats every octave
// (docs/design.md, "The mapping"). That gives every input key its own pitch shift, which uw-core's
// NoteRemap applies to UMP (MIDI 1.0 promoted to MIDI 2.0 when the shift has a fraction) and
// ClapNoteRemap to CLAP note events. The latch, out-of-range drop and retrigger are theirs.

#include <array>
#include <cstddef>
#include <cstdint>

#include "uw/ump/MidiMessage.h"
#include "uw/ump/NoteRemap.h"

namespace edospread12 {

// Parameter ranges (docs/design.md, "Behaviour").
inline constexpr int kMinEdo    = 1;
inline constexpr int kMaxEdo    = 96;
inline constexpr int kKeys      = 12;   // keys of the octave, so table entries
inline constexpr int kOriginA   = 9;    // pitch classes count from C = 0

// How the editor refills the table (docs/design.md, "Filling the table").
enum class Fill : int { Floor = 0, Nearest = 1 };

using Table = std::array<int, kKeys>;   // steps, from the origin up; each 0 … N

// The table `fill` gives for N-EDO: key d of the octave sits at d / 12 of an octave.
// Floor: the highest step at or below it. Nearest: the nearest step, a tie going up.
Table filled(Fill fill, int edo) noexcept;

struct Params {
    int   edo    = 12;
    int   origin = kOriginA;          // pitch class of step 0: C = 0 … B = 11
    Fill  fill   = Fill::Nearest;     // how the table was made; the core doesn't refill
    Table steps  = filled(Fill::Nearest, 12);
};

// The editor's Scale menu: a scale the circle highlights, from the origin. It changes no note: the
// steps lit are the ones the scale's keys are mapped to. Bit d of `keys` is key d from the origin.
// The list is NoteGran Chords' (its core/Scales, which KeyboardEDO shares), in its order and names,
// then the three diminished scales and Blues. The diminished scale's 8 notes come in three sets, a
// half step apart, so all three are here: whole-half from the origin, half-whole from it, and
// whole-half from the 2nd (the origin isn't in it).
struct Scale {
    const char*   name;
    std::uint16_t keys;
};
inline constexpr Scale kScales[] = {
    {"None", 0},
    {"Major (Ionian)", 0b1010'1011'0101},            // 0 2 4 5 7 9 11
    {"Dorian", 0b0110'1010'1101},                    // 0 2 3 5 7 9 10
    {"Phrygian", 0b0101'1010'1011},                  // 0 1 3 5 7 8 10
    {"Lydian", 0b1010'1101'0101},                    // 0 2 4 6 7 9 11
    {"Mixolydian", 0b0110'1011'0101},                // 0 2 4 5 7 9 10
    {"Minor (Aeolian)", 0b0101'1010'1101},           // 0 2 3 5 7 8 10
    {"Locrian", 0b0101'0110'1011},                   // 0 1 3 5 6 8 10
    {"Melodic minor", 0b1010'1010'1101},             // 0 2 3 5 7 9 11
    {"Dorian b2", 0b0110'1010'1011},                 // 0 1 3 5 7 9 10
    {"Lydian augmented", 0b1011'0101'0101},          // 0 2 4 6 8 9 11
    {"Lydian dominant", 0b0110'1101'0101},           // 0 2 4 6 7 9 10
    {"Mixolydian b6", 0b0101'1011'0101},             // 0 2 4 5 7 8 10
    {"Locrian #2", 0b0101'0110'1101},                // 0 2 3 5 6 8 10
    {"Altered", 0b0101'0101'1011},                   // 0 1 3 4 6 8 10
    {"Harmonic minor", 0b1001'1010'1101},            // 0 2 3 5 7 8 11
    {"Locrian #6", 0b0110'0110'1011},                // 0 1 3 5 6 9 10
    {"Ionian #5", 0b1011'0011'0101},                 // 0 2 4 5 8 9 11
    {"Dorian #4", 0b0110'1100'1101},                 // 0 2 3 6 7 9 10
    {"Phrygian dominant", 0b0101'1011'0011},         // 0 1 4 5 7 8 10
    {"Lydian #2", 0b1010'1101'1001},                 // 0 3 4 6 7 9 11
    {"Ultralocrian", 0b0011'0101'1011},              // 0 1 3 4 6 8 9
    {"Harmonic major", 0b1001'1011'0101},            // 0 2 4 5 7 8 11
    {"Major pentatonic", 0b0010'1001'0101},          // 0 2 4 7 9
    {"Minor pentatonic", 0b0100'1010'1001},          // 0 3 5 7 10
    {"Whole tone", 0b0101'0101'0101},                // 0 2 4 6 8 10
    {"Diminished (whole-half)", 0b1011'0110'1101},   // 0 2 3 5 6 8 9 11
    {"Diminished (half-whole)", 0b0110'1101'1011},   // 0 1 3 4 6 7 9 10
    {"Diminished (from the 2nd)", 0b1101'1011'0110}, // 1 2 4 5 7 8 10 11
    {"Blues", 0b0100'1110'1001},                     // 0 3 5 6 7 10
};
inline constexpr int kScaleCount = int(sizeof(kScales) / sizeof(kScales[0]));

// Params with every field clamped to its range (steps to 0 … N, after N is clamped).
Params clamped(const Params& p) noexcept;

// The pitch input `key` plays, in 1/512 semitone (Pitch 7.9 units): the origin of its octave plus
// its step, 12 × step / N semitones, rounded to nearest.
int pitch79(const Params& p, int key) noexcept;

// Every key's shift from its own pitch, in 1/512 semitone, for NoteRemap and ClapNoteRemap.
void shifts(const Params& p, int (&shift79)[128]) noexcept;

// Most messages one process() call can write.
inline constexpr std::size_t kMaxOut = uw::ump::NoteRemap::kMaxOut;

class SpreadCore {
public:
    SpreadCore() noexcept;

    // Takes effect for notes that start after the call; sounding notes keep their latched key.
    void setParams(const Params& p) noexcept;
    const Params& params() const noexcept { return params_; }

    // One UMP message in. MIDI 2.0 notes leave with a Pitch 7.9 attribute; a MIDI 1.0 note whose
    // pitch has a fraction leaves as MIDI 2.0, and one on a whole key stays MIDI 1.0
    // (docs/design.md, "Behaviour"). Writes at most kMaxOut messages to `out`; returns the count.
    std::size_t process(const uw::MidiMessage& in, uw::MidiMessage* out) noexcept {
        return remap_.process(in, out);
    }

    // On by default. Off where MIDI 2.0 can't leave the plugin (clap-wrapper, or a host that
    // takes no MIDI 2.0): a MIDI 1.0 note is then rounded to the nearest key, as in Retune.
    void setPromoteMidi1(bool on) noexcept { remap_.setPromoteMidi1(on); }

    // Forget every sounding note (host reset).
    void reset() noexcept { remap_.reset(); }

private:
    Params             params_;
    uw::ump::NoteRemap remap_;
};

} // namespace edospread12
