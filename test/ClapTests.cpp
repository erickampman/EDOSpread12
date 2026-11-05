// ClapTests: drives the built EDOSpread12.clap through the CLAP C API, with no DAW: dlopen,
// clap_entry, factory, then events in and the output events checked. SpreadCoreTests covers the
// mapping; this covers the CLAP side: the descriptor, note ports and dialects, the 15 parameters
// (names following the origin, text, flush), MIDI 1.0 promotion and when it's off, CLAP notes,
// and state.
//
// Usage: edospread12-clap-tests /path/to/EDOSpread12.clap. Silent unless a check fails.

#include <clap/clap.h>

#include <dlfcn.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "uw/ump/Ump.h"

namespace {

int failures = 0;

#define CHECK(expr)                                                                       \
    do {                                                                                  \
        if (!(expr)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

// --- host ------------------------------------------------------------------------------------

int gRescans = 0;
clap_param_rescan_flags gRescanFlags = 0;
void hostRescan(const clap_host_t*, clap_param_rescan_flags flags) {
    ++gRescans;
    gRescanFlags |= flags;
}
void hostClear(const clap_host_t*, clap_id, clap_param_clear_flags) {}
void hostRequestFlush(const clap_host_t*) {}
const clap_host_params_t kHostParams = {hostRescan, hostClear, hostRequestFlush};

uint32_t hostDialects(const clap_host_t*) { return CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI2; }
uint32_t hostDialectsMidi1(const clap_host_t*) { return CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI; }
void hostNotePortsRescan(const clap_host_t*, uint32_t) {}
const clap_host_note_ports_t kHostNotePorts      = {hostDialects, hostNotePortsRescan};
const clap_host_note_ports_t kHostNotePortsMidi1 = {hostDialectsMidi1, hostNotePortsRescan};

const void* hostGetExtension(const clap_host_t*, const char* id) {
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &kHostParams;
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) return &kHostNotePorts;
    return nullptr;
}
// A host that takes no MIDI 2.0 from plugins (as Bitwig: CLAP | MIDI | MIDI_MPE).
const void* hostGetExtensionMidi1(const clap_host_t*, const char* id) {
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &kHostParams;
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) return &kHostNotePortsMidi1;
    return nullptr;
}
void hostNoop(const clap_host_t*) {}
int gCallbacks = 0;
void hostRequestCallback(const clap_host_t*) { ++gCallbacks; }

const clap_host_t kHost = {
    .clap_version     = CLAP_VERSION_INIT,
    .host_data        = nullptr,
    .name             = "edospread12-clap-tests",
    .vendor           = "Unlikelyware",
    .url              = "",
    .version          = "0",
    .get_extension    = hostGetExtension,
    .request_restart  = hostNoop,
    .request_process  = hostNoop,
    .request_callback = hostRequestCallback,
};

const clap_host_t kMidi1Host = {
    .clap_version     = CLAP_VERSION_INIT,
    .host_data        = nullptr,
    .name             = "edospread12-clap-tests (no MIDI 2.0)",
    .vendor           = "Unlikelyware",
    .url              = "",
    .version          = "0",
    .get_extension    = hostGetExtensionMidi1,
    .request_restart  = hostNoop,
    .request_process  = hostNoop,
    .request_callback = hostNoop,
};

// The same host as clap-wrapper reports itself (clap_proxy.cpp): the real host's name, its url.
const clap_host_t kWrapperHost = {
    .clap_version     = CLAP_VERSION_INIT,
    .host_data        = nullptr,
    .name             = "Logic Pro",
    .vendor           = "The CLAP Wrapper Team",
    .url              = "https://github.com/free-audio/clap-wrapper",
    .version          = "0.16.0",
    .get_extension    = hostGetExtension,
    .request_restart  = hostNoop,
    .request_process  = hostNoop,
    .request_callback = hostNoop,
};

// --- events ----------------------------------------------------------------------------------

// Owns copies of the input events, in order.
struct Input {
    std::vector<std::vector<uint8_t>> storage;

