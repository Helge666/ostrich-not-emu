#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Load an instrument's Linux CLAP without a host, a display or ROMs, play a note, save the
# state, and check what it reports (SMOKE_* in its ci.conf).
#
#   tools/ci/smoke.sh <instrument>
set -euo pipefail
. "$(dirname "$0")/common.sh"
load_instrument "${1:?instrument}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
gcc -std=gnu11 -O2 -o "$tmp/render" "$ROOT/shared/tools/ostrich_clap_render.c" \
	-I"$ROOT/third_party/dpf/distrho/src" -ldl -lm
mkdir -p "$tmp/home" "$tmp/data" "$tmp/config" "$tmp/roms"
env HOME="$tmp/home" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config" \
	"$SMOKE_ENV=$tmp/roms" \
	"$tmp/render" "$IDIR/plugin/bin/$NAME.clap" "$tmp/out.wav" "${SMOKE_ARGS[@]}" \
	2>&1 | tee "$tmp/smoke.txt"
fail=0
for e in "${SMOKE_EXPECT[@]}"; do
	if grep -qF -- "$e" "$tmp/smoke.txt"; then
		echo "ok: $e"
	else
		echo "::error title=Smoke test::$TITLE did not say: $e"; fail=1
	fi
done
exit "$fail"
