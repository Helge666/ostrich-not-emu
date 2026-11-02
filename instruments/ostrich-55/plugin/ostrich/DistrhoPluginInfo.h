// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// Ostrich 55 (O-55), the SC-55.  IDs of its own, apart from Ostrich 220's.
#define DISTRHO_PLUGIN_BRAND   "Ostrich 55"
#define DISTRHO_PLUGIN_NAME    "Ostrich 55"
#define DISTRHO_PLUGIN_URI     "urn:ostrich55:sc55"
#define DISTRHO_PLUGIN_CLAP_ID "org.ostrich55.sc55"

#define DISTRHO_PLUGIN_BRAND_ID  O055
#define DISTRHO_PLUGIN_UNIQUE_ID SC55

#define DISTRHO_PLUGIN_HAS_UI           1
#define DISTRHO_PLUGIN_IS_SYNTH         1
#define DISTRHO_PLUGIN_IS_RT_SAFE       1
#define DISTRHO_PLUGIN_NUM_INPUTS       0
#define DISTRHO_PLUGIN_NUM_OUTPUTS      2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_LATENCY     1

// The LCD goes to the UI as one blob through updateStateValue() (our DPF fork gives JACK,
// CLAP and VST3 that path).
#define DISTRHO_PLUGIN_WANT_STATE       1
#define DISTRHO_PLUGIN_WANT_FULL_STATE  1

#define DISTRHO_UI_USE_NANOVG           1
// The unit's front (OstrichUI.cpp, 1350 x 250 design units) and the bar under it, both
// sides divisible by 4: at every Windows scaling in quarter steps the size is whole pixels,
// and Cubase has nothing to round (Ostrich 220's lesson).
#define DISTRHO_UI_DEFAULT_WIDTH        1404
#define DISTRHO_UI_DEFAULT_HEIGHT       288
#define DISTRHO_UI_USER_RESIZABLE       1
#define DISTRHO_UI_FILE_BROWSER         1     // the ROM setup

#define DISTRHO_PLUGIN_LV2_CATEGORY "lv2:InstrumentPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Instrument|Synth"
#define DISTRHO_PLUGIN_CLAP_FEATURES "instrument", "synthesizer", "stereo"

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
