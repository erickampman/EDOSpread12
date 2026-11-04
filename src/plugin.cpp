// src/plugin.cpp — EDOSpread12: the CLAP plugin around SpreadCore. A note effect with one note
// input and one note output port and 15 parameters (EDO, Origin, Fill, and a step for each of the
// 12 keys from the origin). Hand-written CLAP entry point, as Retune's, so the ports can speak
// MIDI 2.0.
//
// The ports advertise CLAP | MIDI | MIDI2 and prefer MIDI2 (docs/design.md: MIDI 2.0 is the base
// case). Output follows input: CLAP_EVENT_MIDI2 leaves as CLAP_EVENT_MIDI2, and CLAP note events
// as CLAP note events with the fraction as a tuning expression (uw-core's ClapNoteRemap). A
// CLAP_EVENT_MIDI note on a whole key leaves as CLAP_EVENT_MIDI; one with a fraction leaves as
// CLAP_EVENT_MIDI2, since MIDI 1.0 can't carry it (docs/design.md, "Behaviour"). That needs a host
// that takes MIDI 2.0: where the host's note dialects don't include MIDI2, or inside clap-wrapper
// (AUv2, VST3), MIDI 1.0 is rounded to the nearest key instead, as in Retune.
//
// Inside clap-wrapper the ports prefer MIDI 1.0, and both have id 0, as Retune's: the AUv2 wrapper
// turns MIDI 2.0 output into MIDI 1.0 with the key alone, and sends an output event to the port
// whose id equals the event's port_index.
//
// The step parameters are named for their keys, which the origin decides ("Step A", "Step A#" …
// with an A origin). Changing Origin asks the host to rescan the parameter info.
//
// The editor (src/UI/EDOSpread12UI.cpp, DPF + NanoVG) meets this file only at plugin_gui.h: its
// edits arrive through a ring and are applied, and told to the host, at the next process or flush.

#include <clap/clap.h>
#include <strings.h>   // strcasecmp

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "SpreadCore.h"
#include "plugin_gui.h"
#include "uw/clap/ClapNoteRemap.h"
#include "uw/ump/Ump.h"

#ifndef EDOSPREAD12_VERSION_STRING
#define EDOSPREAD12_VERSION_STRING "0.9.0"
#endif

namespace {

namespace ump = uw::ump;
namespace es  = edospread12;

// ---------------------------------------------------------------------------------------------
// Parameters (appended, never inserted: the ids are the indices)
// ---------------------------------------------------------------------------------------------

enum : uint32_t { kEdo = 0, kOrigin = 1, kFill = 2, kStep0 = 3 };

struct ParamSpec {
    const char* name;     // the step parameters' names come from the origin (stepName)
    const char* symbol;   // state file key
    double      min, max, def;
};

constexpr ParamSpec kParams[] = {
    {"EDO", "edo", es::kMinEdo, es::kMaxEdo, 12},
    {"Origin", "origin", 0, es::kKeys - 1, es::kOriginA},
    {"Fill", "fill", 0, 1, double(es::Fill::Nearest)},
    // The host sees 0 … (max N); SpreadCore clamps each step to 0 … N when it uses it.
    {"Step 1", "step1", 0, es::kMaxEdo, 0},
    {"Step 2", "step2", 0, es::kMaxEdo, 1},
    {"Step 3", "step3", 0, es::kMaxEdo, 2},
    {"Step 4", "step4", 0, es::kMaxEdo, 3},
    {"Step 5", "step5", 0, es::kMaxEdo, 4},
    {"Step 6", "step6", 0, es::kMaxEdo, 5},
    {"Step 7", "step7", 0, es::kMaxEdo, 6},
    {"Step 8", "step8", 0, es::kMaxEdo, 7},
    {"Step 9", "step9", 0, es::kMaxEdo, 8},
    {"Step 10", "step10", 0, es::kMaxEdo, 9},
    {"Step 11", "step11", 0, es::kMaxEdo, 10},
    {"Step 12", "step12", 0, es::kMaxEdo, 11},
};
constexpr uint32_t kParamCount = sizeof(kParams) / sizeof(kParams[0]);
static_assert(kParamCount == kStep0 + es::kKeys);

const char* const kNoteNames[es::kKeys] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* const kFillNames[]          = {"Floor", "Nearest"};

double clampParam(uint32_t index, double value) {
    return std::clamp(std::round(value), kParams[index].min, kParams[index].max);
}

// ---------------------------------------------------------------------------------------------
// Editor edits (main thread) → the audio thread, as NoteGran's: process() and flush() are never
// concurrent, so one consumer.
// ---------------------------------------------------------------------------------------------

struct GuiEdit {
    enum Kind : uint8_t { Begin, Value, End } kind;
    uint32_t index;
    double   value;
};

class GuiEditRing {
public:
    bool push(const GuiEdit& edit) {
        const uint32_t t    = tail.load(std::memory_order_relaxed);
        const uint32_t next = (t + 1) & (kCapacity - 1);
        if (next == head.load(std::memory_order_acquire)) return false;   // full: dropped, never blocks
        buffer[t] = edit;
        tail.store(next, std::memory_order_release);
        return true;
    }
    bool pop(GuiEdit& out) {
        const uint32_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        out = buffer[h];
        head.store((h + 1) & (kCapacity - 1), std::memory_order_release);
        return true;
    }

private:
    static constexpr uint32_t kCapacity = 512;   // a power of two; a refill is 12 values and 24 gestures
    GuiEdit                   buffer[kCapacity]{};
    std::atomic<uint32_t>     head{0};
    std::atomic<uint32_t>     tail{0};
};

// ---------------------------------------------------------------------------------------------
// The plugin
// ---------------------------------------------------------------------------------------------

struct Plugin {
    clap_plugin_t      clap{};
    const clap_host_t* host = nullptr;

