#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The install instructions, in one place: INSTALL.txt in each zip, and the release notes.
#
#   .github/release-text.sh windows|linux
#   .github/release-text.sh notes VERSION

set -euo pipefail

roms() {
	cat <<'EOF'
You need your own ROM images of an SC-55 (mk1), firmware v1.00, 1.10, 1.20, 1.21 or
2.00: the CPU ROM, the program ROM and the three wave ROMs. They are recognised by their
SHA-256, so the file names do not matter. The list, with checksums: roms/README.md in the
source repository. No ROMs are included, and none ever will be.

The easy way: load the plugin. While images are missing, the panel shows them; click
CHOOSE FILE and pick any one of your ROM images, or a zip with them. The others in the
same folder are found by their contents and copied to the ROM folder below; then it
starts.

The status line under the panel says which set runs, or which images are missing and
where they were looked for.

Ostrich 55 runs on Nuked-SC55 by nukeykt and J.C. Moyer (GPL-2.0-or-later); everyone
whose work it builds on is named in README.md and THIRD-PARTY.md. The licence texts are
in the folder licenses. Source: https://github.com/Helge666/ostrich55
EOF
}

windows() {
	cat <<'EOF'
Ostrich 55 for Windows (64-bit)

VST3: copy the folder Ostrich55.vst3 to
  C:\Program Files\Common Files\VST3\
CLAP: copy Ostrich55.clap to
  C:\Program Files\Common Files\CLAP\
Standalone: Ostrich55.exe, anywhere. Under the panel it has a MIDI IN field (pick your
keyboard's port) and a BUFFER field (smaller responds faster).

ROMs go in (the panel copies them there for you)
  C:\ProgramData\Ostrich 55\roms\

EOF
	roms
}

linux() {
	cat <<'EOF'
Ostrich 55 for Linux (x86-64)

VST3: copy the folder Ostrich55.vst3 to ~/.vst3/
CLAP: copy Ostrich55.clap to ~/.clap/
Standalone: Ostrich55 (JACK), anywhere.

ROMs go in (the panel copies them there for you)
  ~/.local/share/Ostrich 55/roms/

EOF
	roms
}

notes() {
	local version="$1"
	cat <<EOF
Ostrich 55 ${version}: an emulation of the Roland SC-55 Sound Canvas (mk1), running its
original firmware on [Nuked-SC55](https://github.com/nukeykt/Nuked-SC55) by nukeykt, in
[J.C. Moyer's fork](https://github.com/jcmoyer/Nuked-SC55). All credit for the emulation
goes to them and to the contributors named in the README.

| File | Platform | Contents |
|---|---|---|
| \`Ostrich55-Windows.zip\` | Windows x86-64 | VST3, CLAP, standalone |
| \`Ostrich55-Linux.zip\` | Linux x86-64 | VST3, CLAP, standalone (JACK) |

Each zip has an INSTALL.txt. You need your own ROM images; none are included.

## How well tested

- **Windows VST3:** tested in a DAW.
- **Windows standalone:** tested with a USB keyboard (MIDI IN, BUFFER down to 64).
- **Windows CLAP and all Linux builds: untested.** They build without errors, and an
  automatic test loads the Linux CLAP once (without ROMs or panel), but nobody has used
  them yet.

Known limitations: the VST3 takes no SysEx from the DAW yet (the CLAP and the standalone
do); only the SC-55 mk1 so far. See the README.

Testers welcome: please report what works and what does not (DAW, OS, format) as an
[issue](https://github.com/Helge666/ostrich55/issues). Please do not report Ostrich 55's
problems to Nuked-SC55's authors.
EOF
}

case "${1:-}" in
	windows) windows ;;
	linux)   linux ;;
	notes)   notes "${2:?version}" ;;
	*) echo "usage: $0 windows|linux|notes VERSION" >&2; exit 2 ;;
esac
