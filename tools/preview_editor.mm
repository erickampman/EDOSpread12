// tools/preview_editor.mm — opens EDOSpread12's editor in a window of its own, with no DAW: a
// minimal CLAP host (gui, timer-support, params) on macOS, for looking at and trying the editor.
// Nothing plays; the editor's edits are flushed into the plugin as a host would.
//
// Built as "EDOSpread12 Preview.app" (target preview-editor). Usage:
//   "build/EDOSpread12 Preview.app/Contents/MacOS/EDOSpread12 Preview" /path/to/EDOSpread12.clap [id=value ...]
//   e.g. … build/EDOSpread12.clap 0=19   (EDO 19; the steps stay as they are)
// Parameter ids are src/plugin.cpp's: 0 EDO, 1 Origin, 2 Fill, 3 … 14 the steps from the origin.
// "scale=Lydian dominant" picks a Scale menu entry by name (SpreadCore.h, kScales), through the state.

#import <Cocoa/Cocoa.h>

#include <clap/clap.h>
#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

const clap_plugin_t* gPlugin = nullptr;
std::map<clap_id, NSTimer*> gTimers;
clap_id gNextTimer = 1;
bool gFlushQueued = false;

// --- params: flush the editor's edits on the main thread, outside the call that asked -----------

uint32_t noEvents(const clap_input_events_t*) { return 0; }
const clap_event_header_t* noEvent(const clap_input_events_t*, uint32_t) { return nullptr; }
bool dropEvent(const clap_output_events_t*, const clap_event_header_t*) { return true; }

void flushNow(const clap_input_events_t* in) {
    const auto* params = static_cast<const clap_plugin_params_t*>(gPlugin->get_extension(gPlugin, CLAP_EXT_PARAMS));
    const clap_output_events_t out = {nullptr, dropEvent};
    if (params != nullptr) params->flush(gPlugin, in, &out);
}

void hostRescan(const clap_host_t*, clap_param_rescan_flags) {}
void hostClear(const clap_host_t*, clap_id, clap_param_clear_flags) {}
void hostRequestFlush(const clap_host_t*) {
    if (gFlushQueued) return;
    gFlushQueued = true;
    dispatch_async(dispatch_get_main_queue(), ^{
        gFlushQueued = false;
        const clap_input_events_t in = {nullptr, noEvents, noEvent};
        flushNow(&in);
    });
}
const clap_host_params_t kHostParams = {hostRescan, hostClear, hostRequestFlush};

// --- timer-support ------------------------------------------------------------------------------

bool registerTimer(const clap_host_t*, uint32_t ms, clap_id* id) {
    const clap_id timerId = gNextTimer++;
    const auto* ext = static_cast<const clap_plugin_timer_support_t*>(
        gPlugin->get_extension(gPlugin, CLAP_EXT_TIMER_SUPPORT));
    if (ext == nullptr) return false;
    gTimers[timerId] = [NSTimer scheduledTimerWithTimeInterval:ms / 1000.0
                                                       repeats:YES
                                                         block:^(NSTimer*) { ext->on_timer(gPlugin, timerId); }];
    *id = timerId;
    return true;
}
bool unregisterTimer(const clap_host_t*, clap_id id) {
    auto it = gTimers.find(id);
    if (it == gTimers.end()) return false;
    [it->second invalidate];
    gTimers.erase(it);
    return true;
}
const clap_host_timer_support_t kHostTimer = {registerTimer, unregisterTimer};

uint32_t hostDialects(const clap_host_t*) { return CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI2; }
void hostNotePortsRescan(const clap_host_t*, uint32_t) {}
const clap_host_note_ports_t kHostNotePorts = {hostDialects, hostNotePortsRescan};

const void* hostGetExtension(const clap_host_t*, const char* id) {
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &kHostParams;
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) return &kHostTimer;
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) return &kHostNotePorts;
    return nullptr;
}
void hostNoop(const clap_host_t*) {}
void hostCallback(const clap_host_t*) {
    dispatch_async(dispatch_get_main_queue(), ^{ gPlugin->on_main_thread(gPlugin); });
}

