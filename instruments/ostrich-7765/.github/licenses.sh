#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The licence texts the binaries' parts ask to travel with them (THIRD-PARTY.md), into a
# folder of a release zip.
#
#   .github/licenses.sh <folder>

set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:?folder}"
mkdir -p "$out"

cp ../../third_party/dpf/LICENSE "$out/DPF-ISC.txt"
cp ../../third_party/dpf/dgl/src/pugl-upstream/COPYING "$out/pugl-ISC.txt"
cp ../../third_party/dpf/dgl/src/nanovg/LICENSE.txt "$out/NanoVG-zlib.txt"
cp ../../third_party/dpf/dgl/src/resources/LICENSE-DejaVuSans.ttf.txt "$out/DejaVuSans.txt"
cp ../../third_party/dpf/distrho/src/jackbridge/rtaudio/LICENSE "$out/RtAudio.txt"
cp ../../third_party/dpf/distrho/src/jackbridge/rtmidi/LICENSE "$out/RtMidi.txt"
cp plugin/ostrich/engine/LICENSE-MAME-BSD3.txt "$out/MAME-TR707-BSD3.txt"
