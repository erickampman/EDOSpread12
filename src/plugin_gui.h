// plugin_gui.h — the seam between the plugin (src/plugin.cpp, the raw CLAP plugin) and its editor
// (src/UI/EDOSpread12UI.cpp, DPF + NanoVG), as NoteGran's and StruckSynth's. Neither side includes
// the other's internals; they meet only at these C-linkage functions.
//
// Provided by plugin.cpp, called by the editor:
//   es_plugin_*   per-instance plumbing
//   es_param_*    a parameter's description, value and text, and edits with gestures
//   es_held_keys  which keys are held on the input, for the editor to light
// Provided by the editor, called by plugin.cpp:
//   es_gui_extension   the CLAP gui vtable for this platform
//   es_gui_on_timer    CLAP timer-support: pumps the DPF/pugl event loop and refreshes the editor
//
// Host automation needs no push: the editor reads every value on its timer and shows what changed.

#pragma once

#include <clap/clap.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- provided by plugin.cpp, called by the editor --------------------------------------------

const clap_host_t* es_plugin_host(const clap_plugin_t* plugin);

// The editor owns its per-instance state; plugin.cpp only holds the pointer for it.
void  es_plugin_set_gui(const clap_plugin_t* plugin, void* gui);
void* es_plugin_get_gui(const clap_plugin_t* plugin);

typedef struct es_param_info {
    const char*        name;      // as the host shows it (a step's follows the origin: "Step A")
    double             min, max, def;
    const char* const* labels;    // a menu's, one per step from min; null for a number
} es_param_info_t;

uint32_t es_param_count(const clap_plugin_t* plugin);
bool     es_param_info(const clap_plugin_t* plugin, uint32_t index, es_param_info_t* info);
double   es_param_value(const clap_plugin_t* plugin, uint32_t index);
void     es_param_text(const clap_plugin_t* plugin, uint32_t index, double value, char* out, uint32_t size);

// A drag is begin_gesture, set_from_gui (repeatedly), end_gesture. plugin.cpp applies each value at
// its next process() (or flush) and tells the host, so the move is recorded as automation.
void es_param_begin_gesture(const clap_plugin_t* plugin, uint32_t index);
void es_param_set_from_gui(const clap_plugin_t* plugin, uint32_t index, double value);
void es_param_end_gesture(const clap_plugin_t* plugin, uint32_t index);

// The input keys held now, on any channel: bit k of words[k / 64].
void es_held_keys(const clap_plugin_t* plugin, uint64_t words[2]);

// --- provided by the editor, called by plugin.cpp --------------------------------------------
const clap_plugin_gui_t* es_gui_extension(void);
void                     es_gui_on_timer(const clap_plugin_t* plugin, clap_id timer_id);

#ifdef __cplusplus
}
#endif