    // Parameter values: written by process/flush and by state_load; read by the main thread.
    std::atomic<double> values[kParamCount];
    std::atomic<bool>   valuesChangedByState{false};   // state_load → process: re-apply
    std::atomic<bool>   namesChanged{false};           // Origin moved → main thread: rescan info

    es::SpreadCore    core;        // MIDI 1.0 and 2.0; audio thread while active, main thread otherwise
    uw::ClapNoteRemap clapNotes;   // CLAP note events; likewise
    std::string       scratch;
    bool              wrapped = false;   // hosted by clap-wrapper (AUv2, VST3)

    void*       gui = nullptr;   // the editor's state (EDOSpread12UI.cpp); main thread
    GuiEditRing guiEdits;
    // Input keys held, on any channel: counted on the audio thread, published as bits for the editor.
    uint8_t               heldCount[128] = {};
    std::atomic<uint64_t> heldBits[2]{};
};

Plugin* self(const clap_plugin_t* plugin) { return static_cast<Plugin*>(plugin->plugin_data); }

int value(const Plugin* p, uint32_t id) { return int(p->values[id].load(std::memory_order_relaxed)); }

es::Params currentParams(const Plugin* p) {
    es::Params params;
    params.edo    = value(p, kEdo);
    params.origin = value(p, kOrigin);
    params.fill   = value(p, kFill) == 0 ? es::Fill::Floor : es::Fill::Nearest;
    for (int d = 0; d < es::kKeys; ++d) params.steps[size_t(d)] = value(p, kStep0 + uint32_t(d));
    return params;
}

void applyParams(Plugin* p) {
    const es::Params params = currentParams(p);
    p->core.setParams(params);
    int shifts[128];
    es::shifts(params, shifts);
    p->clapNotes.setShifts(shifts);
}

// Stores a value; a new origin renames the steps, so the host is asked to rescan their info.
void storeValue(Plugin* p, uint32_t id, double value) {
    const double v = clampParam(id, value);
    if (id == kOrigin && v != p->values[kOrigin].load(std::memory_order_relaxed)) {
        p->namesChanged.store(true, std::memory_order_release);
        p->host->request_callback(p->host);
    }
    p->values[id].store(v, std::memory_order_relaxed);
}

void applyParamEvent(Plugin* p, const clap_event_param_value_t* ev) {
    if (ev->param_id >= kParamCount) return;
    storeValue(p, ev->param_id, ev->value);
    applyParams(p);
}

// The editor's edits, applied and told to the host (as param and gesture events at time 0, before
// anything else this block sends). True if any value changed.
bool applyGuiEdits(Plugin* p, const clap_output_events_t* out) {
    bool    changed = false;
    GuiEdit edit;
    while (p->guiEdits.pop(edit)) {
        if (edit.index >= kParamCount) continue;
        if (edit.kind == GuiEdit::Value) {
            storeValue(p, edit.index, edit.value);
            changed = true;
        }
        if (out == nullptr) continue;
        if (edit.kind == GuiEdit::Value) {
            clap_event_param_value_t ev{};
            ev.header     = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
            ev.param_id   = edit.index;
            ev.note_id    = -1;
            ev.port_index = -1;
            ev.channel    = -1;
            ev.key        = -1;
            ev.value      = p->values[edit.index].load(std::memory_order_relaxed);
            out->try_push(out, &ev.header);
        } else {
            clap_event_param_gesture_t ev{};
            ev.header   = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID,
                           uint16_t(edit.kind == GuiEdit::Begin ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                : CLAP_EVENT_PARAM_GESTURE_END),
                           0};
            ev.param_id = edit.index;
            out->try_push(out, &ev.header);
        }
    }
    return changed;
}

