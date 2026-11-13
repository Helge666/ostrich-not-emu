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

cp "$tp/nuked-sc55/LICENSE" "$out/Nuked-SC55-GPL-2.0.txt"
sed -n '/Copyright (c) 2011 IETF Trust/,/POSSIBILITY OF SUCH DAMAGE/p' \
	"$tp/nuked-sc55/src/backend/sha/sha.h" > "$out/RFC6234-SHA-BSD-3-Clause.txt"
grep -q "IETF Trust" "$out/RFC6234-SHA-BSD-3-Clause.txt"
cp "$tp/dpf/LICENSE" "$out/DPF-ISC.txt"
cp "$tp/dpf/dgl/src/pugl-upstream/COPYING" "$out/pugl-ISC.txt"
cp "$tp/dpf/dgl/src/nanovg/LICENSE.txt" "$out/NanoVG-zlib.txt"
cp "$tp/dpf/dgl/src/resources/LICENSE-DejaVuSans.ttf.txt" "$out/DejaVuSans.txt"
cp "$tp/dpf/distrho/src/jackbridge/rtaudio/LICENSE" "$out/RtAudio.txt"
cp "$tp/dpf/distrho/src/jackbridge/rtmidi/LICENSE" "$out/RtMidi.txt"
