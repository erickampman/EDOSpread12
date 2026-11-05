#pragma once

// EdoFacts: what an N-EDO does with the diatonic scale, for the editor's This EDO panel
// (docs/design.md, "Editor": This EDO). Pure integer math on N, no CLAP or UMP, so it can move to uw-core
// for KeyboardEDO and NoteGran later.
//
// The EDO's patent val is the nearest whole number of steps to each prime: v2 = N, v3 =
// round(N log2 3), v5 = round(N log2 5). Its fifth is v3 - v2 steps (KeyboardEDO's edoFifth()). A
// chain of fifths makes the 5L2s (diatonic) scale: large step L = 2F - N (two fifths less an
// octave), small step s = 3N - 5F (three octaves less five fifths). A comma, as prime exponents
// (a2, a3, a5), is tempered out (counts as no steps) when a2 v2 + a3 v3 + a5 v5 = 0.

namespace edospread12 {

// What the chain of fifths gives as a 7-note scale.
enum class Diatonic : int {
    Proper,       // L > s > 0: a diatonic scale (12-EDO's 2:1, 19's 3:2, 17's 3:1, ...)
    Equal,        // L = s: seven equal steps (7-EDO and its multiples)
    Pentatonic,   // s = 0: the half steps vanish, five equal steps (5-, 10-, 15-EDO ...)
    Mavila,       // s > L > 0: the fifth is flat of 7-EDO's, so large and small swap (9, 16, 23 ...)
    None,         // L <= 0 or s < 0: a fifth of 600 cents or less, or sharp of 720
};

struct EdoFacts {
    int      edo        = 12;
    int      fifth      = 7;      // steps
    double   fifthCents = 700.0;
    double   fifthError = 0.0;    // cents from 3/2 (701.955), + sharp
    int      large      = 2;      // the 5L2s steps, L and s; meaningful as Diatonic says
    int      small      = 1;
    Diatonic diatonic   = Diatonic::Proper;
    bool     meantone   = true;   // tempers out 81/80, the syntonic comma
    bool     augmented  = true;   // 128/125, the diesis: three major thirds make an octave
    bool     diminished = true;   // 648/625: four minor thirds make an octave
};

// The facts for N-EDO, N clamped to 1 … 96 (the plugin's range).
EdoFacts edoFacts(int edo) noexcept;

// The panel's rows: a label on the left and a value on the right, UTF-8. `lit` marks a value worth
// the accent (a comma tempered out). For 19-EDO:
//   Fifth            11 steps, 694.7¢
//   From 3/2         -7.2¢
//   Diatonic         3:2
//   Meantone 81/80   yes
//   Augmented 128/125   no
//   Diminished 648/625  no
struct FactRow {
    char label[32];
    char value[32];
    bool lit;
};
inline constexpr int kFactRows = 6;
void factRows(const EdoFacts& f, FactRow (&rows)[kFactRows]) noexcept;

} // namespace edospread12