    void add(const clap_event_header_t& h) {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&h);
        storage.emplace_back(bytes, bytes + h.size);
    }
    void midi1(uint32_t time, uint8_t s, uint8_t d1, uint8_t d2) {
        clap_event_midi_t ev{};
        ev.header = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0};
        ev.data[0] = s; ev.data[1] = d1; ev.data[2] = d2;
        add(ev.header);
    }
    void midi2(uint32_t time, uint32_t w0, uint32_t w1 = 0) {
        clap_event_midi2_t ev{};
        ev.header = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI2, 0};
        ev.data[0] = w0; ev.data[1] = w1;
        add(ev.header);
    }
    void note(uint16_t type, uint32_t time, int32_t noteId, int16_t channel, int16_t key,
              double velocity = 0.8) {
        clap_event_note_t ev{};
        ev.header   = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID, type, 0};
        ev.note_id  = noteId; ev.port_index = 0; ev.channel = channel; ev.key = key;
        ev.velocity = velocity;
        add(ev.header);
    }
    void expression(uint32_t time, clap_note_expression id, int32_t noteId, int16_t channel,
                    int16_t key, double value) {
        clap_event_note_expression_t ev{};
        ev.header        = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_EXPRESSION, 0};
        ev.expression_id = id;
        ev.note_id = noteId; ev.port_index = 0; ev.channel = channel; ev.key = key;
        ev.value = value;
        add(ev.header);
    }
    void param(uint32_t time, clap_id id, double value) {
        clap_event_param_value_t ev{};
        ev.header   = {sizeof(ev), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        ev.param_id = id;
        ev.note_id  = -1; ev.port_index = -1; ev.channel = -1; ev.key = -1;
        ev.value    = value;
        add(ev.header);
    }

    static uint32_t size(const clap_input_events_t* list) {
        return uint32_t(static_cast<const Input*>(list->ctx)->storage.size());
    }
    static const clap_event_header_t* get(const clap_input_events_t* list, uint32_t i) {
        return reinterpret_cast<const clap_event_header_t*>(
            static_cast<const Input*>(list->ctx)->storage[i].data());
    }
};

struct Output {
    struct Event {
        uint32_t time; uint16_t type; uint32_t words[4]; uint8_t bytes[3];
        // CLAP note events and expressions; value is the velocity or the expression value
        int32_t noteId; int16_t channel, key; int32_t expressionId; double value;
    };
    std::vector<Event> events;

    static bool tryPush(const clap_output_events_t* list, const clap_event_header_t* h) {
        auto* self = static_cast<Output*>(list->ctx);
        Event e{h->time, h->type, {}, {}, -1, -1, -1, -1, 0};
        if (h->type == CLAP_EVENT_MIDI2)
            std::memcpy(e.words, reinterpret_cast<const clap_event_midi2_t*>(h)->data, sizeof(e.words));
        else if (h->type == CLAP_EVENT_MIDI)
            std::memcpy(e.bytes, reinterpret_cast<const clap_event_midi_t*>(h)->data, 3);
        else if (h->type == CLAP_EVENT_NOTE_EXPRESSION) {
            const auto* x = reinterpret_cast<const clap_event_note_expression_t*>(h);
            e.noteId = x->note_id; e.channel = x->channel; e.key = x->key;
            e.expressionId = x->expression_id; e.value = x->value;
        } else if (h->type <= CLAP_EVENT_NOTE_END) {
            const auto* n = reinterpret_cast<const clap_event_note_t*>(h);
            e.noteId = n->note_id; e.channel = n->channel; e.key = n->key; e.value = n->velocity;
        }
        self->events.push_back(e);
        return true;
    }
};

bool isMidi1(const Output::Event& e, uint32_t time, uint8_t s, uint8_t d1, uint8_t d2) {
    return e.type == CLAP_EVENT_MIDI && e.time == time && e.bytes[0] == s && e.bytes[1] == d1 &&
           e.bytes[2] == d2;
}
bool isMidi2(const Output::Event& e, uint32_t time, uint32_t w0, uint32_t w1) {
    return e.type == CLAP_EVENT_MIDI2 && e.time == time && e.words[0] == w0 && e.words[1] == w1;
}
bool isNote(const Output::Event& e, uint16_t type, uint32_t time, int32_t noteId, int16_t channel,
            int16_t key) {
    return e.type == type && e.time == time && e.noteId == noteId && e.channel == channel &&
           e.key == key;
}
bool isExpression(const Output::Event& e, uint32_t time, int32_t expressionId, int32_t noteId,
                  int16_t channel, int16_t key, double value) {
    return e.type == CLAP_EVENT_NOTE_EXPRESSION && e.time == time && e.expressionId == expressionId &&
           e.noteId == noteId && e.channel == channel && e.key == key &&
           std::fabs(e.value - value) < 1e-9;
}

