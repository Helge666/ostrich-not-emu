// SPDX-License-Identifier: GPL-3.0-or-later
/*
    Ostrich != Emu (shared/tools) -- loads an instrument's Linux CLAP without a host or a
    display, plays notes through it and writes what comes out, so that the PLUGIN is tested
    (ROM search, boot, MIDI timing, resampler, outputs, state) and not only its core.  The
    CI's smoke test (tools/ci/smoke.sh), and a way to render without a host.

        ostrich_clap_render Plugin.clap out.wav [options]

    --notes LIST     "key[@start][:length],..." (seconds): start 1.0 and length --hold
                     where not given; "" plays none.  Default "60".
    --hold S         the length of a note that gives none (default 2.0)
    --channel N      MIDI channel, 1-16 (default 1)
    --velocity N     1-127 (default 100)
    --rate HZ        default 48000
    --seconds S      how long to render (default 6)
    --param LIST     "id=value,...": parameters set before the first block
    --press LIST     "id[@sec],...": parameters pressed (1.0) for 0.1 s, a bare id at 2.0 s
                     -- the panel keys of Ostrich 32, 55 and 220
    --midi LIST      "hex[@sec],...": short MIDI messages (a program change "c205@1.5"),
                     a bare one at 0.5 s
    --load FILE      a state saved before (--save), loaded before the first block
    --save FILE      the state after the last block goes to this file
    --show-state     prints the state (DPF writes text; what is not printable as '.')
    --settle S       waits this long before saving the state (work on the plugin's threads,
                     a SysEx file in Ostrich 220)
    --raw FILE       every output channel to this file, interleaved, 32-bit floats
    --midi-out FILE  what the plugin sends to its MIDI output, as raw bytes
    --realtime       as slow as playback (a plugin's once-a-second debug log)

    Every output bus gets buffers of its own (all switched on, DIRECT OUTs plugged in).  The
    WAV (16 bit) holds every output channel where there are at most 8, else the first bus
    (the mix); --raw has them all.  Printed: the plugin, its output buses, the state's size,
    the plugin's MIDI out, the peak of every channel.

    Built with:
        gcc -std=gnu11 -O2 -o ostrich_clap_render shared/tools/ostrich_clap_render.c \
            -Ithird_party/dpf/distrho/src -ldl -lm
*/

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "clap/entry.h"
#include "clap/plugin-factory.h"
#include "clap/ext/audio-ports.h"
#include "clap/ext/state.h"

#define FRAMES 256
#define MAX_BUSES 32
#define MAX_CHANNELS 64
#define MAX_EVENTS 512
#define MAX_ITEMS 64
#define WAV_MAX_CHANNELS 8

static const void *host_get_extension(const clap_host_t *h, const char *id) { (void)h; (void)id; return NULL; }
static void host_noop(const clap_host_t *h) { (void)h; }
static const clap_host_t host = {
	CLAP_VERSION, NULL, "ostrich_clap_render", "Ostrich", "", "0",
	host_get_extension, host_noop, host_noop, host_noop
};

// ---- events in --------------------------------------------------------------------------

typedef union { clap_event_header_t header; clap_event_note_t note; clap_event_param_value_t param; clap_event_midi_t midi; } event_t;
typedef struct { event_t ev[MAX_EVENTS]; uint32_t n; } events_t;
static uint32_t in_size(const clap_input_events_t *l) { return ((const events_t *)l->ctx)->n; }
static const clap_event_header_t *in_get(const clap_input_events_t *l, uint32_t i)
{ return &((const events_t *)l->ctx)->ev[i].header; }

static void add_param(events_t *evs, uint32_t time, uint32_t id, double value)
{
	if (evs->n >= MAX_EVENTS)
		return;
	clap_event_param_value_t *e = &evs->ev[evs->n ++].param;
	memset(e, 0, sizeof(*e));
	e->header.size = sizeof(*e);
	e->header.time = time;
	e->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
	e->header.type = CLAP_EVENT_PARAM_VALUE;
	e->param_id = id;
	e->note_id = -1; e->port_index = -1; e->channel = -1; e->key = -1;
	e->value = value;
}

