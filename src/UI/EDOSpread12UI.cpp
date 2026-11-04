// EDOSpread12UI.cpp — EDOSpread12's editor (docs/design.md, "Editor"), DPF + NanoVG, after NoteGran's.
//
// The header (the Unlikelyware logo and "EDOSpread12"), the map (SpreadView.hpp: the step circle
// above a piano octave from the origin), and a Tuning group of the house widget kit (Knob,
// Dropdown, GroupBox, PopupMenu, copied from NoteGran's): EDO, Origin, Fill, and a Fill button. Help,
// at the header's right, opens the help page (resources/help.md) in the browser.
//
// Choosing a Fill, or turning EDO, here refills all 12 keys from the Fill, as one gesture with the
// change (docs/design.md, "Filling the table"); host automation of EDO doesn't. The Fill button
// refills with the current choice, after hand edits. A key's marker dragged round the circle sets
// that key's step. The editor reads every value on its timer and shows what changed, so automation
// and a loaded state need no push. Edits go out through DPF's editParameter / setParameterValue,
// which the UIExporter hands to the seam (plugin_gui.h) as gestures and values.

#include "DistrhoUI.hpp"

#include "src/DistrhoUIInternal.hpp"   // DPF's UIExporter (DPF_ROOT_DIR/distrho is on the include path)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Dropdown.hpp"
#include "HelpButton.hpp"
#include "EmbeddedResources.hpp"   // build-time: the logo's bytes
#include "GroupBox.hpp"
#include "Knob.hpp"
#include "Logo.hpp"
#include "PopupMenu.hpp"
#include "SpreadCore.h"
#include "SpreadView.hpp"
#include "plugin_gui.h"

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Button;
using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::HelpButton;
using DGL_NAMESPACE::Dropdown;
using DGL_NAMESPACE::GroupBox;
using DGL_NAMESPACE::Knob;
using DGL_NAMESPACE::Logo;
using DGL_NAMESPACE::PopupMenu;
using DGL_NAMESPACE::SpreadView;

namespace es = edospread12;

