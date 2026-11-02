#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the Windows installer from the cross-compiled plugin (cd plugin/ostrich; make win).
# Needs NSIS 3 (Ubuntu: the package nsis).
#
#   installer/build.sh [version]     -> installer/Ostrich55-Setup-<version>.exe
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${1:-$(sed -n 's/.*d_version(\([0-9]*\), *\([0-9]*\), *\([0-9]*\)).*/\1.\2.\3/p' "$HERE/plugin/ostrich/OstrichPlugin.cpp")}"
BIN="$HERE/plugin/ostrich/bin-win"
for f in Ostrich55.clap Ostrich55.exe Ostrich55.vst3; do
  [ -e "$BIN/$f" ] || { echo "missing $BIN/$f -- run 'make win' in plugin/ostrich first" >&2; exit 1; }
done
command -v makensis >/dev/null || { echo "makensis not found -- install NSIS (Ubuntu: nsis)" >&2; exit 1; }

# The texts that go with it, with Windows line ends.
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp -r "$BIN"/Ostrich55.vst3 "$BIN"/Ostrich55.clap "$BIN"/Ostrich55.exe "$STAGE/"
for t in LICENSE:LICENSE.txt installer/README.txt:README.txt THIRD-PARTY.md:THIRD-PARTY.txt; do
  sed 's/$/\r/' "$HERE/${t%%:*}" > "$STAGE/${t##*:}"
done

OUT="$HERE/installer/Ostrich55-Setup-$VERSION.exe"
makensis -V2 -INPUTCHARSET UTF8 -DVERSION="$VERSION" -DSRC="$STAGE" -DOUT="$OUT" "$HERE/installer/ostrich55.nsi"
echo "built $OUT"