static void add_note(events_t *evs, uint32_t time, bool on, int channel, int key, double velocity)
{
	if (evs->n >= MAX_EVENTS)
		return;
	clap_event_note_t *e = &evs->ev[evs->n ++].note;
	memset(e, 0, sizeof(*e));
	e->header.size = sizeof(*e);
	e->header.time = time;
	e->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
	e->header.type = on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF;
	e->note_id = -1; e->port_index = 0; e->channel = (int16_t)channel; e->key = (int16_t)key;
	e->velocity = velocity;
}

static void add_midi(events_t *evs, uint32_t time, const uint8_t data[3])
{
	if (evs->n >= MAX_EVENTS)
		return;
	clap_event_midi_t *e = &evs->ev[evs->n ++].midi;
	memset(e, 0, sizeof(*e));
	e->header.size = sizeof(*e);
	e->header.time = time;
	e->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
	e->header.type = CLAP_EVENT_MIDI;
	e->port_index = 0;
	memcpy(e->data, data, 3);
}

// CLAP wants a block's events in time order; those at one time keep theirs.
static void sort_events(events_t *evs)
{
	for (uint32_t a = 1; a < evs->n; a ++)
		for (uint32_t b = a; b > 0 && evs->ev[b].header.time < evs->ev[b - 1].header.time; b --)
		{
			const event_t t = evs->ev[b]; evs->ev[b] = evs->ev[b - 1]; evs->ev[b - 1] = t;
		}
}

// ---- the plugin's MIDI OUT: counted, the first printed, all to --midi-out ----------------

static FILE *midi_out_file;
static uint64_t render_pos;
static unsigned midi_out_events, midi_out_sysex;

static bool out_push(const clap_output_events_t *l, const clap_event_header_t *e)
{
	(void)l;
	if (e->space_id != CLAP_CORE_EVENT_SPACE_ID)
		return true;
	const uint8_t *bytes = NULL; uint32_t n = 0;
	if (e->type == CLAP_EVENT_MIDI)
	{
		const clap_event_midi_t *m = (const clap_event_midi_t *)e;
		bytes = m->data;
		n = (m->data[0] & 0xF0) == 0xC0 || (m->data[0] & 0xF0) == 0xD0 ? 2 : m->data[0] >= 0xF0 ? 1 : 3;
	}
	else if (e->type == CLAP_EVENT_MIDI_SYSEX)
	{
		const clap_event_midi_sysex_t *m = (const clap_event_midi_sysex_t *)e;
		bytes = m->buffer; n = m->size;
		midi_out_sysex ++;
	}
	else
		return true;
	if (midi_out_events ++ < 20)
	{
		printf("midi out @%llu:", (unsigned long long)(render_pos + e->time));
		for (uint32_t i = 0; i < n && i < 12; i ++)
			printf(" %02X", bytes[i]);
		printf(n > 12 ? " ... (%u bytes)\n" : "\n", n);
	}
	if (midi_out_file)
		fwrite(bytes, 1, n, midi_out_file);
	return true;
}

// ---- the state --------------------------------------------------------------------------

typedef struct { uint8_t *p; size_t n, cap; } buf_t;
static int64_t ostream_write(const clap_ostream_t *s, const void *d, uint64_t n)
{
	buf_t *b = (buf_t *)s->ctx;
	if (b->n + n > b->cap) { b->cap = (b->n + n) * 2; b->p = realloc(b->p, b->cap); }
	memcpy(b->p + b->n, d, n); b->n += n; return (int64_t)n;
}
static int64_t istream_read(const clap_istream_t *s, void *d, uint64_t n)
{
	buf_t *b = (buf_t *)s->ctx;
	const uint64_t left = b->cap - b->n, k = n < left ? n : left;
	memcpy(d, b->p + b->n, k); b->n += k; return (int64_t)k;
}

static bool read_file(const char *path, buf_t *b)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	fseek(f, 0, SEEK_END); b->cap = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
	b->p = malloc(b->cap ? b->cap : 1); b->n = 0;
	const bool ok = fread(b->p, 1, b->cap, f) == b->cap;
	fclose(f);
	return ok;
}

// ---- the command line -------------------------------------------------------------------

static int usage(const char *self)
{
	fprintf(stderr, "usage: %s Plugin.clap out.wav [--notes key@sec:len,...] [--hold s] [--channel n]\n"
	                "       [--velocity n] [--rate hz] [--seconds s] [--param id=value,...] [--press id@sec,...]\n"
	                "       [--midi hex@sec,...]\n"
	                "       [--load file] [--save file] [--show-state] [--settle s] [--raw file]\n"
	                "       [--midi-out file] [--realtime]       (shared/tools/ostrich_clap_render.c)\n", self);
	return 2;
}

