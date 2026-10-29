// SpreadView.hpp — EDOSpread12's map (docs/design.md, "Editor"): the step circle of the chosen EDO
// above a piano octave from the origin. EDOSpread12UI.cpp hands it the settings on its timer; it
// talks to the plugin only through its Callback.
//
// - The circle: N steps, step 0 (the origin) at the top. Its ring is cut into 12 arcs, one per piano
//   key from the origin, dark for a black key and light for a white one, breaking where Fill puts
//   them (Nearest: each key's 12-TET pitch ±50¢; Floor: from it up to the next key's).
// - Each key's step is a mark on the circle, named; the selected key's is the indicator, which is
//   dragged round in whole steps (past the top from the left is step N, the octave above). Clicking
//   a mark selects its key.
// - The piano: one octave from the origin, laid out straight. A click selects a key; a curve joins
//   it to its indicator. A key out of order or sharing a step is tinted; a held key is lit.
// - Button: a plain push button, for Fill (a refill with the current choice).

#ifndef EDOSPREAD12_SPREADVIEW_HPP
#define EDOSPREAD12_SPREADVIEW_HPP

#include "NanoVG.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

START_NAMESPACE_DGL

namespace spread {

inline Color withAlpha(Color c, float a) { c.alpha = a; return c; }

const Color kBack(20, 22, 26);
const Color kBorder(62, 67, 77);
const Color kDim(96, 102, 114);
const Color kText(176, 182, 190);
const Color kBright(226, 230, 236);
const Color kWhiteArc(150, 156, 168);
const Color kBlackArc(44, 47, 55);
const Color kWhiteKey(214, 218, 226);
const Color kBlackKey(38, 41, 48);
const Color kWarn(225, 160, 70);   // a key out of order, or sharing a step

const char* const kNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

inline bool isBlack(int pitchClass)
{
    const int pc = ((pitchClass % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

} // namespace spread

// --- Button ----------------------------------------------------------------------------------------

class Button : public NanoSubWidget
{
public:
    class Callback
    {
    public:
        virtual ~Callback() {}
        virtual void buttonClicked(Button* button) = 0;
    };

    Button(Widget* parent, Callback* callback) : NanoSubWidget(parent), fCallback(callback)
    {
        fHasFont = loadSharedResources();
    }

    void setLabel(const std::string& label) { fLabel = label; }
    void setAccentColor(Color c) { fAccent = c; }
    void setUiScale(float s) { fUiScale = s; }

protected:
    void onNanoDisplay() override
    {
        using namespace spread;
        const float s = fUiScale, w = getWidth(), h = getHeight();
        const float bh = std::min(h - 4.0f, 26.0f * s), by = (h - bh) / 2.0f, inset = 5.0f * s;
        beginPath(); roundedRect(inset, by, w - 2.0f * inset, bh, 5.0f * s);
        fillColor(fDown ? withAlpha(fAccent, 0.45f) : Color(48, 52, 60)); fill();
        strokeColor(fAccent); strokeWidth(1.0f * s); stroke();
        if (!fHasFont) return;
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(12.0f * s);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(kBright);
        text(w / 2.0f, by + bh / 2.0f, fLabel.c_str(), nullptr);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        if (ev.press && contains(ev.pos)) { fDown = true; repaint(); return true; }
        if (!ev.press && fDown)
        {
            fDown = false;
            repaint();
            if (contains(ev.pos)) fCallback->buttonClicked(this);
            return true;
        }
        return false;
    }

private:
    Callback*   fCallback;
    std::string fLabel;
    Color       fAccent  = Color(120, 200, 170);
    float       fUiScale = 1.0f;
    bool        fDown    = false;
    bool        fHasFont = false;
};

// --- SpreadView ------------------------------------------------------------------------------------

class SpreadView : public NanoSubWidget
{
public:
    class Callback
    {
    public:
        virtual ~Callback() {}
        virtual void keySelected(int degree) = 0;              // 0 … 11, from the origin
        virtual void stepDragStarted(int degree) = 0;
        virtual void stepChanged(int degree, int step) = 0;   // 0 … N
        virtual void stepDragFinished(int degree) = 0;
    };

    SpreadView(Widget* parent, Callback* callback) : NanoSubWidget(parent), fCallback(callback)
    {
        fHasFont = loadSharedResources();
    }

    void setAccent(Color c) { fAccent = c; }
    void setUiScale(float s) { fUiScale = s; repaint(); }

    // The settings: N, the origin's pitch class, Fill (0 Floor, 1 Nearest), and each key's step
    // from the origin up (clamped here to 0 … N, as the plugin does).
    void setModel(int edo, int origin, int fill, const std::array<int, 12>& steps)
    {
        fEdo = std::max(edo, 1);
        fOrigin = ((origin % 12) + 12) % 12;
        fFill = fill;
        for (int d = 0; d < 12; ++d) fSteps[d] = std::clamp(steps[d], 0, fEdo);
        repaint();
    }
    // Which keys of the octave (from the origin) are held, on any octave.
    void setHeld(const std::array<bool, 12>& held)
    {
        if (held == fHeld) return;
        fHeld = held;
        repaint();
    }
    void setSelected(int degree) { fSelected = std::clamp(degree, 0, 11); repaint(); }
    int  selected() const { return fSelected; }
    bool dragging() const { return fDragging; }

protected:
    void onNanoDisplay() override
    {
        using namespace spread;
        const float w = getWidth(), h = getHeight(), s = fUiScale;
        beginPath(); roundedRect(1.5f, 1.5f, w - 3.0f, h - 3.0f, 8.0f);
        fillColor(kBack); fill();
        strokeColor(kBorder); strokeWidth(1.5f); stroke();
        layout();

        // The ring: 12 arcs, a key each, dark or light as the key is on a piano.
        const float ro = fR, ri = fR - 14.0f * s;
        const float shift = fFill == 1 ? -0.5f : 0.0f;   // Nearest: centred on the key's pitch
        for (int d = 0; d < 12; ++d)
        {
            // A thick stroke, not a filled ring segment: without a stencil buffer NanoVG fills a
            // concave path's whole bounding box. A small gap keeps the keys apart.
            const float gap = 0.0025f;
            const float a0 = angle((d + shift) / 12.0f + gap), a1 = angle((d + 1 + shift) / 12.0f - gap);
            beginPath();
            arc(fCx, fCy, (ro + ri) / 2.0f, a0, a1, CW);
            lineCap(BUTT);
            strokeColor(isBlack(fOrigin + d) ? kBlackArc : kWhiteArc);
            strokeWidth(ro - ri);
            stroke();
            if (fHasFont)
            {
                float x, y; point(ro + 11.0f * s, (d + shift + 0.5f) / 12.0f, x, y);
                fontFace(NANOVG_DEJAVU_SANS_TTF);
                fontSize(10.5f * s);
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(kDim);
                text(x, y, kNames[(fOrigin + d) % 12], nullptr);
            }
        }

        // The N steps, ticked inside the ring; step 0 (the origin) longer.
        const float t0 = ri - 2.0f * s;
        for (int k = 0; k < fEdo; ++k)
        {
            const float len = (k == 0 ? 10.0f : 5.0f) * s;
            float x0, y0, x1, y1;
            point(t0, k / float(fEdo), x0, y0);
            point(t0 - len, k / float(fEdo), x1, y1);
            beginPath(); moveTo(x0, y0); lineTo(x1, y1);
            strokeColor(k == 0 ? kText : kDim); strokeWidth((k == 0 ? 1.6f : 1.0f) * s); stroke();
        }

        // The selected key's curve, from the piano up to its indicator (under the marks).
        float ix, iy; markPoint(fSelected, ix, iy);
        float kx, ky, kw, kh; keyRect(fSelected, kx, ky, kw, kh);
        const float sx = kx + kw / 2.0f, sy = ky;
        beginPath();
        moveTo(sx, sy);
        bezierTo(sx, sy - 70.0f * s, ix + (fCx - ix) * 0.35f, iy + (fCy - iy) * 0.35f + 40.0f * s, ix, iy);
        strokeColor(withAlpha(fAccent, 0.75f)); strokeWidth(2.0f * s); stroke();

        // Every key's mark; the selected one last, as the indicator.
        for (int d = 0; d < 12; ++d)
            if (d != fSelected) drawMark(d, false);
        drawMark(fSelected, true);

        drawPiano();
        drawReadout();
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        const float x = static_cast<float>(ev.pos.getX()), y = static_cast<float>(ev.pos.getY());
        if (!ev.press)
        {
            if (!fDragging) return false;
            fDragging = false;
            fCallback->stepDragFinished(fSelected);
            repaint();
            return true;
        }
        if (!contains(ev.pos)) return false;
        const int key = keyAt(x, y);
        if (key >= 0)
        {
            fSelected = key;
            fCallback->keySelected(key);
            repaint();
            return true;
        }
        // A key's mark selects that key, as its piano key would, and can then be dragged; it stays
        // on its step until the pointer moves.
        const int mark = markAt(x, y);
        if (mark >= 0)
        {
            if (mark != fSelected)
            {
                fSelected = mark;
                fCallback->keySelected(mark);
                repaint();
            }
            fDragging = true;
            fCallback->stepDragStarted(fSelected);
            return true;
        }
        // Elsewhere on the ring: the selected key's step follows the pointer.
        const float r = std::hypot(x - fCx, y - fCy);
        if (r >= fMarkR - 22.0f * fUiScale && r <= fR + 8.0f * fUiScale)
        {
            fDragging = true;
            fCallback->stepDragStarted(fSelected);
            dragTo(x, y);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (!fDragging) return false;
        dragTo(static_cast<float>(ev.pos.getX()), static_cast<float>(ev.pos.getY()));
        return true;
    }

private:
    // Clockwise from the top, as a fraction f of the octave.
    static float angle(float f) { return -static_cast<float>(M_PI) / 2.0f + 2.0f * static_cast<float>(M_PI) * f; }
    void point(float r, float f, float& x, float& y) const
    {
        x = fCx + r * std::cos(angle(f));
        y = fCy + r * std::sin(angle(f));
    }
    void markPoint(int d, float& x, float& y) const { point(fMarkR, fSteps[d] / float(fEdo), x, y); }

    void layout()
    {
        const float w = getWidth(), h = getHeight(), s = fUiScale;
        fTextTop = 12.0f * s;
        fPianoH  = std::min(h * 0.24f, 120.0f * s);
        fPianoY  = h - fPianoH - 14.0f * s;
        fPianoW  = std::min(w - 28.0f * s, 520.0f * s);
        fPianoX  = (w - fPianoW) / 2.0f;
        const float top = fTextTop + 46.0f * s, bottom = fPianoY - 28.0f * s;
        fR     = std::max(30.0f * s, std::min((bottom - top) / 2.0f - 14.0f * s, w / 2.0f - 40.0f * s));
        fCx    = w / 2.0f;
        fCy    = (top + bottom) / 2.0f;
        fMarkR = fR - 34.0f * s;
    }

    // The piano, from the origin: white keys in a row, black keys over their boundaries. Positions
    // in white-key units; a black key at either end gets half a unit of room.
    void keyUnits(int d, float& centre, bool& black, float& total) const
    {
        float u = spread::isBlack(fOrigin) ? 0.5f : 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            const bool b = spread::isBlack(fOrigin + i);
            if (i == d) { black = b; centre = b ? u : u + 0.5f; }
            if (!b) u += 1.0f;
        }
        total = u + (spread::isBlack(fOrigin + 11) ? 0.5f : 0.0f);
    }
    void keyRect(int d, float& x, float& y, float& w, float& h) const
    {
        float centre, total; bool black;
        keyUnits(d, centre, black, total);
        const float unit = fPianoW / total;
        w = black ? unit * 0.62f : unit;
        h = black ? fPianoH * 0.6f : fPianoH;
        x = fPianoX + centre * unit - w / 2.0f;
        y = fPianoY;
    }
    int keyAt(float px, float py) const
    {
        for (int pass = 0; pass < 2; ++pass)   // black keys first: they're on top
            for (int d = 0; d < 12; ++d)
            {
                if (spread::isBlack(fOrigin + d) != (pass == 0)) continue;
                float x, y, w, h; keyRect(d, x, y, w, h);
                if (px >= x && px <= x + w && py >= y && py <= y + h) return d;
            }
        return -1;
    }
    // The mark under the pointer, nearest first; on a shared step the selected key's (it's on top).
    int markAt(float px, float py) const
    {
        int   best  = -1;
        float bestD = 0.0f;
        for (int d = 0; d < 12; ++d)
        {
            float x, y; markPoint(d, x, y);
            const float dist = std::hypot(px - x, py - y);
            if (dist > (d == fSelected ? 14.0f : 10.0f) * fUiScale) continue;
            if (best < 0 || dist < bestD || (dist == bestD && d == fSelected)) { best = d; bestD = dist; }
        }
        return best;
    }

    // Out of order (below the key before it, or above the key after it), or on another key's step.
    bool warned(int d) const
    {
        if (d > 0 && fSteps[d] < fSteps[d - 1]) return true;
        if (d < 11 && fSteps[d] > fSteps[d + 1]) return true;
        for (int e = 0; e < 12; ++e)
            if (e != d && fSteps[e] == fSteps[d]) return true;
        return false;
    }

    void drawMark(int d, bool indicator)
    {
        using namespace spread;
        const float s = fUiScale;
        float x, y; markPoint(d, x, y);
        const float r = (indicator ? 11.0f : 7.5f) * s;
        beginPath(); circle(x, y, r);
        if (indicator) fillColor(fAccent);
        else if (fHeld[d]) fillColor(withAlpha(fAccent, 0.85f));
        else fillColor(isBlack(fOrigin + d) ? Color(70, 75, 86) : Color(120, 126, 138));
        fill();
        if (indicator || fHeld[d]) { strokeColor(kBright); strokeWidth(1.5f * s); stroke(); }
        if (!fHasFont) return;
        // The name, inward of the mark. Keys sharing a step stack their names along the radius,
        // the selected key's innermost.
        int before = 0;
        for (int e = 0; e < 12; ++e)
            if (e != d && e != fSelected && fSteps[e] == fSteps[d] && (indicator || e < d)) ++before;
        float lx, ly;
        point(fMarkR - (22.0f + 12.0f * before + (indicator ? 3.0f : 0.0f)) * s, fSteps[d] / float(fEdo), lx, ly);
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize((indicator ? 12.5f : 10.0f) * s);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(indicator ? kBright : kText);
        text(lx, ly, kNames[(fOrigin + d) % 12], nullptr);
    }

    void drawPiano()
    {
        using namespace spread;
        const float s = fUiScale;
        for (int pass = 0; pass < 2; ++pass)
            for (int d = 0; d < 12; ++d)
            {
                const bool black = isBlack(fOrigin + d);
                if (black != (pass == 1)) continue;
                float x, y, w, h; keyRect(d, x, y, w, h);
                beginPath(); roundedRect(x + 0.5f, y, w - 1.0f, h, 3.0f * s);
                Color c = black ? kBlackKey : kWhiteKey;
                if (fHeld[d]) c = black ? withAlpha(fAccent, 0.9f) : fAccent;
                fillColor(c); fill();
                strokeColor(kBack); strokeWidth(1.0f * s); stroke();
                if (warned(d))
                {
                    beginPath(); rect(x + 2.0f * s, y + h - 7.0f * s, w - 4.0f * s, 4.0f * s);
                    fillColor(kWarn); fill();
                }
                if (d == fSelected)
                {
                    beginPath(); roundedRect(x + 1.5f * s, y + 1.5f * s, w - 3.0f * s, h - 3.0f * s, 3.0f * s);
                    strokeColor(fAccent); strokeWidth(2.5f * s); stroke();
                }
                if (!fHasFont) continue;
                fontFace(NANOVG_DEJAVU_SANS_TTF);
                fontSize((black ? 9.5f : 11.0f) * s);
                textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
                fillColor(black ? kText : Color(60, 64, 72));
                text(x + w / 2.0f, y + h - 10.0f * s, kNames[(fOrigin + d) % 12], nullptr);
            }
    }

    // "C → step 5 of 19 · 316¢ · +16¢", and what's odd about the key.
    void drawReadout()
    {
        using namespace spread;
        if (!fHasFont) return;
        const float s = fUiScale;
        const int   d = fSelected, step = fSteps[d];
        const double cents = 1200.0 * step / fEdo, offset = cents - 100.0 * d;
        char line[160];
        if (step == fEdo)
            std::snprintf(line, sizeof(line), "%s \xe2\x86\x92 step %d of %d (the %s above) \xc2\xb7 %.0f\xc2\xa2 \xc2\xb7 %+.0f\xc2\xa2",
                          kNames[(fOrigin + d) % 12], step, fEdo, kNames[fOrigin], cents, offset);
        else
            std::snprintf(line, sizeof(line), "%s \xe2\x86\x92 step %d of %d \xc2\xb7 %.0f\xc2\xa2 \xc2\xb7 %+.0f\xc2\xa2",
                          kNames[(fOrigin + d) % 12], step, fEdo, cents, offset);
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fontSize(14.0f * s);
        fillColor(kBright);
        text(14.0f * s, fTextTop, line, nullptr);

        std::string shares;
        for (int e = 0; e < 12; ++e)
            if (e != d && fSteps[e] == step)
                shares += std::string(shares.empty() ? "shares its step with " : ", ") + kNames[(fOrigin + e) % 12];
        const bool  outOfOrder = (d > 0 && step < fSteps[d - 1]) || (d < 11 && step > fSteps[d + 1]);
        std::string note       = outOfOrder ? "Out of order" : "";
        if (!shares.empty()) note += note.empty() ? "S" + shares.substr(1) : "; " + shares;
        const bool hint = note.empty();
        if (hint) note = "Click a key, then drag its marker round the circle";
        fontSize(11.0f * s);
        fillColor(hint ? kDim : kWarn);
        text(14.0f * s, fTextTop + 20.0f * s, note.c_str(), nullptr);
    }

    void dragTo(float x, float y)
    {
        // The fraction of the octave clockwise from the top; rounding up to N is just left of the top.
        float f = (std::atan2(y - fCy, x - fCx) + static_cast<float>(M_PI) / 2.0f) / (2.0f * static_cast<float>(M_PI));
        f -= std::floor(f);
        const int step = std::clamp(static_cast<int>(std::lround(f * fEdo)), 0, fEdo);
        if (step == fSteps[fSelected]) return;
        fSteps[fSelected] = step;
        fCallback->stepChanged(fSelected, step);
        repaint();
    }

    Callback*            fCallback;
    Color                fAccent   = Color(120, 200, 170);
    float                fUiScale  = 1.0f;
    bool                 fHasFont  = false;
    int                  fEdo      = 12;
    int                  fOrigin   = 9;
    int                  fFill     = 1;
    std::array<int, 12>  fSteps{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    std::array<bool, 12> fHeld{};
    int                  fSelected = 0;
    bool                 fDragging = false;
    // Geometry, from layout().
    float fCx = 0, fCy = 0, fR = 100, fMarkR = 70, fTextTop = 12;
    float fPianoX = 0, fPianoY = 0, fPianoW = 100, fPianoH = 60;
};

END_NAMESPACE_DGL

#endif // EDOSPREAD12_SPREADVIEW_HPP