// --- held keys, for the editor ---

void hold(Plugin* p, int key, bool on) {
    if (key < 0 || key > 127) return;
    uint8_t& n = p->heldCount[key];
    if (on && n < 255) ++n;
    else if (!on && n > 0) --n;
}

void releaseAll(Plugin* p) { std::memset(p->heldCount, 0, sizeof(p->heldCount)); }

void publishHeld(Plugin* p) {
    uint64_t words[2] = {0, 0};
    for (int k = 0; k < 128; ++k)
        if (p->heldCount[k] != 0) words[k / 64] |= uint64_t(1) << (k % 64);
    p->heldBits[0].store(words[0], std::memory_order_relaxed);
    p->heldBits[1].store(words[1], std::memory_order_relaxed);
}

// A UMP note on or off (MIDI 1.0 or 2.0), or All Sound / Notes Off.
void trackHeld(Plugin* p, const uw::MidiMessage& m) {
    if (!ump::isChannelVoice(m)) return;
    if (ump::isNoteOn(m)) hold(p, ump::noteNumber(m), true);
    else if (ump::isNoteOff(m)) hold(p, ump::noteNumber(m), false);
    else if (ump::status(m) == ump::Status::ControlChange) {
        const auto index = (m.words[0] >> 8) & 0x7F;
        if (index == 120 || index == 123) releaseAll(p);
    }
}

// "Step A", with an A origin, for the first step parameter.
void stepName(const Plugin* p, uint32_t id, char* out, size_t size) {
    const int key = (value(p, kOrigin) + int(id - kStep0)) % es::kKeys;
    std::snprintf(out, size, "Step %s", kNoteNames[key]);
}

// ---------------------------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------------------------

bool push(const clap_output_events_t* out, const clap_event_header_t* h) {
    return out != nullptr && out->try_push != nullptr && out->try_push(out, h);
}

// Run one UMP through the core and push what comes out: MIDI 1.0 (MT 0x2) as CLAP_EVENT_MIDI,
// MIDI 2.0 (MT 0x4, including a promoted MIDI 1.0 note) as CLAP_EVENT_MIDI2.
void spreadAndPush(Plugin* p, const clap_event_header_t* in, uint16_t port, const uw::MidiMessage& m,
                   const clap_output_events_t* out) {
    uw::MidiMessage results[es::kMaxOut];
    const std::size_t n = p->core.process(m, results);
    for (std::size_t i = 0; i < n; ++i) {
        if (ump::messageType(results[i]) == ump::MessageType::Midi1ChannelVoice) {
            clap_event_midi_t ev{};
            ev.header = {sizeof(ev), in->time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, in->flags};
            ev.port_index = port;
            if (ump::toMidi1Bytes(results[i], ev.data) == 0) continue;
            push(out, &ev.header);
        } else {
            clap_event_midi2_t ev{};
            ev.header = {sizeof(ev), in->time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI2, in->flags};
            ev.port_index = port;
            std::memcpy(ev.data, results[i].words, sizeof(ev.data));
            push(out, &ev.header);
        }
    }
}

