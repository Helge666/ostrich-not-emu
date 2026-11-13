#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The licence texts the binaries' parts ask to travel with them (THIRD-PARTY.md), into a
# folder of a release zip.
#
#   ci/licenses.sh <folder>

set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:?folder}"
mkdir -p "$out"
tp=../../third_party
gm="$tp/gearmulator"

cp "$gm/LICENSE.md" "$out/gearmulator-GPL-3.0.txt"
cp "$gm/source/cpu/dsp56300/LICENSE.md" "$out/dsp56300-GPL-3.0.txt"
cp "$gm/source/cpu/dsp56300/source/asmjit/LICENSE.md" "$out/AsmJit-zlib.txt"
cp "$gm/source/3rdparty/libresample/LICENSE-BSD.txt" "$out/libresample-BSD-3-Clause.txt"
cp "$gm/source/3rdparty/libresample/LICENSE-LGPL.txt" "$out/libresample-LGPL-2.1.txt"
# Musashi (mc68k): its licence is part of its readme.
sed -n '/^LICENSE AND COPYRIGHT/,/^THE SOFTWARE\./p' "$gm/source/cpu/mc68k/Musashi/readme.txt" \
	> "$out/Musashi-MIT.txt"
grep -q "Karl Stenerud" "$out/Musashi-MIT.txt"
tail -1 "$out/Musashi-MIT.txt" | grep -q "^THE SOFTWARE\."
# Nuked-MT32 (GPL-2.0-or-later) and Munt (LGPL-2.1-or-later), from which parts of 88emu
# derive, ship no licence file inside gearmulator: the standard texts.
cp ../../shared/licenses/GPL-2.0.txt "$out/GPL-2.0-Nuked-MT32.txt"
cp "$gm/source/3rdparty/libresample/LICENSE-LGPL.txt" "$out/LGPL-2.1-Munt.txt"
cp "$tp/dpf/LICENSE" "$out/DPF-ISC.txt"
cp "$tp/dpf/dgl/src/pugl-upstream/COPYING" "$out/pugl-ISC.txt"
cp "$tp/dpf/dgl/src/nanovg/LICENSE.txt" "$out/NanoVG-zlib.txt"
cp "$tp/dpf/dgl/src/resources/LICENSE-DejaVuSans.ttf.txt" "$out/DejaVuSans.txt"
cp "$tp/dpf/distrho/src/jackbridge/rtaudio/LICENSE" "$out/RtAudio.txt"
cp "$tp/dpf/distrho/src/jackbridge/rtmidi/LICENSE" "$out/RtMidi.txt"
