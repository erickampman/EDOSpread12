// SpreadCore tests: the fills, the mapping, and notes through it in each dialect (docs/design.md,
// phase 1). The latch itself is uw-core's NoteRemap and ClapNoteRemap, tested there. Silent unless
// a check fails.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

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

} // namespace

int main() {
    testFills();
    testDefaultPassesThrough();
    testMapping();
    testDialects();
    testHeldNoteKeepsItsPitch();
    testSmallEdoSharedSteps();
    testOutOfRange();
    if (failures != 0) {
        std::fprintf(stderr, "edospread12-core: %d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