const clap_host_t kHost = {
    .clap_version     = CLAP_VERSION_INIT,
    .host_data        = nullptr,
    .name             = "EDOSpread12 preview",
    .vendor           = "Unlikelyware",
    .url              = "",
    .version          = "0",
    .get_extension    = hostGetExtension,
    .request_restart  = hostNoop,
    .request_process  = hostNoop,
    .request_callback = hostCallback,
};

// Parameter settings from the command line, as one flush of param events.
struct Settings {
    std::vector<clap_event_param_value_t> events;
    static uint32_t size(const clap_input_events_t* l) { return uint32_t(static_cast<Settings*>(l->ctx)->events.size()); }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i) {
        return &static_cast<Settings*>(l->ctx)->events[i].header;
    }
};

} // namespace

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc < 2) {
            std::fprintf(stderr, "usage: preview-editor /path/to/EDOSpread12.clap [id=value ...]\n");
            return 2;
        }
        std::string path = argv[1];
        const std::string name = path.substr(path.find_last_of('/') + 1);
        std::string binary = path + "/Contents/MacOS/" + name.substr(0, name.size() - 5);
        void* lib = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (lib == nullptr) { std::fprintf(stderr, "can't load %s: %s\n", binary.c_str(), dlerror()); return 1; }
        const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
        if (entry == nullptr || !entry->init(path.c_str())) return 1;
        const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
        gPlugin = factory->create_plugin(factory, &kHost, desc->id);
        if (gPlugin == nullptr || !gPlugin->init(gPlugin)) return 1;

        Settings settings;
        for (int i = 2; i < argc; ++i) {
            if (std::strncmp(argv[i], "scale=", 6) == 0) {   // not a parameter: a line of the state
                struct Text { std::string bytes; size_t pos = 0; } text{"EDOSpread12 1\nscale " + std::string(argv[i] + 6) + "\n"};
                const clap_istream_t is = {&text, [](const clap_istream_t* s, void* buffer, uint64_t size) -> int64_t {
                    auto* t = static_cast<Text*>(s->ctx);
                    const size_t n = std::min<size_t>(size_t(size), t->bytes.size() - t->pos);
                    std::memcpy(buffer, t->bytes.data() + t->pos, n);
                    t->pos += n;
                    return int64_t(n);
                }};
                const auto* state = static_cast<const clap_plugin_state_t*>(gPlugin->get_extension(gPlugin, CLAP_EXT_STATE));
                if (state != nullptr) state->load(gPlugin, &is);
                continue;
            }
            clap_event_param_value_t ev{};
            ev.header   = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
            ev.param_id = clap_id(std::atoi(argv[i]));
            const char* eq = std::strchr(argv[i], '=');
            if (eq == nullptr) continue;
            ev.value    = std::atof(eq + 1);
            ev.note_id = -1; ev.port_index = -1; ev.channel = -1; ev.key = -1;
            settings.events.push_back(ev);
        }
        const clap_input_events_t in = {&settings, Settings::size, Settings::get};
        flushNow(&in);

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        const auto* gui = static_cast<const clap_plugin_gui_t*>(gPlugin->get_extension(gPlugin, CLAP_EXT_GUI));
        if (gui == nullptr || !gui->create(gPlugin, CLAP_WINDOW_API_COCOA, false)) return 1;
        uint32_t w = 0, h = 0;
        gui->get_size(gPlugin, &w, &h);
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(120, 120, w, h)
                                                       styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [window setTitle:@"EDOSpread12 preview"];
        [window setReleasedWhenClosed:NO];
        clap_window_t parent{CLAP_WINDOW_API_COCOA, {}};
        parent.cocoa = (__bridge void*)[window contentView];
        gui->set_parent(gPlugin, &parent);
        gui->set_size(gPlugin, w, h);
        gui->show(gPlugin);
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification
                                                          object:window
                                                           queue:nil
                                                      usingBlock:^(NSNotification*) { [NSApp terminate:nil]; }];
        [NSApp run];
        return 0;
    }
}
