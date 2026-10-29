#include "SpreadCore.h"

#include <algorithm>

namespace edospread12 {

namespace {

// num / den rounded down (den > 0).
int floorDiv(int num, int den) noexcept {
    return num >= 0 ? num / den : -((-num + den - 1) / den);
}

// pitch79() for params already clamped.
int pitch79Clamped(const Params& c, int key) noexcept {
    const int    octave = floorDiv(key - c.origin, kKeys);
    const int    d      = key - c.origin - kKeys * octave;   // 0 … 11
    const int    step   = c.steps[static_cast<std::size_t>(d)];
    // 512 × 12 × step / N, rounded to nearest (never negative).
    return (c.origin + kKeys * octave) * 512 + (2 * 512 * kKeys * step + c.edo) / (2 * c.edo);
}

} // namespace

Table filled(Fill fill, int edo) noexcept {
    const int n = std::clamp(edo, kMinEdo, kMaxEdo);
    Table t{};
    for (int d = 0; d < kKeys; ++d)
        t[static_cast<std::size_t>(d)] = fill == Fill::Floor ? d * n / kKeys
                                                             : (2 * d * n + kKeys) / (2 * kKeys);
    return t;
}

Params clamped(const Params& p) noexcept {
    Params c;
    c.edo    = std::clamp(p.edo, kMinEdo, kMaxEdo);
    c.origin = std::clamp(p.origin, 0, kKeys - 1);
    c.fill   = p.fill == Fill::Floor ? Fill::Floor : Fill::Nearest;
    for (std::size_t d = 0; d < c.steps.size(); ++d) c.steps[d] = std::clamp(p.steps[d], 0, c.edo);
    return c;
}

int pitch79(const Params& p, int key) noexcept { return pitch79Clamped(clamped(p), key); }

void shifts(const Params& p, int (&shift79)[128]) noexcept {
    const Params c = clamped(p);
    for (int key = 0; key < 128; ++key) shift79[key] = pitch79Clamped(c, key) - key * 512;
}

SpreadCore::SpreadCore() noexcept {
    remap_.setPromoteMidi1(true);
    setParams(Params{});
}

void SpreadCore::setParams(const Params& p) noexcept {
    params_ = clamped(p);
    int s[128];
    shifts(params_, s);
    remap_.setShifts(s);
}

} // namespace edospread12
