# SPDX-License-Identifier: GPL-3.0-or-later
# Sourced by the other tools/ci scripts: the repository root, and an instrument's ci.conf.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

load_instrument() {   # load_instrument <slug>   (ostrich-55 or 55)
	SLUG="ostrich-${1#ostrich-}"
	IDIR="$ROOT/instruments/$SLUG"
	[ -f "$IDIR/ci/ci.conf" ] || { echo "no instrument $SLUG (instruments/$SLUG/ci/ci.conf)" >&2; exit 2; }
	# shellcheck disable=SC1091
	. "$IDIR/ci/ci.conf"
}

all_instruments() {
	local d
	for d in "$ROOT"/instruments/*/ci/ci.conf; do basename "$(dirname "$(dirname "$d")")"; done
}