// --- the plugin under test -------------------------------------------------------------------

const clap_plugin_entry_t* gEntry = nullptr;
const clap_plugin_factory_t* gFactory = nullptr;

struct Instance {
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_state_t* state = nullptr;
    const clap_plugin_note_ports_t* notePorts = nullptr;

    explicit Instance(const clap_host_t* host = &kHost) {
        plugin = gFactory->create_plugin(gFactory, host, "com.unlikelyware.edospread12");
        if (plugin == nullptr || !plugin->init(plugin)) return;
        params    = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
        state     = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
        notePorts = static_cast<const clap_plugin_note_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
        plugin->activate(plugin, 48000.0, 1, 512);
        plugin->start_processing(plugin);
    }
    ~Instance() {
        if (plugin == nullptr) return;
        plugin->stop_processing(plugin);
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
    }
    bool ok() const { return plugin != nullptr && params != nullptr && state != nullptr && notePorts != nullptr; }

    std::vector<Output::Event> process(Input& in) {
        Output out;
        const clap_input_events_t inList = {&in, Input::size, Input::get};
        const clap_output_events_t outList = {&out, Output::tryPush};
        clap_process_t p{};
        p.steady_time = -1;
        p.frames_count = 512;
        p.in_events = &inList;
        p.out_events = &outList;
        CHECK(plugin->process(plugin, &p) == CLAP_PROCESS_CONTINUE);
        return out.events;
    }

    double value(clap_id id) {
        double v = -1000;
        params->get_value(plugin, id, &v);
        return v;
    }
};

bool openLibrary(const std::string& path) {
    std::string binary = path;
    const std::string name = path.substr(path.find_last_of('/') + 1);
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".clap") == 0)
        binary = path + "/Contents/MacOS/" + name.substr(0, name.size() - 5);
    void* handle = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) return false;
    gEntry = static_cast<const clap_plugin_entry_t*>(dlsym(handle, "clap_entry"));
    if (gEntry == nullptr || !gEntry->init(path.c_str())) return false;
    gFactory = static_cast<const clap_plugin_factory_t*>(gEntry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    return gFactory != nullptr;
}

// --- tests -----------------------------------------------------------------------------------

namespace ump = uw::ump;

enum : clap_id { kEdo = 0, kOrigin = 1, kFill = 2, kStep0 = 3 };

// 19-EDO from A, Nearest: the steps for keys A … G#.
const int kNearest19[12] = {0, 2, 3, 5, 6, 8, 10, 11, 13, 14, 16, 17};

void set19(Input& in) {
    in.param(0, kEdo, 19);
    for (clap_id d = 0; d < 12; ++d) in.param(0, kStep0 + d, kNearest19[d]);
}

// C5 (72) in 19-EDO from A: step 5, 69 + 60/19 semitones.
constexpr uint32_t kC5Pitch79 = 69 * 512 + 1617;

void testDescriptor() {
    CHECK(gFactory->get_plugin_count(gFactory) == 1);
    const clap_plugin_descriptor_t* d = gFactory->get_plugin_descriptor(gFactory, 0);
    CHECK(d != nullptr && std::strcmp(d->id, "com.unlikelyware.edospread12") == 0 &&
          std::strcmp(d->name, "EDOSpread12") == 0);
    bool noteEffect = false;
    for (const char* const* f = d->features; *f != nullptr; ++f)
        noteEffect |= std::strcmp(*f, CLAP_PLUGIN_FEATURE_NOTE_EFFECT) == 0;
    CHECK(noteEffect);
}

void testNotePorts() {
    Instance r;
    CHECK(r.ok());
    if (!r.ok()) return;
    for (bool input : {true, false}) {
        CHECK(r.notePorts->count(r.plugin, input) == 1);
        clap_note_port_info_t info{};
        CHECK(r.notePorts->get(r.plugin, 0, input, &info));
        CHECK(info.supported_dialects ==
              (CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI2));
        CHECK(info.preferred_dialect == CLAP_NOTE_DIALECT_MIDI2);
        CHECK(info.id == 0);
    }
    Instance w(&kWrapperHost);
    if (!w.ok()) return;
    clap_note_port_info_t info{};
    CHECK(w.notePorts->get(w.plugin, 0, true, &info) && info.preferred_dialect == CLAP_NOTE_DIALECT_MIDI);
}