void handleEvent(Plugin* p, const clap_event_header_t* h, const clap_output_events_t* out) {
    if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
    switch (h->type) {
        case CLAP_EVENT_PARAM_VALUE:
            applyParamEvent(p, reinterpret_cast<const clap_event_param_value_t*>(h));
            break;

        case CLAP_EVENT_MIDI2: {
            const auto* ev = reinterpret_cast<const clap_event_midi2_t*>(h);
            uw::MidiMessage m;
            std::memcpy(m.words, ev->data, sizeof(m.words));
            m.wordCount = ump::wordCountForType(uint8_t(m.words[0] >> 28));
            trackHeld(p, m);
            spreadAndPush(p, h, ev->port_index, m, out);
            break;
        }

        case CLAP_EVENT_MIDI: {
            const auto* ev = reinterpret_cast<const clap_event_midi_t*>(h);
            if (ev->data[0] >= 0x80 && ev->data[0] < 0xF0) {
                // Channel voice: wrap as MT 0x2 (group 0), the core's MIDI 1.0 form.
                uw::MidiMessage m;
                m.words[0]  = 0x2000'0000u | (uint32_t(ev->data[0]) << 16) |
                              (uint32_t(ev->data[1] & 0x7F) << 8) | uint32_t(ev->data[2] & 0x7F);
                m.wordCount = 1;
                trackHeld(p, m);
                spreadAndPush(p, h, ev->port_index, m, out);
            } else {
                push(out, h);   // system messages pass through as they are
            }
            break;
        }

        case CLAP_EVENT_NOTE_ON:
        case CLAP_EVENT_NOTE_OFF:
        case CLAP_EVENT_NOTE_CHOKE: {
            const auto* ev = reinterpret_cast<const clap_event_note_t*>(h);
            if (ev->key < 0) {
                if (h->type != CLAP_EVENT_NOTE_ON) releaseAll(p);   // a wildcard end
            } else {
                hold(p, ev->key, h->type == CLAP_EVENT_NOTE_ON);
            }
            p->clapNotes.process(h, out);
            break;
        }
        case CLAP_EVENT_NOTE_EXPRESSION:
            p->clapNotes.process(h, out);
            break;

        case CLAP_EVENT_MIDI_SYSEX:
            push(out, h);   // SysEx passes through
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------------------------
// clap_plugin
// ---------------------------------------------------------------------------------------------

bool plug_init(const clap_plugin_t* plugin) {
    Plugin* p = self(plugin);
    // Promote MIDI 1.0 to MIDI 2.0 only where MIDI 2.0 can leave: not inside clap-wrapper, and
    // only to a host whose note dialects include MIDI2.
    const auto* notePorts = static_cast<const clap_host_note_ports_t*>(
        p->host->get_extension(p->host, CLAP_EXT_NOTE_PORTS));
    const bool hostMidi2 = notePorts != nullptr && notePorts->supported_dialects != nullptr &&
                           (notePorts->supported_dialects(p->host) & CLAP_NOTE_DIALECT_MIDI2) != 0;
    p->core.setPromoteMidi1(hostMidi2 && !p->wrapped);
    return true;
}

void plug_destroy(const clap_plugin_t* plugin) { delete self(plugin); }

bool plug_activate(const clap_plugin_t* plugin, double, uint32_t, uint32_t) {
    Plugin* p = self(plugin);
    p->core.reset();
    p->clapNotes.reset();
    releaseAll(p);
    publishHeld(p);
    applyParams(p);
    p->valuesChangedByState.store(false, std::memory_order_relaxed);
    return true;
}

void plug_deactivate(const clap_plugin_t*) {}
bool plug_start_processing(const clap_plugin_t*) { return true; }
void plug_stop_processing(const clap_plugin_t*) {}

void plug_reset(const clap_plugin_t* plugin) {
    self(plugin)->core.reset();
    self(plugin)->clapNotes.reset();
    releaseAll(self(plugin));
    publishHeld(self(plugin));
}

clap_process_status plug_process(const clap_plugin_t* plugin, const clap_process_t* process) {
    Plugin* p = self(plugin);
    const bool fromGui = applyGuiEdits(p, process->out_events);
    if (p->valuesChangedByState.exchange(false, std::memory_order_acquire) || fromGui)
        applyParams(p);

    const clap_input_events_t* in = process->in_events;
    const uint32_t count = in != nullptr ? in->size(in) : 0;
    for (uint32_t i = 0; i < count; ++i)
        handleEvent(p, in->get(in, i), process->out_events);
    publishHeld(p);
    return CLAP_PROCESS_CONTINUE;
}

void plug_on_main_thread(const clap_plugin_t* plugin) {
    Plugin* p = self(plugin);
    if (!p->namesChanged.exchange(false, std::memory_order_acquire)) return;
    const auto* hostParams = static_cast<const clap_host_params_t*>(
        p->host->get_extension(p->host, CLAP_EXT_PARAMS));
    if (hostParams != nullptr && hostParams->rescan != nullptr)
        hostParams->rescan(p->host, CLAP_PARAM_RESCAN_INFO);
}

// ---------------------------------------------------------------------------------------------
// audio-ports extension: none (a note effect)
// ---------------------------------------------------------------------------------------------

uint32_t audio_ports_count(const clap_plugin_t*, bool) { return 0; }
bool audio_ports_get(const clap_plugin_t*, uint32_t, bool, clap_audio_port_info_t*) { return false; }
const clap_plugin_audio_ports_t kAudioPorts = {audio_ports_count, audio_ports_get};

// ---------------------------------------------------------------------------------------------
// note-ports extension: one in, one out
// ---------------------------------------------------------------------------------------------

uint32_t note_ports_count(const clap_plugin_t*, bool) { return 1; }

bool note_ports_get(const clap_plugin_t* plugin, uint32_t index, bool isInput, clap_note_port_info_t* info) {
    if (index != 0) return false;
    std::memset(info, 0, sizeof(*info));
    info->id                 = 0;   // ids may overlap between inputs and outputs
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI2;
    info->preferred_dialect  = self(plugin)->wrapped ? CLAP_NOTE_DIALECT_MIDI : CLAP_NOTE_DIALECT_MIDI2;
    std::snprintf(info->name, sizeof(info->name), "%s", isInput ? "Notes in" : "Notes out");
    return true;
}

const clap_plugin_note_ports_t kNotePorts = {note_ports_count, note_ports_get};

// ---------------------------------------------------------------------------------------------
// params extension
// ---------------------------------------------------------------------------------------------

uint32_t params_count(const clap_plugin_t*) { return kParamCount; }

bool params_get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* info) {
    if (index >= kParamCount) return false;
    std::memset(info, 0, sizeof(*info));
    info->id            = index;
    info->flags         = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
    if (index == kOrigin || index == kFill) info->flags |= CLAP_PARAM_IS_ENUM;
    info->min_value     = kParams[index].min;
    info->max_value     = kParams[index].max;
    info->default_value = kParams[index].def;
    if (index >= kStep0) {
        stepName(self(plugin), index, info->name, sizeof(info->name));
        std::snprintf(info->module, sizeof(info->module), "Steps");
    } else {
        std::snprintf(info->name, sizeof(info->name), "%s", kParams[index].name);
    }
    return true;
}

bool params_get_value(const clap_plugin_t* plugin, clap_id id, double* out) {
    if (id >= kParamCount) return false;
    *out = self(plugin)->values[id].load(std::memory_order_relaxed);
    return true;
}

bool params_value_to_text(const clap_plugin_t*, clap_id id, double value, char* out, uint32_t size) {
    if (id >= kParamCount) return false;
    const int v = int(clampParam(id, value));
    if (id == kOrigin)    std::snprintf(out, size, "%s", kNoteNames[v]);
    else if (id == kFill) std::snprintf(out, size, "%s", kFillNames[v]);
    else                  std::snprintf(out, size, "%d", v);
    return true;
}

bool params_text_to_value(const clap_plugin_t*, clap_id id, const char* text, double* out) {
    if (id >= kParamCount) return false;
    // Origin and Fill take their names too ("A#", "nearest"), ignoring case.
    if (id == kOrigin || id == kFill) {
        const char* const* names = id == kOrigin ? kNoteNames : kFillNames;
        const int          count = id == kOrigin ? es::kKeys : 2;
        for (int i = 0; i < count; ++i)
            if (strcasecmp(text, names[i]) == 0) {
                *out = i;
                return true;
            }
    }
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (end == text) return false;
    *out = clampParam(id, value);
    return true;
}

void params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in,
                  const clap_output_events_t* out) {
    // Never concurrent with process, so the core can be updated here too.
    Plugin* p = self(plugin);
    if (applyGuiEdits(p, out)) applyParams(p);
    const uint32_t count = in != nullptr ? in->size(in) : 0;
    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header_t* h = in->get(in, i);
        if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE)
            applyParamEvent(p, reinterpret_cast<const clap_event_param_value_t*>(h));
    }
}

