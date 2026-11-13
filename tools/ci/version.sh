#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# An instrument's version for a CI run, from its source: plugin/ostrich/Version.hpp, or
# else getVersion()'s d_version(x, y, z) in plugin/ostrich/OstrichPlugin.cpp.
#
#   tools/ci/version.sh <instrument> <ref>
#
# A release tag (<tag>-v1.2.3, <tag>-preview-1.2.3) must name the source's version, or the
# binaries would report another one than the release says; then this fails. Otherwise:
# <version>-<commits>-g<hash>.
set -euo pipefail
. "$(dirname "$0")/common.sh"
load_instrument "${1:?instrument}"; ref="${2:?ref}"

hdr="$IDIR/plugin/ostrich/Version.hpp"
if [ -f "$hdr" ]; then
	num() { sed -n "s/^#define OSTRICH_VERSION_$1 *\([0-9][0-9]*\).*/\1/p" "$hdr" | head -1; }
	src="$(num MAJOR).$(num MINOR).$(num PATCH)"
else
	src="$(sed -n 's/.*d_version( *\([0-9]*\), *\([0-9]*\), *\([0-9]*\) *).*/\1.\2.\3/p' \
		"$IDIR/plugin/ostrich/OstrichPlugin.cpp" | head -1)"
fi
[[ "$src" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "no version found in $SLUG's source (got '$src')" >&2; exit 1; }

case "$ref" in
	refs/tags/"$TAG"-v*)        v="${ref#refs/tags/$TAG-v}" ;;
	refs/tags/"$TAG"-preview-*) v="${ref#refs/tags/$TAG-preview-}" ;;
	*) echo "$src-$(git -C "$ROOT" rev-list --count HEAD)-g$(git -C "$ROOT" rev-parse --short HEAD)"; exit 0 ;;
esac
if [ "$v" != "$src" ]; then
	echo "tag ${ref#refs/tags/} names $v, but $SLUG's source says $src" >&2; exit 1
fi
echo "$v"