namespace {

// --- the look ----------------------------------------------------------------------------------

const Color kAccent(120, 200, 170);   // EDOSpread12's own: sea green
const Color kWindow(26, 28, 33);
const Color kTitle(226, 230, 236);

// The design size; the editor scales from it, keeping its shape.
constexpr int kWidth = DISTRHO_UI_DEFAULT_WIDTH, kHeight = DISTRHO_UI_DEFAULT_HEIGHT;

// Parameter ids (src/plugin.cpp).
enum : uint32_t { kEdo = 0, kOrigin = 1, kFill = 2, kStep0 = 3, kParamCount = 15 };

// The plugin whose UIExporter is being built, for the editor's constructor (gui_set_parent).
const clap_plugin_t* gCreatingFor = nullptr;

// --- the editor --------------------------------------------------------------------------------

class EDOSpread12UI : public UI, public Knob::Callback, public Dropdown::Callback, public PopupMenu::Callback,
                      public Button::Callback, public SpreadView::Callback {
public:
    EDOSpread12UI() : UI(kWidth, kHeight) {
        fPlugin = static_cast<const clap_plugin_t*>(getPluginInstancePointer());
        if (fPlugin == nullptr) fPlugin = gCreatingFor;
        setGeometryConstraints(kWidth * 4 / 5, kHeight * 4 / 5, true, false);   // from 0.8×, keeping its shape
        fHasFont = loadSharedResources();

        fLogo = std::make_unique<Logo>(this, EmbeddedResources::Unlikelyware_Final_ColorData,
                                       EmbeddedResources::Unlikelyware_Final_ColorDataSize);
        fView = std::make_unique<SpreadView>(this, this);
        fView->setAccent(kAccent);

        fGroup = std::make_unique<GroupBox>(this);
        fGroup->setTitle("Tuning");
        fGroup->setAccentColor(kAccent);
        fGroup->setColumns(1);

        const clap_plugin_t* plugin = fPlugin;
        auto format = [plugin](uint32_t id) {
            return [plugin, id](float v) {
                char text[64];
                es_param_text(plugin, id, v, text, sizeof(text));
                return std::string(text);
            };
        };
        fEdoKnob = std::make_unique<Knob>(fGroup.get(), this);
        fEdoKnob->setId(kEdo);
        fEdoKnob->setLabel("EDO");
        fEdoKnob->setAccentColor(kAccent);
        fEdoKnob->configure(es::kMinEdo, es::kMaxEdo, false, 12);
        fEdoKnob->setInteger(true);
        fEdoKnob->setFormatter(format(kEdo));
        fGroup->addControl(fEdoKnob.get());

        es_param_info_t info{};
        for (uint32_t id : {kOrigin, kFill}) {
            auto d = std::make_unique<Dropdown>(fGroup.get(), this);
            d->setId(id);
            d->setLabel(id == kOrigin ? "Origin" : "Fill");
            std::vector<std::string> choices;
            if (fPlugin != nullptr && es_param_info(fPlugin, id, &info) && info.labels != nullptr)
                for (int i = 0; i <= static_cast<int>(info.max - info.min); ++i) choices.emplace_back(info.labels[i]);
            d->setChoices(choices);
            fGroup->addControl(d.get());
            (id == kOrigin ? fOriginMenu : fFillMenu) = std::move(d);
        }
        fFillButton = std::make_unique<Button>(fGroup.get(), this);
        fFillButton->setLabel("Fill all keys");
        fFillButton->setAccentColor(kAccent);
        fGroup->addControl(fFillButton.get());

        // help.html shows the logo as "Unlikelyware.png", so it is written out under that name.
        fHelp = std::make_unique<HelpButton>(this, std::vector<HelpButton::File>{
            {"EDOSpread12-help.html", EmbeddedResources::helpData, EmbeddedResources::helpDataSize},
            {"Unlikelyware.png", EmbeddedResources::Unlikelyware_Final_ColorData,
             EmbeddedResources::Unlikelyware_Final_ColorDataSize}});
        fMenu = std::make_unique<PopupMenu>(this, this);   // last, so it draws on top

        refresh(true);
        layout();
    }

    // A control is a child of its group's box, and a DGL child unlinks itself from its parent as it
    // goes: so the controls go first, then the box.
    ~EDOSpread12UI() override {
        fMenu.reset();
        fEdoKnob.reset();
        fOriginMenu.reset();
        fFillMenu.reset();
        fFillButton.reset();
        fGroup.reset();
    }

protected:
    // Values are read on the timer (uiIdle); this only keeps a pushed value from being stale.
    void parameterChanged(uint32_t index, float value) override {
        if (index < kParamCount && !fEdoDragging) {
            fValues[index] = value;
            show();
        }
    }

    // --- EDO: a turn refills the keys, all in the one gesture ---
    void knobDragStarted(Knob*) override {
        fEdoDragging = true;
        editParameter(kEdo, true);
        for (uint32_t d = 0; d < 12; ++d) editParameter(kStep0 + d, true);
    }
    void knobValueChanged(Knob*, float v) override {
        const int edo = static_cast<int>(std::lround(v));
        if (edo == static_cast<int>(std::lround(fValues[kEdo]))) return;
        set(kEdo, edo);
        refill(edo, fill());
        show();
    }
    void knobDragFinished(Knob*) override {
        for (uint32_t d = 0; d < 12; ++d) editParameter(kStep0 + d, false);
        editParameter(kEdo, false);
        fEdoDragging = false;
    }

    // --- Origin and Fill ---
    void dropdownClicked(Dropdown* d) override {
        int x, y, w, h;
        d->boxRect(x, y, w, h);
        fMenu->openFor(d->getId(), d->choices(), d->index(), x, y + h, y, w, static_cast<int>(getHeight()));
    }
    void menuSelected(PopupMenu* menu, int index) override {
        if (menu->tag() == kOrigin) {
            editParameter(kOrigin, true);
            set(kOrigin, index);
            editParameter(kOrigin, false);
        } else if (menu->tag() == kFill) {
            editParameter(kFill, true);
            for (uint32_t d = 0; d < 12; ++d) editParameter(kStep0 + d, true);
            set(kFill, index);
            refill(edo(), index == 0 ? es::Fill::Floor : es::Fill::Nearest);
            for (uint32_t d = 0; d < 12; ++d) editParameter(kStep0 + d, false);
            editParameter(kFill, false);
        }
        show();
    }
    // A click outside the window never reaches us: losing focus closes an open menu.
    void uiFocus(bool focus, DGL_NAMESPACE::CrossingMode mode) override {
        UI::uiFocus(focus, mode);
        if (!focus && fMenu != nullptr) fMenu->dismiss();
    }

    // --- the Fill button: the current Fill again, over hand edits ---
    void buttonClicked(Button*) override {
        for (uint32_t d = 0; d < 12; ++d) editParameter(kStep0 + d, true);
        refill(edo(), fill());
        for (uint32_t d = 0; d < 12; ++d) editParameter(kStep0 + d, false);
        show();
    }

    // --- the map ---
    void keySelected(int) override {}
    void stepDragStarted(int d) override { editParameter(kStep0 + static_cast<uint32_t>(d), true); }
    void stepChanged(int d, int step) override { set(kStep0 + static_cast<uint32_t>(d), step); }
    void stepDragFinished(int d) override { editParameter(kStep0 + static_cast<uint32_t>(d), false); }

    // The timer: values, and the keys held.
    void uiIdle() override {
        if (!fInitialSizeNudged) {   // some Linux hosts apply the first size only once events run
            fInitialSizeNudged = true;
            setSize(getWidth(), getHeight());
        }
        refresh(false);
        if (fPlugin == nullptr) return;
        uint64_t words[2];
        es_held_keys(fPlugin, words);
        std::array<bool, 12> held{};
        const int origin = static_cast<int>(std::lround(fValues[kOrigin]));
        for (int k = 0; k < 128; ++k)
            if ((words[k / 64] >> (k % 64)) & 1) held[static_cast<std::size_t>(((k - origin) % 12 + 12) % 12)] = true;
        fView->setHeld(held);
    }

    void onNanoDisplay() override {
        const float w = getWidth(), h = getHeight();
        beginPath();
        rect(0, 0, w, h);
        fillColor(kWindow);
        fill();
        if (!fHasFont) return;
        const float s = scale();
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fontSize(22 * s);
        fillColor(kTitle);
        const float after = text(fTitleX, 22 * s, "EDO", nullptr);
        fillColor(kAccent);
        text(after, 22 * s, "Spread12", nullptr);
    }
    void onResize(const ResizeEvent& ev) override {
        UI::onResize(ev);
        layout();
    }

private:
    float scale() const { return getWidth() / static_cast<float>(kWidth); }
    int   edo() const { return static_cast<int>(std::lround(fValues[kEdo])); }
    es::Fill fill() const { return std::lround(fValues[kFill]) == 0 ? es::Fill::Floor : es::Fill::Nearest; }

    // A value to the plugin, inside a gesture the caller opened.
    void set(uint32_t id, double v) {
        fValues[id] = v;
        setParameterValue(id, static_cast<float>(v));
    }

    // The 12 steps `fill` gives for N-EDO.
    void refill(int n, es::Fill f) {
        const es::Table t = es::filled(f, n);
        for (uint32_t d = 0; d < 12; ++d) set(kStep0 + d, t[d]);
    }

    // Every value from the plugin; shown if anything changed (or `all`). A drag in progress keeps
    // what it says.
    void refresh(bool all) {
        if (fPlugin == nullptr) return;
        bool changed = all;
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (fEdoDragging && (i == kEdo || i >= kStep0)) continue;
            if (fView->dragging() && i == kStep0 + static_cast<uint32_t>(fView->selected())) continue;
            const double v = es_param_value(fPlugin, i);
            if (v != fValues[i]) changed = true;
            fValues[i] = v;
        }
        if (changed) show();
    }