const clap_plugin_params_t kParamsExt = {params_count, params_get_info, params_get_value,
                                         params_value_to_text, params_text_to_value, params_flush};

// ---------------------------------------------------------------------------------------------
// state extension: "EDOSpread12 1" then "<symbol> <value>" lines; unknown symbols are ignored
// ---------------------------------------------------------------------------------------------

bool state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    Plugin* p = self(plugin);
    p->scratch = "EDOSpread12 1\n";
    char line[64];
    for (uint32_t i = 0; i < kParamCount; ++i) {
        std::snprintf(line, sizeof(line), "%s %d\n", kParams[i].symbol, value(p, i));
        p->scratch += line;
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(p->scratch.data());
    for (uint64_t done = 0; done < p->scratch.size();) {
        const int64_t written = stream->write(stream, bytes + done, p->scratch.size() - done);
        if (written <= 0) return false;
        done += uint64_t(written);
    }
    return true;
}

bool state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    Plugin* p = self(plugin);
    std::string text;
    char buffer[256];
    for (;;) {
        const int64_t read = stream->read(stream, buffer, sizeof(buffer));
        if (read < 0) return false;
        if (read == 0) break;
        text.append(buffer, size_t(read));
    }
    if (text.rfind("EDOSpread12 ", 0) != 0) return false;   // empty, or not ours

    size_t pos = text.find('\n');
    while (pos != std::string::npos && pos + 1 < text.size()) {
        const size_t start = pos + 1;
        pos = text.find('\n', start);
        const std::string line = text.substr(start, pos == std::string::npos ? std::string::npos
                                                                              : pos - start);
        const size_t space = line.find(' ');
        if (space == std::string::npos) continue;
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (line.compare(0, space, kParams[i].symbol) == 0)
                p->values[i].store(clampParam(i, std::strtod(line.c_str() + space + 1, nullptr)),
                                   std::memory_order_relaxed);
    }
    p->valuesChangedByState.store(true, std::memory_order_release);

    // The step names follow the origin, so the info may have changed with the values.
    const auto* hostParams = static_cast<const clap_host_params_t*>(
        p->host->get_extension(p->host, CLAP_EXT_PARAMS));
    if (hostParams != nullptr && hostParams->rescan != nullptr)
        hostParams->rescan(p->host, CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_INFO);
    return true;
}

