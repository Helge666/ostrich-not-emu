#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Install the Linux builds of the Ostrich instruments where hosts look for them:
#
#   VST3        ~/.vst3/<Name>.vst3
#   CLAP        ~/.clap/<Name>.clap
#   standalone  ~/.local/bin/<Name>
#
#   tools/install-plugins.sh [-n] [instrument ...]
#
#   instrument  32, 55, 220, 7765 or ostrich-32, ...; default: every instrument
#   -n          only say what would be installed
#
# VST3_DIR, CLAP_DIR and BIN_DIR override the folders.  An installed copy is replaced as a
# whole; a host that has the old one loaded keeps it until it reloads.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VST3_DIR="${VST3_DIR:-$HOME/.vst3}"
CLAP_DIR="${CLAP_DIR:-$HOME/.clap}"
BIN_DIR="${BIN_DIR:-$HOME/.local/bin}"

DRY=0
if [ "${1:-}" = "-n" ]; then DRY=1; shift; fi

slugs=()
if [ $# -eq 0 ]; then
	for d in "$ROOT"/instruments/*/; do slugs+=("$(basename "$d")"); done
else
	for a in "$@"; do slugs+=("ostrich-${a#ostrich-}"); done
fi

put() {   # put <built file or bundle> <destination>
	local src="$1" dst="$2"
	if [ "$DRY" = 1 ]; then echo "  would install $dst"; return; fi
	mkdir -p "$(dirname "$dst")"
	local tmp="$dst.new.$$"
	rm -rf "$tmp"
	cp -a "$src" "$tmp"
	rm -rf "$dst"
	mv "$tmp" "$dst"
	echo "  $dst"
}

missing=0
for s in "${slugs[@]}"; do
	mk="$ROOT/instruments/$s/plugin/ostrich/Makefile"
	[ -f "$mk" ] || { echo "$s: no such instrument" >&2; exit 2; }
	name="$(sed -n 's/^NAME *= *//p' "$mk")"
	bin="$ROOT/instruments/$s/plugin/bin"
	echo "$s ($name):"
	n=0
	if [ -d "$bin/$name.vst3" ]; then put "$bin/$name.vst3" "$VST3_DIR/$name.vst3"; n=$((n+1)); fi
	if [ -f "$bin/$name.clap" ]; then put "$bin/$name.clap" "$CLAP_DIR/$name.clap"; n=$((n+1)); fi
	if [ -f "$bin/$name" ];      then put "$bin/$name"      "$BIN_DIR/$name";      n=$((n+1)); fi
	if [ "$n" = 0 ]; then
		echo "  nothing built yet (make in instruments/$s/plugin/ostrich)"
		missing=1
	fi
done
exit "$missing"