    void show() {
        if (!fEdoDragging) fEdoKnob->setRealValue(static_cast<float>(fValues[kEdo]), false), fEdoKnob->repaint();
        fOriginMenu->setIndex(static_cast<int>(std::lround(fValues[kOrigin])));
        fFillMenu->setIndex(static_cast<int>(std::lround(fValues[kFill])));
        std::array<int, 12> steps{};
        for (uint32_t d = 0; d < 12; ++d) steps[d] = static_cast<int>(std::lround(fValues[kStep0 + d]));
        fView->setModel(edo(), static_cast<int>(std::lround(fValues[kOrigin])), static_cast<int>(std::lround(fValues[kFill])),
                        steps);
    }

    void layout() {
        const int   W = static_cast<int>(getWidth()), H = static_cast<int>(getHeight());
        const float s = scale();
        const int   M = static_cast<int>(14 * s), gap = static_cast<int>(10 * s), hdr = static_cast<int>(46 * s);

        fLogo->setHeightPreservingAspect(22 * s);
        fLogo->setAbsolutePos(M, static_cast<int>(11 * s));
        fTitleX = static_cast<float>(M + static_cast<int>(fLogo->getWidth()) + static_cast<int>(12 * s));
        fMenu->setUiScale(s);
        const int helpW = static_cast<int>(26 * s);   // Help at the header's right
        fHelp->setUiScale(s);
        fHelp->setAbsolutePos(W - M - helpW, static_cast<int>(9 * s));
        fHelp->setSize(static_cast<uint>(helpW), static_cast<uint>(26 * s));

        const int sideW = static_cast<int>(210 * s);
        const int viewW = W - 2 * M - gap - sideW, bodyH = H - hdr - M;
        fView->setAbsolutePos(M, hdr);
        fView->setSize(static_cast<uint>(viewW), static_cast<uint>(bodyH));
        fView->setUiScale(s);

        fGroup->setAbsolutePos(M + viewW + gap, hdr);
        fGroup->setSize(static_cast<uint>(sideW), static_cast<uint>(std::min(bodyH, static_cast<int>(430 * s))));
        fGroup->setUiScale(s);
        fGroup->relayout();
        const float d = std::min({70.0f * s, static_cast<float>(fEdoKnob->getHeight()) - 38.0f * s,
                                  static_cast<float>(fEdoKnob->getWidth()) - 6.0f});
        fEdoKnob->setDiameter(std::max(d, 16.0f), s);
        fOriginMenu->setUiScale(s);
        fFillMenu->setUiScale(s);
        fFillButton->setUiScale(s);
    }

