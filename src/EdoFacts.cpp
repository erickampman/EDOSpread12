#include "EdoFacts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "SpreadCore.h"   // kMinEdo, kMaxEdo

namespace edospread12 {

namespace {

// Steps nearest `ratio` in N-EDO (the patent val's entry for one prime).
int nearestSteps(int edo, double ratio) noexcept {
    return static_cast<int>(std::lround(edo * std::log2(ratio)));
}

// Does the val (v2, v3, v5) send the comma with exponents (a2, a3, a5) to no steps?
bool tempers(int v2, int v3, int v5, int a2, int a3, int a5) noexcept {
    return a2 * v2 + a3 * v3 + a5 * v5 == 0;
}

} // namespace

EdoFacts edoFacts(int edo) noexcept {
    EdoFacts f;
    const int n = std::clamp(edo, kMinEdo, kMaxEdo);
    const int v2 = n, v3 = nearestSteps(n, 3.0), v5 = nearestSteps(n, 5.0);
    f.edo        = n;
    f.fifth      = v3 - v2;
    f.fifthCents = 1200.0 * f.fifth / n;
    f.fifthError = f.fifthCents - 1200.0 * std::log2(1.5);
    f.large      = 2 * f.fifth - n;
    f.small      = 3 * n - 5 * f.fifth;
    if (f.large <= 0 || f.small < 0)  f.diatonic = Diatonic::None;
    else if (f.small == 0)            f.diatonic = Diatonic::Pentatonic;
    else if (f.large == f.small)      f.diatonic = Diatonic::Equal;
    else if (f.large > f.small)       f.diatonic = Diatonic::Proper;
    else                              f.diatonic = Diatonic::Mavila;
    f.meantone   = tempers(v2, v3, v5, -4, 4, -1);   // 81/80 = 3^4 / (2^4 · 5)
    f.augmented  = tempers(v2, v3, v5, 7, 0, -3);    // 128/125 = 2^7 / 5^3
    f.diminished = tempers(v2, v3, v5, 3, 4, -4);    // 648/625 = 2^3 · 3^4 / 5^4
    return f;
}

void factRows(const EdoFacts& f, FactRow (&rows)[kFactRows]) noexcept {
    auto row = [&rows](int i, const char* label, bool lit) -> char* {
        std::snprintf(rows[i].label, sizeof(rows[i].label), "%s", label);
        rows[i].lit = lit;
        return rows[i].value;
    };
    constexpr std::size_t kValue = sizeof(FactRow::value);

    std::snprintf(row(0, "Fifth", false), kValue, "%d step%s, %.1f\xc2\xa2", f.fifth, f.fifth == 1 ? "" : "s",
                  f.fifthCents);
    // Rounded first, so a fifth within 0.05 cents never reads "-0.0".
    const double error = std::round(f.fifthError * 10.0) / 10.0;
    std::snprintf(row(1, "From 3/2", false), kValue, "%+.1f\xc2\xa2", error == 0.0 ? 0.0 : error);

    char* diatonic = row(2, "Diatonic", false);
    switch (f.diatonic) {
    case Diatonic::Proper:     std::snprintf(diatonic, kValue, "%d:%d", f.large, f.small); break;
    case Diatonic::Equal:      std::snprintf(diatonic, kValue, "7 equal steps"); break;
    case Diatonic::Pentatonic: std::snprintf(diatonic, kValue, "5 equal steps"); break;
    case Diatonic::Mavila:     std::snprintf(diatonic, kValue, "mavila, %d:%d", f.large, f.small); break;
    case Diatonic::None:       std::snprintf(diatonic, kValue, "none"); break;
    }

    std::snprintf(row(3, "Meantone 81/80", f.meantone), kValue, "%s", f.meantone ? "yes" : "no");
    std::snprintf(row(4, "Augmented 128/125", f.augmented), kValue, "%s", f.augmented ? "yes" : "no");
    std::snprintf(row(5, "Diminished 648/625", f.diminished), kValue, "%s", f.diminished ? "yes" : "no");
}

} // namespace edospread12
