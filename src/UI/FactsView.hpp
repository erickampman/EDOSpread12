// FactsView.hpp — the This EDO panel's rows (docs/design.md, "Editor": This EDO): what the chosen EDO does
// with the diatonic scale, from EdoFacts (src/EdoFacts.h). A label on the left, its value on the
// right; a comma the EDO tempers out has its value in the accent. A view only: no mouse.

#ifndef EDOSPREAD12_FACTSVIEW_HPP
#define EDOSPREAD12_FACTSVIEW_HPP

#include "NanoVG.hpp"

#include "EdoFacts.h"

START_NAMESPACE_DGL

class FactsView : public NanoSubWidget
{
public:
    explicit FactsView(Widget* parent)
        : NanoSubWidget(parent)
    {
        fHasFont = loadSharedResources();
        setEdo(12);
    }

    void setAccent(Color c) { fAccent = c; }
    void setUiScale(float s) { fUiScale = s; repaint(); }
    void setEdo(int edo)
    {
        if (edo == fEdo) return;
        fEdo = edo;
        edospread12::factRows(edospread12::edoFacts(edo), fRows);
        repaint();
    }

protected:
    void onNanoDisplay() override
    {
        if (!fHasFont) return;
        const float s = fUiScale, w = getWidth(), h = getHeight();
        const float rowH = h / edospread12::kFactRows;
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(11.0f * s);
        for (int i = 0; i < edospread12::kFactRows; ++i)
        {
            const float y = rowH * (i + 0.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(Color(150, 156, 168));
            text(2.0f * s, y, fRows[i].label, nullptr);
            textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
            fillColor(fRows[i].lit ? fAccent : Color(226, 230, 236));
            text(w - 2.0f * s, y, fRows[i].value, nullptr);
        }
    }

private:
    edospread12::FactRow fRows[edospread12::kFactRows] {};
    Color fAccent   = Color(120, 200, 170);
    float fUiScale  = 1.0f;
    int   fEdo      = 0;
    bool  fHasFont  = false;
};

END_NAMESPACE_DGL

#endif // EDOSPREAD12_FACTSVIEW_HPP