    const clap_plugin_t*        fPlugin = nullptr;
    std::array<double, kParamCount> fValues{};
    std::unique_ptr<Logo>       fLogo;
    std::unique_ptr<SpreadView> fView;
    std::unique_ptr<GroupBox>   fGroup;
    std::unique_ptr<Knob>       fEdoKnob;
    std::unique_ptr<Dropdown>   fOriginMenu, fFillMenu;
    std::unique_ptr<Button>     fFillButton;
    std::unique_ptr<HelpButton> fHelp;
    std::unique_ptr<PopupMenu>  fMenu;
    bool                        fEdoDragging       = false;
    bool                        fHasFont           = false;
    bool                        fInitialSizeNudged = false;
    float                       fTitleX            = 16.0f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EDOSpread12UI)
};

} // namespace

UI* createUI() { return new EDOSpread12UI(); }

// ================================================================================================
// CLAP `gui` extension, as NoteGran's: gui_create() only makes a holder, since UIExporter can't be
// built until the host gives a parent window in set_parent().
// ================================================================================================

namespace {

struct GuiState {
    const clap_plugin_t*        plugin = nullptr;
    std::unique_ptr<UIExporter> exporter;
    uintptr_t                   parentWindow = 0;
    // The size in the host's units (points on macOS, pixels on X11): the source of truth, from
    // which the UIExporter's pixel size is derived at the live scale.
    uint32_t width           = kWidth;
    uint32_t height          = kHeight;
    double   appliedScale    = 0.0;
    bool     timerRegistered = false;
    clap_id  timerId         = 0;
};

#ifdef __APPLE__
extern "C" double es_gui_backing_scale(uintptr_t nsView);   // BackingScale_mac.mm
#endif

// Host units → pixels: on macOS the backing scale of the window the editor is in now (DPF caches
// its own once, maybe from the wrong screen); on X11 the sizes are pixels already.
double liveScale(const GuiState* gui) {
#ifdef __APPLE__
    return es_gui_backing_scale(gui->parentWindow);
#else
    (void)gui;
    return 1.0;
#endif
}

void applySize(GuiState* gui) {
    if (!gui->exporter) return;
    const double scale = liveScale(gui);
    gui->appliedScale  = scale;
    gui->exporter->setWindowSizeFromHost(uint(std::lround(gui->width * scale)), uint(std::lround(gui->height * scale)));
}

#ifdef __APPLE__
constexpr const char* kWindowApi = CLAP_WINDOW_API_COCOA;
#else
constexpr const char* kWindowApi = CLAP_WINDOW_API_X11;
#endif

const clap_host_timer_support_t* hostTimer(const clap_plugin_t* plugin) {
    const clap_host_t* host = es_plugin_host(plugin);
    return static_cast<const clap_host_timer_support_t*>(host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
}

bool gui_is_api_supported(const clap_plugin_t*, const char* api, bool isFloating) {
    return !isFloating && std::strcmp(api, kWindowApi) == 0;
}

bool gui_get_preferred_api(const clap_plugin_t*, const char** api, bool* isFloating) {
    *api        = kWindowApi;
    *isFloating = false;
    return true;
}

bool gui_create(const clap_plugin_t* plugin, const char* api, bool isFloating) {
    if (!gui_is_api_supported(plugin, api, isFloating)) return false;
    auto* gui   = new GuiState();
    gui->plugin = plugin;
    es_plugin_set_gui(plugin, gui);
    return true;
}

void gui_destroy(const clap_plugin_t* plugin) {
    auto* gui = static_cast<GuiState*>(es_plugin_get_gui(plugin));
    es_plugin_set_gui(plugin, nullptr);
    if (gui == nullptr) return;
    if (gui->timerRegistered)
        if (const auto* timer = hostTimer(plugin)) timer->unregister_timer(es_plugin_host(plugin), gui->timerId);
    delete gui;   // the UIExporter, and the editor with it
}

// The UIExporter's callbacks; `ptr` is the clap_plugin_t it was built with.
void onEditParam(void* ptr, uint32_t index, bool started) {
    const auto* plugin = static_cast<const clap_plugin_t*>(ptr);
    if (started) es_param_begin_gesture(plugin, index);
    else es_param_end_gesture(plugin, index);
}

void onSetParam(void* ptr, uint32_t index, float value) {
    es_param_set_from_gui(static_cast<const clap_plugin_t*>(ptr), index, double(value));
}

bool gui_set_scale(const clap_plugin_t*, double) { return false; }   // cocoa's sizes are logical

bool gui_get_size(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
    auto* gui = static_cast<GuiState*>(es_plugin_get_gui(plugin));
    if (gui == nullptr) return false;
    *width  = gui->width;
    *height = gui->height;
    return true;
}

bool gui_can_resize(const clap_plugin_t*) { return true; }

bool gui_get_resize_hints(const clap_plugin_t*, clap_gui_resize_hints_t* hints) {
    hints->can_resize_horizontally = true;
    hints->can_resize_vertically   = true;
    hints->preserve_aspect_ratio   = true;
    hints->aspect_ratio_width      = kWidth;
    hints->aspect_ratio_height     = kHeight;
    return true;
}

bool gui_adjust_size(const clap_plugin_t*, uint32_t* width, uint32_t* height) {
    // Keep the design's ratio and the smallest size (0.8×), as the UI's own constraints do.
    *width  = std::max<uint32_t>(*width, kWidth * 4 / 5);
    *height = static_cast<uint32_t>(std::lround(*width * double(kHeight) / kWidth));
    return true;
}

bool gui_set_size(const clap_plugin_t* plugin, uint32_t width, uint32_t height) {
    auto* gui = static_cast<GuiState*>(es_plugin_get_gui(plugin));
    if (gui == nullptr) return false;
    gui->width  = width;
    gui->height = height;
    applySize(gui);
    return true;
}

bool gui_set_parent(const clap_plugin_t* plugin, const clap_window_t* window) {
    auto* gui = static_cast<GuiState*>(es_plugin_get_gui(plugin));
    if (gui == nullptr || std::strcmp(window->api, kWindowApi) != 0) return false;
#ifdef __APPLE__
    gui->parentWindow = reinterpret_cast<uintptr_t>(window->cocoa);
#else
    gui->parentWindow = static_cast<uintptr_t>(window->x11);
#endif
    if (!gui->exporter) {
        gCreatingFor = plugin;
        gui->exporter.reset(new UIExporter(const_cast<clap_plugin_t*>(plugin), gui->parentWindow, 48000.0,
                                           onEditParam, onSetParam, nullptr, nullptr, nullptr, nullptr, nullptr,
                                           const_cast<clap_plugin_t*>(plugin), 0.0,
                                           DGL_NAMESPACE::Application::kTypeClassic));
        gCreatingFor = nullptr;
        applySize(gui);
    }
    return true;
}

bool gui_set_transient(const clap_plugin_t*, const clap_window_t*) { return false; }   // embedded only
void gui_suggest_title(const clap_plugin_t*, const char*) {}

bool gui_show(const clap_plugin_t* plugin) {
    auto* gui = static_cast<GuiState*>(es_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return false;
    if (!gui->timerRegistered)
        if (const auto* timer = hostTimer(plugin))
            gui->timerRegistered = timer->register_timer(es_plugin_host(plugin), 16 /* ~60 fps */, &gui->timerId);
    return true;
}

bool gui_hide(const clap_plugin_t* plugin) {
    auto* gui = static_cast<GuiState*>(es_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return false;
    if (gui->timerRegistered) {
        if (const auto* timer = hostTimer(plugin)) timer->unregister_timer(es_plugin_host(plugin), gui->timerId);
        gui->timerRegistered = false;
    }
    return true;
}

const clap_plugin_gui_t kGui = {
    .is_api_supported  = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create            = gui_create,
    .destroy           = gui_destroy,
    .set_scale         = gui_set_scale,
    .get_size          = gui_get_size,
    .can_resize        = gui_can_resize,
    .get_resize_hints  = gui_get_resize_hints,
    .adjust_size       = gui_adjust_size,
    .set_size          = gui_set_size,
    .set_parent        = gui_set_parent,
    .set_transient     = gui_set_transient,
    .suggest_title     = gui_suggest_title,
    .show              = gui_show,
    .hide              = gui_hide,
};

} // namespace

extern "C" const clap_plugin_gui_t* es_gui_extension(void) { return &kGui; }

// CLAP timer-support: re-derive the size if the window changed screens, then pump DPF/pugl.
extern "C" void es_gui_on_timer(const clap_plugin_t* plugin, clap_id) {
    auto* gui = static_cast<GuiState*>(es_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return;
    if (liveScale(gui) != gui->appliedScale) applySize(gui);
    gui->exporter->plugin_idle();
}

END_NAMESPACE_DISTRHO