void testParams() {
    Instance r;
    if (!r.ok()) return;
    CHECK(r.params->count(r.plugin) == 15);
    const char* names[] = {"EDO", "Origin", "Fill", "Step A", "Step A#", "Step B", "Step C", "Step C#",
                           "Step D", "Step D#", "Step E", "Step F", "Step F#", "Step G", "Step G#"};
    for (clap_id i = 0; i < 15; ++i) {
        clap_param_info_t info{};
        CHECK(r.params->get_info(r.plugin, i, &info));
        CHECK(info.id == i && std::strcmp(info.name, names[i]) == 0);
        CHECK((info.flags & CLAP_PARAM_IS_STEPPED) != 0);
        CHECK(r.value(i) == info.default_value);
        if (i >= kStep0) CHECK(info.min_value == 0 && info.max_value == 96 && info.default_value == i - kStep0);
    }
    clap_param_info_t info{};
    CHECK(r.params->get_info(r.plugin, kEdo, &info) && info.min_value == 1 && info.max_value == 96 &&
          info.default_value == 12);
    CHECK(r.params->get_info(r.plugin, kOrigin, &info) && info.default_value == 9 &&
          (info.flags & CLAP_PARAM_IS_ENUM) != 0);
    CHECK(r.params->get_info(r.plugin, kFill, &info) && info.default_value == 1 &&
          (info.flags & CLAP_PARAM_IS_ENUM) != 0);

    char text[64];
    double v = 0;
    CHECK(r.params->value_to_text(r.plugin, kOrigin, 9, text, sizeof(text)) && std::strcmp(text, "A") == 0);
    CHECK(r.params->value_to_text(r.plugin, kFill, 0, text, sizeof(text)) && std::strcmp(text, "Floor") == 0);
    CHECK(r.params->value_to_text(r.plugin, kStep0 + 3, 5, text, sizeof(text)) && std::strcmp(text, "5") == 0);
    CHECK(r.params->text_to_value(r.plugin, kOrigin, "c#", &v) && v == 1);
    CHECK(r.params->text_to_value(r.plugin, kOrigin, "4", &v) && v == 4);
    CHECK(r.params->text_to_value(r.plugin, kFill, "Nearest", &v) && v == 1);
    CHECK(r.params->text_to_value(r.plugin, kEdo, "500", &v) && v == 96);   // clamped
    CHECK(!r.params->text_to_value(r.plugin, kOrigin, "H", &v));

    // Changing the origin renames the steps, through a main-thread rescan.
    gCallbacks = 0;
    gRescanFlags = 0;
    Input in;
    in.param(0, kOrigin, 0);
    r.process(in);
    CHECK(gCallbacks == 1);
    r.plugin->on_main_thread(r.plugin);
    CHECK((gRescanFlags & CLAP_PARAM_RESCAN_INFO) != 0);
    CHECK(r.params->get_info(r.plugin, kStep0, &info) && std::strcmp(info.name, "Step C") == 0);
    CHECK(r.params->get_info(r.plugin, kStep0 + 11, &info) && std::strcmp(info.name, "Step B") == 0);
}

