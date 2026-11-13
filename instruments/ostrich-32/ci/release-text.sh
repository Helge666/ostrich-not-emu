#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The install instructions, in one place: INSTALL.txt in each zip, and the release notes.
#
#   ci/release-text.sh windows|linux
#   ci/release-text.sh notes VERSION

set -euo pipefail

roms() {
	cat <<'EOF'
You need your own ROM images of a Roland MT-32, CM-32L or CM-32LN: the control ROM, the
PCM (wave) ROM and the reverb ROM. They are recognised by their contents, so the file
names do not matter. The list: instruments/ostrich-32/roms/README.md in the source
repository. No ROMs are included, and none ever will be.

The status line under the panel says which machine runs, or that images are missing and
where they were looked for.

Ostrich 32 runs on 88emu, part of gearmulator by The Usual Suspects (GPL-3.0); its LA32
emulation derives from Nuked-MT32 by nukeykt, its reverb from Munt by Sergey V. Mikayev.
Everyone whose work it builds on is named in README.md and THIRD-PARTY.md. The licence
texts are in the folder licenses.
Source: https://github.com/Helge666/ostrich-not-emu/tree/main/instruments/ostrich-32
EOF
}

windows() {
	cat <<'EOF'
Ostrich 32 for Windows (64-bit)

VST3: copy the folder Ostrich32.vst3 to
  C:\Program Files\Common Files\VST3\
CLAP: copy Ostrich32.clap to
  C:\Program Files\Common Files\CLAP\
Standalone: Ostrich32.exe, anywhere.

ROMs go in
  C:\ProgramData\Ostrich 32\roms\

EOF
	roms
}

linux() {
	cat <<'EOF'
Ostrich 32 for Linux (x86-64)

VST3: copy the folder Ostrich32.vst3 to ~/.vst3/
CLAP: copy Ostrich32.clap to ~/.clap/
Standalone: Ostrich32 (JACK), anywhere.

ROMs go in
  ~/.local/share/Ostrich 32/roms/

EOF
	roms
}

notes() {
	local version="$1"
	cat <<EOF
Ostrich 32 ${version}: an emulation of the Roland MT-32, CM-32L and CM-32LN, running the
unit's original firmware on 88emu from [gearmulator](https://github.com/dsp56300/gearmulator)
by The Usual Suspects. All credit for the emulation goes to them, to nukeykt (Nuked-MT32)
and to Sergey V. Mikayev (Munt).

| File | Platform | Contents |
|---|---|---|
| \`Ostrich32-Windows.zip\` | Windows x86-64 | VST3, CLAP, standalone |
| \`Ostrich32-Linux.zip\` | Linux x86-64 | VST3, CLAP, standalone (JACK) |

Each zip has an INSTALL.txt. You need your own ROM images; none are included.

## How well tested

- **Windows VST3:** tested in Cubase.
- **Everything else: untested.** It builds without errors, and an automatic test loads the
  Linux CLAP once (without ROMs or panel).

Please report what works and what does not (DAW, OS, format) as an
[issue](https://github.com/Helge666/ostrich-not-emu/issues), naming Ostrich 32. Please do
not report Ostrich 32's problems to gearmulator's authors.
EOF
}

case "${1:-}" in
	windows) windows ;;
	linux)   linux ;;
	notes)   notes "${2:?version}" ;;
	*) echo "usage: $0 windows|linux|notes VERSION" >&2; exit 2 ;;
esac