const clap_plugin_state_t kStateExt = {state_save, state_load};

void plug_on_timer(const clap_plugin_t* plugin, clap_id timerId) { es_gui_on_timer(plugin, timerId); }

const clap_plugin_timer_support_t kTimerExt = {plug_on_timer};

const void* plug_get_extension(const clap_plugin_t*, const char* id) {
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return es_gui_extension();
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) return &kTimerExt;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &kAudioPorts;
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) return &kNotePorts;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &kParamsExt;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &kStateExt;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// descriptor + factory + entry
// ---------------------------------------------------------------------------------------------

const char* kFeatures[] = {
    CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
    CLAP_PLUGIN_FEATURE_UTILITY,
    nullptr,
};

const clap_plugin_descriptor_t kDescriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "com.unlikelyware.edospread12",
    .name         = "EDOSpread12",
    .vendor       = "Unlikelyware",
    .url          = "https://unlikelyware.com",
    .manual_url   = "",
    .support_url  = "",
    .version      = EDOSPREAD12_VERSION_STRING,
    .description  = "Plays an equal division of the octave from a 12-key keyboard: each key of the "
                    "octave is given its own step.",
    .features     = kFeatures,
};

const clap_plugin_t* factory_create(const clap_plugin_factory_t*, const clap_host_t* host,
                                    const char* id) {
    if (std::strcmp(id, kDescriptor.id) != 0) return nullptr;
    auto* p = new (std::nothrow) Plugin();
    if (p == nullptr) return nullptr;
    p->host = host;
    // clap-wrapper's host names the plugin's real host but keeps its own url (clap_proxy.cpp).
    p->wrapped = host->url != nullptr &&
                 std::strcmp(host->url, "https://github.com/free-audio/clap-wrapper") == 0;
    for (uint32_t i = 0; i < kParamCount; ++i)
        p->values[i].store(kParams[i].def, std::memory_order_relaxed);
    applyParams(p);
    p->clap = clap_plugin_t{
        .desc             = &kDescriptor,
        .plugin_data      = p,
        .init             = plug_init,
        .destroy          = plug_destroy,
        .activate         = plug_activate,
        .deactivate       = plug_deactivate,
        .start_processing = plug_start_processing,
        .stop_processing  = plug_stop_processing,
        .reset            = plug_reset,
        .process          = plug_process,
        .get_extension    = plug_get_extension,
        .on_main_thread   = plug_on_main_thread,
    };
    return &p->clap;
}

