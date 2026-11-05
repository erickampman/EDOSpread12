// SpreadCore tests: the fills, the mapping, and notes through it in each dialect (docs/design.md,
// phase 1). The latch itself is uw-core's NoteRemap and ClapNoteRemap, tested there. Silent unless
// a check fails.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

#include "EdoFacts.h"
#include "SpreadCore.h"
#include "uw/clap/ClapNoteRemap.h"
#include "uw/ump/Ump.h"

using namespace edospread12;
namespace ump = uw::ump;

namespace {

int failures = 0;

#define CHECK(expr)                                                                       \
    do {                                                                                  \
        if (!(expr)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

std::vector<uw::MidiMessage> run(SpreadCore& core, const uw::MidiMessage& in) {
    uw::MidiMessage out[kMaxOut];
    const std::size_t n = core.process(in, out);
    return {out, out + n};
}

Params make(int edo, Fill fill, int origin = kOriginA) {
    Params p;
    p.edo    = edo;
    p.fill   = fill;
    p.origin = origin;
    p.steps  = filled(fill, edo);
    return p;
}

// Semitones above C-1 (MIDI key 0), from 7.9.
double semitones(int pitch79) { return pitch79 / 512.0; }

// ---------------------------------------------------------------------------------------------

void testFills() {
    struct Case {
        int   edo;
        Fill  fill;
        Table steps;
    };
    const Case cases[] = {
        {12, Fill::Nearest, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
        {12, Fill::Floor,   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
        {19, Fill::Nearest, {0, 2, 3, 5, 6, 8, 10, 11, 13, 14, 16, 17}},   // 9.5 → 10: a tie goes up
        {19, Fill::Floor,   {0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17}},
        {31, Fill::Nearest, {0, 3, 5, 8, 10, 13, 16, 18, 21, 23, 26, 28}},
        {5,  Fill::Nearest, {0, 0, 1, 1, 2, 2, 3, 3, 3, 4, 4, 5}},         // B reaches N: the octave above
        {5,  Fill::Floor,   {0, 0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4}},
        {1,  Fill::Nearest, {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}},
        {1,  Fill::Floor,   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {24, Fill::Nearest, {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22}},
        {96, Fill::Floor,   {0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88}},
    };
    for (const Case& c : cases) {
        const Table t = filled(c.fill, c.edo);
        CHECK(t == c.steps);
        if (t != c.steps) std::fprintf(stderr, "  (%d-EDO, fill %d)\n", c.edo, static_cast<int>(c.fill));
    }
    // Floor never sounds sharp of 12-TET; Nearest is never further than half a step from it.
    for (int n = kMinEdo; n <= kMaxEdo; ++n) {
        const Table f = filled(Fill::Floor, n), r = filled(Fill::Nearest, n);
        for (int d = 0; d < kKeys; ++d) {
            CHECK(f[static_cast<std::size_t>(d)] * kKeys <= d * n);
            CHECK(std::abs(r[static_cast<std::size_t>(d)] * kKeys - d * n) * 2 <= kKeys);
        }
    }
}

void testDefaultPassesThrough() {
    const Params p;
    for (int key = 0; key < 128; ++key) CHECK(pitch79(p, key) == key * 512);

    SpreadCore core;
    for (int key = 0; key < 128; ++key) {
        const auto k = static_cast<std::uint8_t>(key);
        auto o = run(core, ump::midi1NoteOn(0, 0, k, 100));
        CHECK(o.size() == 1 && o[0].words[0] == ump::midi1NoteOn(0, 0, k, 100).words[0]);
        o = run(core, ump::midi1NoteOff(0, 0, k));
        CHECK(o.size() == 1 && ump::isNoteOff(o[0]) && ump::noteNumber(o[0]) == key);
    }
}

void testMapping() {
    // 19-EDO from A: A4 stays; C5 is 5 steps up, 316¢; G#4 is the octave below's step 17.
    const Params p = make(19, Fill::Nearest);
    CHECK(pitch79(p, 69) == 69 * 512);
    CHECK(pitch79(p, 72) == 69 * 512 + 1617);                 // 512 × 60/19 = 1616.8
    CHECK(pitch79(p, 68) == 57 * 512 + 5497);                 // 512 × 12 × 17/19 = 5497.3
    CHECK(pitch79(p, 81) == 81 * 512);                        // A5: the next origin
    CHECK(pitch79(p, 84) - pitch79(p, 72) == 12 * 512);       // the table repeats every octave
    CHECK(pitch79(p, 0) == pitch79(p, 12) - 12 * 512);        // below the lowest origin too
    CHECK(std::fabs(semitones(pitch79(p, 72) - 69 * 512) * 100 - 315.8) < 0.1);

    // The origin moves the whole pattern: from C, C4 stays and A4 is C's step 14 (9 × 19/12 = 14.25).
    const Params c = make(19, Fill::Nearest, 0);
    CHECK(pitch79(c, 60) == 60 * 512);
    CHECK(pitch79(c, 69) == 60 * 512 + (2 * 512 * 12 * 14 + 19) / (2 * 19));

    // Step N is the next origin.
    Params n = make(19, Fill::Nearest);
    n.steps[11] = 19;
    CHECK(pitch79(n, 68) == 69 * 512);

    // Out of order: A# above B.
    Params o = make(12, Fill::Nearest);
    o.steps[1] = 2;
    o.steps[2] = 1;
    CHECK(pitch79(o, 70) == 71 * 512 && pitch79(o, 71) == 70 * 512);

    // Steps are clamped to 0 … N, EDO to 1 … 96, the origin to a pitch class.
    Params x = make(12, Fill::Nearest);
    x.steps[3] = 40;
    x.edo      = 19;
    CHECK(clamped(x).steps[3] == 19);
    x.edo = 500;
    CHECK(clamped(x).edo == kMaxEdo);
    x.origin = -3;
    CHECK(clamped(x).origin == 0);
}

void testDialects() {
    SpreadCore core;
    core.setParams(make(19, Fill::Nearest));

    // MIDI 2.0: the exact pitch.
    auto o = run(core, ump::midi2NoteOn(0, 0, 72, 0x8000));
    CHECK(o.size() == 1 && ump::messageType(o[0]) == ump::MessageType::Midi2ChannelVoice &&
          ump::noteNumber(o[0]) == 72 && ump::notePitch79(o[0]) == 69 * 512 + 1617);

    // MIDI 1.0 with a fraction: promoted to MIDI 2.0, its Note Off too.
    o = run(core, ump::midi1NoteOn(0, 1, 72, 100));
    CHECK(o.size() == 1 && ump::messageType(o[0]) == ump::MessageType::Midi2ChannelVoice &&
          ump::notePitch79(o[0]) == 69 * 512 + 1617);
    o = run(core, ump::midi1NoteOff(0, 1, 72));
    CHECK(o.size() == 1 && ump::messageType(o[0]) == ump::MessageType::Midi2ChannelVoice &&
          ump::isNoteOff(o[0]) && ump::noteNumber(o[0]) == 72);

    // MIDI 1.0 landing on a whole key (the origin) stays MIDI 1.0.
    o = run(core, ump::midi1NoteOn(0, 1, 69, 100));
    CHECK(o.size() == 1 && ump::messageType(o[0]) == ump::MessageType::Midi1ChannelVoice &&
          ump::noteNumber(o[0]) == 69);

    // CLAP notes: the nearest key and a tuning expression.
    struct Collector {
        std::vector<std::pair<int, double>> events;   // key, tuning (0 for a note)
        clap_output_events_t out{this, [](const clap_output_events_t* l, const clap_event_header_t* h) {
            auto* self = static_cast<Collector*>(l->ctx);
            if (h->type == CLAP_EVENT_NOTE_EXPRESSION) {
                const auto* x = reinterpret_cast<const clap_event_note_expression_t*>(h);
                self->events.push_back({x->key, x->value});
            } else {
                self->events.push_back({reinterpret_cast<const clap_event_note_t*>(h)->key, 0.0});
            }
            return true;
        }};
    } c;
    uw::ClapNoteRemap clap;
    int s[128];
    shifts(make(19, Fill::Nearest), s);
    clap.setShifts(s);
    clap_event_note_t on{};
    on.header  = {sizeof(on), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0};
    on.note_id = 1;
    on.key     = 72;
    clap.process(&on.header, &c.out);
    CHECK(c.events.size() == 2 && c.events[0].first == 72 && c.events[1].first == 72 &&
          std::fabs(c.events[1].second - (1617 - 3 * 512) / 512.0) < 1e-9);
}

void testHeldNoteKeepsItsPitch() {
    SpreadCore core;
    core.setParams(make(19, Fill::Nearest));
    run(core, ump::midi2NoteOn(0, 0, 72, 0x8000));

    // Back to 12-EDO while C5 is held: its Note Off still goes where it went.
    core.setParams(make(12, Fill::Nearest));
    auto o = run(core, ump::midi2NoteOff(0, 0, 72));
    CHECK(o.size() == 1 && ump::isNoteOff(o[0]) && ump::noteNumber(o[0]) == 72);
    o = run(core, ump::midi2NoteOn(0, 0, 72, 0x8000));
    CHECK(o.size() == 1 && ump::notePitch79(o[0]) == 72 * 512);
}

void testSmallEdoSharedSteps() {
    // 5-EDO from A: A and A# both land on step 0 (A). The second ends the first; no stuck notes.
    SpreadCore core;
    core.setParams(make(5, Fill::Nearest));
    auto o = run(core, ump::midi1NoteOn(0, 0, 69, 100));
    CHECK(o.size() == 1 && ump::noteNumber(o[0]) == 69);
    o = run(core, ump::midi1NoteOn(0, 0, 70, 100));
    CHECK(o.size() == 2 && ump::isNoteOff(o[0]) && ump::noteNumber(o[0]) == 69 && ump::isNoteOn(o[1]) &&
          ump::noteNumber(o[1]) == 69);
    CHECK(run(core, ump::midi1NoteOff(0, 0, 69)).empty());
    o = run(core, ump::midi1NoteOff(0, 0, 70));
    CHECK(o.size() == 1 && ump::isNoteOff(o[0]) && ump::noteNumber(o[0]) == 69);

    // 1-EDO: every key is an A.
    core.setParams(make(1, Fill::Floor));
    for (int key = 60; key < 72; ++key) {
        o = run(core, ump::midi1NoteOn(0, 2, static_cast<std::uint8_t>(key), 100));
        CHECK(!o.empty() && ump::noteNumber(o.back()) % 12 == kOriginA);
    }
}

void testOutOfRange() {
    // From C, with G sent up to the next C: G9 (127) would be 132, so it's dropped with its Note Off.
    Params p = make(12, Fill::Nearest, 0);
    p.steps[7] = 12;
    SpreadCore core;
    core.setParams(p);
    CHECK(pitch79(p, 127) == 132 * 512);
    CHECK(run(core, ump::midi1NoteOn(0, 0, 127, 100)).empty());
    CHECK(run(core, ump::midi1NoteOff(0, 0, 127)).empty());
    auto o = run(core, ump::midi1NoteOn(0, 0, 115, 100));   // G8 goes to C9, in range
    CHECK(o.size() == 1 && ump::noteNumber(o[0]) == 120);
}

// The Scale menu: NoteGran Chords' list (core/Scales, spelled as places on the chain of fifths from
// the key, C 0, G 1, D 2 … F -1), in its order, then the three diminished scales and Blues.
void testScales() {
    auto bits = [](std::initializer_list<int> keys) {
        std::uint16_t m = 0;
        for (int k : keys) m |= std::uint16_t(1u << k);
        return m;
    };
    auto fifths = [](std::initializer_list<int> places) {
        std::uint16_t m = 0;
        for (int p : places) m |= std::uint16_t(1u << (((p * 7) % 12 + 12) % 12));
        return m;
    };
    struct Want { const char* name; std::uint16_t keys; };
    const Want want[] = {
        {"None", 0},
        {"Major (Ionian)", fifths({0, 2, 4, -1, 1, 3, 5})},
        {"Dorian", fifths({0, 2, -3, -1, 1, 3, -2})},
        {"Phrygian", fifths({0, -5, -3, -1, 1, -4, -2})},
        {"Lydian", fifths({0, 2, 4, 6, 1, 3, 5})},
        {"Mixolydian", fifths({0, 2, 4, -1, 1, 3, -2})},
        {"Minor (Aeolian)", fifths({0, 2, -3, -1, 1, -4, -2})},
        {"Locrian", fifths({0, -5, -3, -1, -6, -4, -2})},
        {"Melodic minor", fifths({0, 2, -3, -1, 1, 3, 5})},
        {"Dorian b2", fifths({0, -5, -3, -1, 1, 3, -2})},
        {"Lydian augmented", fifths({0, 2, 4, 6, 8, 3, 5})},
        {"Lydian dominant", fifths({0, 2, 4, 6, 1, 3, -2})},
        {"Mixolydian b6", fifths({0, 2, 4, -1, 1, -4, -2})},
        {"Locrian #2", fifths({0, 2, -3, -1, -6, -4, -2})},
        {"Altered", fifths({0, -5, -3, -8, -6, -4, -2})},
        {"Harmonic minor", fifths({0, 2, -3, -1, 1, -4, 5})},
        {"Locrian #6", fifths({0, -5, -3, -1, -6, 3, -2})},
        {"Ionian #5", fifths({0, 2, 4, -1, 8, 3, 5})},
        {"Dorian #4", fifths({0, 2, -3, 6, 1, 3, -2})},
        {"Phrygian dominant", fifths({0, -5, 4, -1, 1, -4, -2})},
        {"Lydian #2", fifths({0, 9, 4, 6, 1, 3, 5})},
        {"Ultralocrian", fifths({0, -5, -3, -8, -6, -4, -9})},
        {"Harmonic major", fifths({0, 2, 4, -1, 1, -4, 5})},
        {"Major pentatonic", fifths({0, 2, 4, 1, 3})},
        {"Minor pentatonic", fifths({0, -3, -1, 1, -2})},
        {"Whole tone", fifths({0, 2, 4, 6, 8, 10})},
        {"Diminished (whole-half)", bits({0, 2, 3, 5, 6, 8, 9, 11})},
        {"Diminished (half-whole)", bits({0, 1, 3, 4, 6, 7, 9, 10})},
        {"Diminished (from the 2nd)", bits({1, 2, 4, 5, 7, 8, 10, 11})},
        {"Blues", bits({0, 3, 5, 6, 7, 10})},
    };
    CHECK(kScaleCount == int(sizeof(want) / sizeof(want[0])));
    for (int i = 0; i < kScaleCount && i < int(sizeof(want) / sizeof(want[0])); ++i) {
        CHECK(std::string(kScales[i].name) == want[i].name);
        CHECK(kScales[i].keys == want[i].keys);
    }
    // The three diminished sets are a half step apart, and between them hold every key twice.
    const std::uint16_t wh = bits({0, 2, 3, 5, 6, 8, 9, 11});
    auto up = [](std::uint16_t m, int n) { return std::uint16_t(((m << n) | (m >> (12 - n))) & 0xFFF); };
    CHECK(up(wh, 1) == bits({0, 1, 3, 4, 6, 7, 9, 10}) && up(wh, 2) == bits({1, 2, 4, 5, 7, 8, 10, 11}) && up(wh, 3) == wh);
}

// The This EDO facts (src/EdoFacts.h), against values worked out by hand and the familiar cases:
// 12 tempers all three commas, 19 and 31 are meantone, 17 and 22 temper none, 16 is mavila.
void testEdoFacts() {
    struct Case {
        int      edo, fifth, large, small;
        Diatonic diatonic;
        bool     meantone, augmented, diminished;
    };
    const Case cases[] = {
        {1,  1,  1,  -2, Diatonic::None,       false, false, false},
        {2,  1,  0,  1,  Diatonic::None,       false, false, false},
        {5,  3,  1,  0,  Diatonic::Pentatonic, true,  false, false},
        {7,  4,  1,  1,  Diatonic::Equal,      true,  false, false},
        {9,  5,  1,  2,  Diatonic::Mavila,     false, true,  false},
        {10, 6,  2,  0,  Diatonic::Pentatonic, false, false, false},
        {12, 7,  2,  1,  Diatonic::Proper,     true,  true,  true},
        {13, 8,  3,  -1, Diatonic::None,       false, false, false},
        {16, 9,  2,  3,  Diatonic::Mavila,     false, false, true},
        {17, 10, 3,  1,  Diatonic::Proper,     false, false, false},
        {19, 11, 3,  2,  Diatonic::Proper,     true,  false, false},
        {22, 13, 4,  1,  Diatonic::Proper,     false, false, false},
        {24, 14, 4,  2,  Diatonic::Proper,     true,  true,  true},
        {31, 18, 5,  3,  Diatonic::Proper,     true,  false, false},
        {53, 31, 9,  4,  Diatonic::Proper,     false, false, false},
        {72, 42, 12, 6,  Diatonic::Proper,     false, false, false},
        {96, 56, 16, 8,  Diatonic::Proper,     false, false, false},   // 223 steps for 5: not 12-EDO×8
    };
    for (const Case& c : cases) {
        const EdoFacts f = edoFacts(c.edo);
        const bool ok = f.edo == c.edo && f.fifth == c.fifth && f.large == c.large && f.small == c.small &&
                        f.diatonic == c.diatonic && f.meantone == c.meantone && f.augmented == c.augmented &&
                        f.diminished == c.diminished;
        CHECK(ok);
        if (!ok) std::fprintf(stderr, "  (%d-EDO: fifth %d, L:s %d:%d)\n", c.edo, f.fifth, f.large, f.small);
    }
    // Five fifths less three octaves and two fifths less an octave: 5L2s spans the octave.
    for (int n = kMinEdo; n <= kMaxEdo; ++n) {
        const EdoFacts f = edoFacts(n);
        CHECK(5 * f.large + 2 * f.small == n);
        CHECK(std::abs(f.fifthCents - 1200.0 * f.fifth / n) < 1e-9);
        CHECK(std::abs(f.fifthError) <= 600.0 / n + 1e-9);   // the nearest step: within half a step
    }
    CHECK(edoFacts(0).edo == kMinEdo && edoFacts(500).edo == kMaxEdo);

    FactRow rows[kFactRows];
    factRows(edoFacts(19), rows);
    CHECK(std::string(rows[0].label) == "Fifth" && std::string(rows[0].value) == "11 steps, 694.7\xc2\xa2");
    CHECK(std::string(rows[1].value) == "-7.2\xc2\xa2");
    CHECK(std::string(rows[2].value) == "3:2");
    CHECK(std::string(rows[3].label) == "Meantone 81/80" && std::string(rows[3].value) == "yes" && rows[3].lit);
    CHECK(std::string(rows[4].value) == "no" && !rows[4].lit);
    factRows(edoFacts(16), rows);
    CHECK(std::string(rows[2].value) == "mavila, 2:3");
    CHECK(std::string(rows[5].value) == "yes" && rows[5].lit);
    factRows(edoFacts(1), rows);
    CHECK(std::string(rows[0].value) == "1 step, 1200.0\xc2\xa2" && std::string(rows[2].value) == "none");
    factRows(edoFacts(53), rows);
    CHECK(std::string(rows[1].value) == "-0.1\xc2\xa2");   // 701.887: rounds to -0.1, not "-0.0"
}

} // namespace

int main() {
    testFills();
    testDefaultPassesThrough();
    testMapping();
    testDialects();
    testHeldNoteKeepsItsPitch();
    testSmallEdoSharedSteps();
    testOutOfRange();
    testScales();
    testEdoFacts();
    if (failures != 0) {
        std::fprintf(stderr, "edospread12-core: %d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
