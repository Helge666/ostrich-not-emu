#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The Windows binaries are linked statically: they may import system DLLs only, or hosts
# refuse to load them on a machine without mingw.
#
#   tools/ci/check-imports.sh <instrument>
set -euo pipefail
. "$(dirname "$0")/common.sh"
load_instrument "${1:?instrument}"
cd "$IDIR/plugin/ostrich/bin-win"
for f in "$NAME.clap" "$NAME.vst3/Contents/x86_64-win/$NAME.vst3" "$NAME.exe"; do
	echo "--- $f"
	x86_64-w64-mingw32-objdump -p "$f" | grep -i "DLL Name"
	if x86_64-w64-mingw32-objdump -p "$f" | grep -iE "DLL Name:.*(libstdc\+\+|libgcc|libwinpthread)"; then
		echo "::error title=Windows build::$f imports a mingw runtime DLL"
		exit 1
	fi
done