int main(int argc, char **argv)
{
	if (argc < 3 || argv[1][0] == '-' || argv[2][0] == '-')
		return usage(argv[0]);
	const char *notes_arg = "60", *param_arg = "", *press_arg = "", *midi_arg = "";
	const char *load = NULL, *save = NULL, *raw = NULL, *midi_out = NULL;
	double hold = 2.0, rate = 48000.0, seconds = 6.0, settle = 0.0;
	int channel = 1, velocity = 100;
	bool show_state = false, realtime = false;
	for (int i = 3; i < argc; i ++)
	{
		const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (strcmp(a, "--show-state") == 0) { show_state = true; continue; }
		if (strcmp(a, "--realtime") == 0) { realtime = true; continue; }
		if (!v) { fprintf(stderr, "%s wants a value\n", a); return usage(argv[0]); }
		i ++;
		if (strcmp(a, "--notes") == 0) notes_arg = v;
		else if (strcmp(a, "--hold") == 0) hold = atof(v);
		else if (strcmp(a, "--channel") == 0) channel = atoi(v);
		else if (strcmp(a, "--velocity") == 0) velocity = atoi(v);
		else if (strcmp(a, "--rate") == 0) rate = atof(v);
		else if (strcmp(a, "--seconds") == 0) seconds = atof(v);
		else if (strcmp(a, "--param") == 0) param_arg = v;
		else if (strcmp(a, "--press") == 0) press_arg = v;
		else if (strcmp(a, "--midi") == 0) midi_arg = v;
		else if (strcmp(a, "--load") == 0) load = v;
		else if (strcmp(a, "--save") == 0) save = v;
		else if (strcmp(a, "--settle") == 0) settle = atof(v);
		else if (strcmp(a, "--raw") == 0) raw = v;
		else if (strcmp(a, "--midi-out") == 0) midi_out = v;
		else { fprintf(stderr, "unknown option %s\n", a); return usage(argv[0]); }
	}
	if (channel < 1 || channel > 16 || velocity < 1 || velocity > 127 || rate < 8000.0 || seconds <= 0.0)
	{
		fprintf(stderr, "channel 1-16, velocity 1-127, rate >= 8000, seconds > 0\n");
		return 2;
	}

	struct { int key; double at, len; } notes[MAX_ITEMS]; int nnotes = 0;
	for (char *t = strtok(strdup(notes_arg), ","); t && nnotes < MAX_ITEMS; t = strtok(NULL, ","))
	{
		const char *at = strchr(t, '@'), *len = strchr(t, ':');
		notes[nnotes].key = atoi(t);
		notes[nnotes].at = at ? atof(at + 1) : 1.0;
		notes[nnotes ++].len = len ? atof(len + 1) : hold;
	}
	struct { uint32_t id; double value; } params[MAX_ITEMS]; int nparams = 0;
	for (char *t = strtok(strdup(param_arg), ","); t && nparams < MAX_ITEMS; t = strtok(NULL, ","))
	{
		const char *eq = strchr(t, '=');
		if (!eq) continue;
		params[nparams].id = (uint32_t)atoi(t);
		params[nparams ++].value = atof(eq + 1);
	}
	struct { uint32_t id; double at; } presses[MAX_ITEMS]; int npresses = 0;
	for (char *t = strtok(strdup(press_arg), ","); t && npresses < MAX_ITEMS; t = strtok(NULL, ","))
	{
		const char *at = strchr(t, '@');
		presses[npresses].id = (uint32_t)atoi(t);
		presses[npresses ++].at = at ? atof(at + 1) : 2.0;
	}

	struct { uint8_t data[3]; double at; } midis[MAX_ITEMS]; int nmidis = 0;
	for (char *t = strtok(strdup(midi_arg), ","); t && nmidis < MAX_ITEMS; t = strtok(NULL, ","))
	{
		const char *at = strchr(t, '@');
		unsigned b[3] = { 0, 0, 0 };
		if (sscanf(t, "%2x%2x%2x", &b[0], &b[1], &b[2]) < 2) continue;
		for (int k = 0; k < 3; k ++) midis[nmidis].data[k] = (uint8_t)b[k];
		midis[nmidis ++].at = at ? atof(at + 1) : 0.5;
	}

	// ---- the plugin ----------------------------------------------------------------------

	void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 1; }
	const clap_plugin_entry_t *entry = dlsym(lib, "clap_entry");
	if (!entry) { fprintf(stderr, "no clap_entry\n"); return 1; }
	entry->init(argv[1]);
	const clap_plugin_factory_t *f = entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
	const clap_plugin_descriptor_t *d = f->get_plugin_descriptor(f, 0);
	printf("plugin: %s (%s) version %s\n", d->name, d->id, d->version);
	const clap_plugin_t *p = f->create_plugin(f, &host, d->id);
	if (!p->init(p)) { fprintf(stderr, "init failed\n"); return 1; }

	// The output buses, as a host would see them.
	const clap_plugin_audio_ports_t *ap = p->get_extension(p, CLAP_EXT_AUDIO_PORTS);
	uint32_t nbus = ap ? ap->count(p, false) : 0, width[MAX_BUSES], nchan = 0;
	char busName[MAX_BUSES][CLAP_NAME_SIZE];
	if (nbus > MAX_BUSES) nbus = MAX_BUSES;
	for (uint32_t b = 0; b < nbus; b ++)
	{
		clap_audio_port_info_t info;
		memset(&info, 0, sizeof(info));
		ap->get(p, b, false, &info);
		width[b] = info.channel_count;
		nchan += info.channel_count;
		snprintf(busName[b], sizeof(busName[b]), "%s", info.name);
		printf("output bus %u: %s, %u channel%s, type %s\n", b, info.name, info.channel_count,
		       info.channel_count == 1 ? "" : "s", info.port_type ? info.port_type : "none");
	}
	if (nbus == 0)
	{
		nbus = 1; width[0] = nchan = 2;
		snprintf(busName[0], sizeof(busName[0]), "out");
	}
	printf("outputs: %u buses, %u channels\n", nbus, nchan);
	if (nchan > MAX_CHANNELS) { fprintf(stderr, "more than %d output channels\n", MAX_CHANNELS); return 1; }

	const clap_plugin_state_t *st = p->get_extension(p, CLAP_EXT_STATE);
	if (load)
	{
		buf_t b = { 0 };
		if (!st || !read_file(load, &b)) { fprintf(stderr, "cannot load the state %s\n", load); return 1; }
		clap_istream_t is = { &b, istream_read };
		printf("state: %s\n", st->load(p, &is) ? "loaded" : "LOAD FAILED");
		free(b.p);
	}
	if (!p->activate(p, rate, 1, FRAMES)) { fprintf(stderr, "activate failed\n"); return 1; }
	if (midi_out && !(midi_out_file = fopen(midi_out, "wb"))) { fprintf(stderr, "cannot write %s\n", midi_out); return 1; }
	p->start_processing(p);

	// ---- the render ----------------------------------------------------------------------

	const uint32_t total = (uint32_t)(rate * seconds);
	float *out[MAX_CHANNELS];
	for (uint32_t c = 0; c < nchan; c ++)
		out[c] = calloc(total + FRAMES, sizeof(float));
	static events_t evs;
	for (uint32_t pos = 0; pos < total; pos += FRAMES)
	{
		evs.n = 0;
		for (int j = 0; pos == 0 && j < nparams; j ++)
			add_param(&evs, 0, params[j].id, params[j].value);
		for (int j = 0; j < nnotes; j ++)
			for (int k = 0; k < 2; k ++)
			{
				const uint32_t at = (uint32_t)(rate * (notes[j].at + (k ? notes[j].len : 0.0)));
				if (at >= pos && at < pos + FRAMES)
					add_note(&evs, at - pos, !k, channel - 1, notes[j].key, velocity / 127.0);
			}
		for (int j = 0; j < npresses; j ++)
			for (int k = 0; k < 2; k ++)
			{
				const uint32_t at = (uint32_t)(rate * (presses[j].at + (k ? 0.1 : 0.0)));
				if (at >= pos && at < pos + FRAMES)
					add_param(&evs, at - pos, presses[j].id, k ? 0.0 : 1.0);
			}
		for (int j = 0; j < nmidis; j ++)
		{
			const uint32_t at = (uint32_t)(rate * midis[j].at);
			if (at >= pos && at < pos + FRAMES)
				add_midi(&evs, at - pos, midis[j].data);
		}
		sort_events(&evs);

		clap_input_events_t in = { &evs, in_size, in_get };
		clap_output_events_t oev = { NULL, out_push };
		float *chans[MAX_CHANNELS];
		clap_audio_buffer_t ob[MAX_BUSES];
		for (uint32_t b = 0, c = 0; b < nbus; b ++)
		{
			ob[b] = (clap_audio_buffer_t){ chans + c, NULL, width[b], 0, 0 };
			for (uint32_t k = 0; k < width[b]; k ++, c ++)
				chans[c] = out[c] + pos;
		}
		clap_process_t proc = { (int64_t)pos, FRAMES, NULL, NULL, ob, 0, nbus, &in, &oev };
		render_pos = pos;
		p->process(p, &proc);
		if (realtime)
			usleep((useconds_t)(1e6 * FRAMES / rate));
	}

	if (settle > 0.0)
		usleep((useconds_t)(settle * 1e6));
	if (st)
	{
		buf_t b = { 0 };
		clap_ostream_t os = { &b, ostream_write };
		const bool ok = st->save(p, &os);
		printf("state: %s, %zu bytes\n", ok ? "saved" : "FAILED", b.n);
		if (show_state)
		{
			for (size_t i = 0; i < b.n; i ++)
				putchar(b.p[i] == '\n' || (b.p[i] >= 0x20 && b.p[i] < 0x7F) ? b.p[i] : '.');
			putchar('\n');
		}
		if (save)
		{
			FILE *sf = fopen(save, "wb");
			if (!sf || fwrite(b.p, 1, b.n, sf) != b.n) { fprintf(stderr, "cannot write %s\n", save); return 1; }
			fclose(sf);
		}
		free(b.p);
	}
	printf("midi out: %u events, %u of them SysEx\n", midi_out_events, midi_out_sysex);
	if (midi_out_file)
		fclose(midi_out_file);
	p->stop_processing(p);
	p->deactivate(p);
	p->destroy(p);
	entry->deinit();

	// ---- what came out -------------------------------------------------------------------

	const uint32_t nwav = nchan <= WAV_MAX_CHANNELS ? nchan : width[0];
	FILE *w = fopen(argv[2], "wb");
	if (!w) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
	const uint32_t bytes = total * 2 * nwav;
	uint32_t v; uint16_t s;
	fwrite("RIFF", 1, 4, w); v = 36 + bytes; fwrite(&v, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w);
	v = 16; fwrite(&v, 4, 1, w); s = 1; fwrite(&s, 2, 1, w); s = (uint16_t)nwav; fwrite(&s, 2, 1, w);
	v = (uint32_t)rate; fwrite(&v, 4, 1, w); v = (uint32_t)rate * 2 * nwav; fwrite(&v, 4, 1, w);
	s = (uint16_t)(2 * nwav); fwrite(&s, 2, 1, w); s = 16; fwrite(&s, 2, 1, w);
	fwrite("data", 1, 4, w); fwrite(&bytes, 4, 1, w);
	for (uint32_t i = 0; i < total; i ++)
		for (uint32_t c = 0; c < nwav; c ++)
		{
			int q = (int)(out[c][i] * 32767.0f); if (q > 32767) q = 32767; if (q < -32768) q = -32768;
			const int16_t q16 = (int16_t)q; fwrite(&q16, 2, 1, w);
		}
	fclose(w);
	printf("wrote %s, %u channel%s%s\n", argv[2], nwav, nwav == 1 ? "" : "s",
	       nwav < nchan ? " (the first bus; --raw has every one)" : "");
	if (raw)
	{
		FILE *r = fopen(raw, "wb");
		if (!r) { fprintf(stderr, "cannot write %s\n", raw); return 1; }
		for (uint32_t i = 0; i < total; i ++)
			for (uint32_t c = 0; c < nchan; c ++)
				fwrite(&out[c][i], sizeof(float), 1, r);
		fclose(r);
	}
	for (uint32_t b = 0, c = 0; b < nbus; b ++)
	{
		printf("peak %s:", busName[b]);
		for (uint32_t k = 0; k < width[b]; k ++, c ++)
		{
			float peak = 0;
			for (uint32_t i = 0; i < total; i ++)
			{
				const float x = out[c][i] < 0 ? -out[c][i] : out[c][i];
				if (x > peak) peak = x;
			}
			printf(" %.3f", peak);
		}
		printf("\n");
	}
	return 0;
}
