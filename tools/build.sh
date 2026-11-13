#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build the Ostrich instruments on the build host and install the Linux plugins.
#
#   tools/build.sh [--linux-only] [--no-install] [instrument ...]
#
#   instrument    32, 55, 220, 7765 or ostrich-32, ...; default: every instrument
#   --linux-only  skip the Windows build (mingw-w64)
#   --no-install  do not run tools/install-plugins.sh afterwards
#
# For each instrument: `make` (Linux VST3, CLAP, standalone -> plugin/bin) and `make win`
# (Windows -> plugin/ostrich/bin-win), at -j5 and nice 19 so the host stays responsive.
# Then the Linux builds that succeeded go to ~/.vst3, ~/.clap and ~/.local/bin.
# On Windows, tools/build.ps1 runs this over SSH and installs the Windows builds there.

set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WIN=1
INSTALL=1
slugs=()
for a in "$@"; do
	case "$a" in
		--linux-only) WIN=0 ;;
		--no-install) INSTALL=0 ;;
		-*) echo "unknown option $a" >&2; exit 2 ;;
		*) slugs+=("ostrich-${a#ostrich-}") ;;
	esac
done
if [ ${#slugs[@]} -eq 0 ]; then
	for d in "$ROOT"/instruments/*/; do slugs+=("$(basename "$d")"); done
fi

ok=()
failed=()
for s in "${slugs[@]}"; do
	dir="$ROOT/instruments/$s/plugin/ostrich"
	[ -f "$dir/Makefile" ] || { echo "$s: no such instrument" >&2; exit 2; }
	targets=(all)
	[ "$WIN" = 1 ] && targets+=(win)
	good=1
	for t in "${targets[@]}"; do
		echo "=== $s: make $t"
		start=$(date +%s)
		if nice -n 19 make -C "$dir" -j5 "$t"; then
			echo "=== $s: make $t ok ($(( $(date +%s) - start )) s)"
		else
			echo "=== $s: make $t FAILED"
			good=0
		fi
	done
	if [ "$good" = 1 ]; then ok+=("$s"); else failed+=("$s"); fi
done

if [ "$INSTALL" = 1 ] && [ ${#ok[@]} -gt 0 ]; then
	echo "=== installing the Linux plugins"
	"$ROOT/tools/install-plugins.sh" "${ok[@]}"
fi

echo
echo "built:  ${ok[*]:-none}"
echo "failed: ${failed[*]:-none}"
[ ${#failed[@]} -eq 0 ]