void testDefaultPassesThrough() {
    Instance r;
    if (!r.ok()) return;
    Input in;
    in.midi1(0, 0x90, 61, 100);
    in.midi1(1, 0xB0, 7, 64);
    in.midi1(2, 0xF8, 0, 0);
    in.midi1(3, 0x80, 61, 0);
    in.midi2(4, 0x40903C00, 0x8000'0000);
    const auto out = r.process(in);
    CHECK(out.size() == 5);
    if (out.size() == 5) {
        CHECK(isMidi1(out[0], 0, 0x90, 61, 100));
        CHECK(isMidi1(out[1], 1, 0xB0, 7, 64));
        CHECK(out[2].type == CLAP_EVENT_MIDI && out[2].bytes[0] == 0xF8);
        CHECK(isMidi1(out[3], 3, 0x80, 61, 0));
        // The same pitch, now stated: uw-core's transposeNote79 always writes the attribute.
        CHECK(isMidi2(out[4], 4, 0x40903C03, 0x8000'0000u + (60u << 9)));
    }
}

void testMidi2() {
    Instance r;
    if (!r.ok()) return;
    Input in;
    set19(in);
    in.midi2(5, 0x40904800, 0x8000'0000);   // C5
    in.midi2(6, 0x40804800, 0);
    in.midi2(7, 0x40904500, 0x8000'0000);   // A4, the origin: no attribute needed, but one is set
    const auto out = r.process(in);
    CHECK(out.size() == 3);
    if (out.size() == 3) {
        CHECK(isMidi2(out[0], 5, 0x40904803, 0x8000'0000u + kC5Pitch79));
        CHECK(isMidi2(out[1], 6, 0x40804800, 0));
        CHECK(out[2].words[0] == 0x40904503 && (out[2].words[1] & 0xFFFF) == 69u * 512);
    }
}

void testMidi1Promoted() {
    Instance r;
    if (!r.ok()) return;
    Input in;
    set19(in);
    in.midi1(10, 0x90, 72, 127);   // a fraction: out as MIDI 2.0
    in.midi1(11, 0xA0, 72, 127);   // its poly pressure follows, as MIDI 2.0
    in.midi1(12, 0x80, 72, 0);
    in.midi1(13, 0x90, 69, 100);   // the origin, a whole key: stays MIDI 1.0
    const auto out = r.process(in);
    CHECK(out.size() == 4);
    if (out.size() == 4) {
        CHECK(isMidi2(out[0], 10, 0x40904803, 0xFFFF'0000u + kC5Pitch79));
        CHECK(isMidi2(out[1], 11, 0x40A04800, 0xFFFF'FFFFu));
        CHECK(isMidi2(out[2], 12, 0x40804800, 0));
        CHECK(isMidi1(out[3], 13, 0x90, 69, 100));
    }
}

void testMidi1RoundedWithoutMidi2() {
    // C up to step 7 of 19 from A: 69 + 84/19 = 73.42, rounded to 73. No MIDI 2.0 out, because
    // the host takes none, or because clap-wrapper can't carry it.
    for (const clap_host_t* host : {&kMidi1Host, &kWrapperHost}) {
        Instance r(host);
        if (!r.ok()) return;
        Input in;
        set19(in);
        in.param(0, kStep0 + 3, 7);
        in.midi1(0, 0x90, 72, 100);
        in.midi1(1, 0x80, 72, 0);
        const auto out = r.process(in);
        CHECK(out.size() == 2);
        if (out.size() == 2) {
            CHECK(isMidi1(out[0], 0, 0x90, 73, 100));
            CHECK(isMidi1(out[1], 1, 0x80, 73, 0));
        }
    }
}

void testHeldNoteAcrossBlocks() {
    Instance r;
    if (!r.ok()) return;
    Input a;
    set19(a);
    a.midi2(0, 0x40904800, 0x8000'0000);
    CHECK(r.process(a).size() == 1);

    Input b;
    b.param(0, kEdo, 12);               // back toward 12-EDO while C5 is held
    for (clap_id d = 0; d < 12; ++d) b.param(0, kStep0 + d, d);
    b.midi2(100, 0x40804800, 0);
    b.midi2(200, 0x40904800, 0x8000'0000);
    const auto out = r.process(b);
    CHECK(out.size() == 2);
    if (out.size() == 2) {
        CHECK(isMidi2(out[0], 100, 0x40804800, 0));
        CHECK(out[1].time == 200 && (out[1].words[1] & 0xFFFF) == 72u * 512);
    }
}

void testClapNotes() {
    Instance r;
    if (!r.ok()) return;
    Input in;
    set19(in);
    in.note(CLAP_EVENT_NOTE_ON, 10, 7, 0, 72);
    in.expression(20, CLAP_NOTE_EXPRESSION_VOLUME, 7, 0, 72, 0.5);
    in.note(CLAP_EVENT_NOTE_OFF, 30, 7, 0, 72);
    const auto out = r.process(in);
    CHECK(out.size() == 4);
    if (out.size() == 4) {
        CHECK(isNote(out[0], CLAP_EVENT_NOTE_ON, 10, 7, 0, 72));
        CHECK(isExpression(out[1], 10, CLAP_NOTE_EXPRESSION_TUNING, 7, 0, 72, (1617 - 3 * 512) / 512.0));
        CHECK(isExpression(out[2], 20, CLAP_NOTE_EXPRESSION_VOLUME, 7, 0, 72, 0.5));
        CHECK(isNote(out[3], CLAP_EVENT_NOTE_OFF, 30, 7, 0, 72));
    }
}

struct Sink {
    std::string bytes;
    static int64_t write(const clap_ostream_t* s, const void* buffer, uint64_t size) {
        static_cast<Sink*>(s->ctx)->bytes.append(static_cast<const char*>(buffer), size_t(size));
        return int64_t(size);
    }
};
struct Source {
    std::string bytes;
    size_t pos = 0;
    static int64_t read(const clap_istream_t* s, void* buffer, uint64_t size) {
        auto* self = static_cast<Source*>(s->ctx);
        const size_t n = std::min<size_t>(size_t(size), self->bytes.size() - self->pos);
        std::memcpy(buffer, self->bytes.data() + self->pos, n);
        self->pos += n;
        return int64_t(n);
    }
};

void testState() {
    Sink sink;
    {
        Instance r;
        if (!r.ok()) return;
        Input in;
        set19(in);
        in.param(0, kOrigin, 0);
        in.param(0, kFill, 0);
        r.process(in);
        const clap_ostream_t os = {&sink, Sink::write};
        CHECK(r.state->save(r.plugin, &os));
    }
    CHECK(sink.bytes == "EDOSpread12 1\nedo 19\norigin 0\nfill 0\nstep1 0\nstep2 2\nstep3 3\nstep4 5\n"
                        "step5 6\nstep6 8\nstep7 10\nstep8 11\nstep9 13\nstep10 14\nstep11 16\nstep12 17\nscale None\n");

    Instance r;
    if (!r.ok()) return;
    gRescans = 0;
    gRescanFlags = 0;
    Source source{sink.bytes};
    const clap_istream_t is = {&source, Source::read};
    CHECK(r.state->load(r.plugin, &is));
    CHECK(gRescans == 1 && (gRescanFlags & CLAP_PARAM_RESCAN_VALUES) != 0 &&
          (gRescanFlags & CLAP_PARAM_RESCAN_INFO) != 0);
    CHECK(r.value(kEdo) == 19 && r.value(kOrigin) == 0 && r.value(kFill) == 0 && r.value(kStep0 + 3) == 5);
    Input in;
    in.midi2(0, 0x40904500, 0x8000'0000);   // A4 from C: step 14 of 19, 60 + 168/19
    const auto out = r.process(in);
    CHECK(out.size() == 1 && (out[0].words[1] & 0xFFFF) == 60u * 512 + (2 * 512 * 12 * 14 + 19) / 38);

    // The Scale menu's choice, kept in the state by name; unknown or missing, it's None.
    auto scaleAfterLoading = [&r](const char* text) {
        Source source{text};
        const clap_istream_t is = {&source, Source::read};
        if (!r.state->load(r.plugin, &is)) return std::string("load failed");
        Sink out;
        const clap_ostream_t os = {&out, Sink::write};
        if (!r.state->save(r.plugin, &os)) return std::string("save failed");
        const size_t at = out.bytes.find("\nscale ");
        return at == std::string::npos ? std::string("none") : out.bytes.substr(at + 1, out.bytes.find('\n', at + 1) - at - 1);
    };
    CHECK(scaleAfterLoading("EDOSpread12 1\nedo 12\nscale Lydian dominant\n") == "scale Lydian dominant");
    CHECK(scaleAfterLoading("EDOSpread12 1\nscale Diminished (from the 2nd)\n") == "scale Diminished (from the 2nd)");
    CHECK(scaleAfterLoading("EDOSpread12 1\nscale Lydian\n") == "scale Lydian");   // not Lydian #2 or the rest
    CHECK(scaleAfterLoading("EDOSpread12 1\nscale Hungarian\n") == "scale None");
    CHECK(scaleAfterLoading("EDOSpread12 1\nedo 12\n") == "scale None");

    Source bad{"Retune 1\n"};
    const clap_istream_t badStream = {&bad, Source::read};
    CHECK(!r.state->load(r.plugin, &badStream));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: edospread12-clap-tests /path/to/EDOSpread12.clap\n");
        return 2;
    }
    if (!openLibrary(argv[1])) {
        std::fprintf(stderr, "can't load %s\n", argv[1]);
        return 1;
    }
    testDescriptor();
    testNotePorts();
    testParams();
    testDefaultPassesThrough();
    testMidi2();
    testMidi1Promoted();
    testMidi1RoundedWithoutMidi2();
    testHeldNoteAcrossBlocks();
    testClapNotes();
    testState();
    gEntry->deinit();
    if (failures != 0) {
        std::fprintf(stderr, "EDOSpread12 CLAP: %d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