uint32_t factory_count(const clap_plugin_factory_t*) { return 1; }

const clap_plugin_descriptor_t* factory_get(const clap_plugin_factory_t*, uint32_t index) {
    return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_factory_t kFactory = {factory_count, factory_get, factory_create};

bool entry_init(const char*) { return true; }
void entry_deinit() {}

const void* entry_get_factory(const char* factoryId) {
    return std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0 ? &kFactory : nullptr;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// The seam the editor calls (plugin_gui.h)
// ---------------------------------------------------------------------------------------------

extern "C" {

const clap_host_t* es_plugin_host(const clap_plugin_t* plugin) { return self(plugin)->host; }

void  es_plugin_set_gui(const clap_plugin_t* plugin, void* gui) { self(plugin)->gui = gui; }
void* es_plugin_get_gui(const clap_plugin_t* plugin) { return self(plugin)->gui; }

uint32_t es_param_count(const clap_plugin_t*) { return kParamCount; }

bool es_param_info(const clap_plugin_t* plugin, uint32_t index, es_param_info_t* info) {
    if (index >= kParamCount) return false;
    static thread_local char names[kParamCount][CLAP_NAME_SIZE];   // the main thread's, for a step
    clap_param_info_t clap{};
    params_get_info(plugin, index, &clap);
    std::snprintf(names[index], sizeof(names[index]), "%s", clap.name);
    info->name   = names[index];
    info->min    = kParams[index].min;
    info->max    = kParams[index].max;
    info->def    = kParams[index].def;
    info->labels = index == kOrigin ? kNoteNames : index == kFill ? kFillNames : nullptr;
    return true;
}

double es_param_value(const clap_plugin_t* plugin, uint32_t index) {
    return index < kParamCount ? self(plugin)->values[index].load(std::memory_order_relaxed) : 0.0;
}

void es_param_text(const clap_plugin_t* plugin, uint32_t index, double v, char* out, uint32_t size) {
    if (size == 0) return;
    out[0] = 0;
    params_value_to_text(plugin, index, v, out, size);
}

void es_param_begin_gesture(const clap_plugin_t* plugin, uint32_t index) {
    self(plugin)->guiEdits.push({GuiEdit::Begin, index, 0.0});
}

void es_param_set_from_gui(const clap_plugin_t* plugin, uint32_t index, double v) {
    Plugin* p = self(plugin);
    if (index >= kParamCount) return;
    // Stored at once, so the editor's next read shows it; applied at the next process() or flush.
    storeValue(p, index, v);   // a new origin asks for the steps' names to be rescanned
    p->guiEdits.push({GuiEdit::Value, index, v});
    const auto* hostParams = static_cast<const clap_host_params_t*>(p->host->get_extension(p->host, CLAP_EXT_PARAMS));
    if (hostParams != nullptr && hostParams->request_flush != nullptr) hostParams->request_flush(p->host);
}

void es_param_end_gesture(const clap_plugin_t* plugin, uint32_t index) {
    self(plugin)->guiEdits.push({GuiEdit::End, index, 0.0});
}

void es_held_keys(const clap_plugin_t* plugin, uint64_t words[2]) {
    words[0] = self(plugin)->heldBits[0].load(std::memory_order_relaxed);
    words[1] = self(plugin)->heldBits[1].load(std::memory_order_relaxed);
}

} // extern "C"

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init         = entry_init,
    .deinit       = entry_deinit,
    .get_factory  = entry_get_factory,
};
