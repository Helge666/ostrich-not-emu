#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The release zips of an instrument, from its builds: <NAME>-Windows.zip and
# <NAME>-Linux.zip, each with VERSION.txt, INSTALL.txt (ci/release-text.sh), LICENSE,
# README.md, THIRD-PARTY.md and the licence texts (ci/licenses.sh).
#
#   tools/ci/package.sh <instrument> <version> <out folder>
set -euo pipefail
. "$(dirname "$0")/common.sh"
load_instrument "${1:?instrument}"; version="${2:?version}"
mkdir -p "${3:?out folder}"; out="$(cd "$3" && pwd)"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
commit="$(git -C "$ROOT" rev-parse HEAD)"

pack() {   # pack <zip> <section> <path...>
	local zip="$1" section="$2"; shift 2
	local dir="$stage/$section"
	mkdir -p "$dir"
	cp -r "$@" "$dir/"
	printf '%s\nversion  %s\ncommit   %s\n' "$TITLE" "$version" "$commit" > "$dir/VERSION.txt"
	"$IDIR/ci/release-text.sh" "$section" > "$dir/INSTALL.txt"
	cp "$IDIR/LICENSE" "$IDIR/README.md" "$IDIR/THIRD-PARTY.md" "$dir/"
	"$IDIR/ci/licenses.sh" "$dir/licenses"
	rm -f "$out/$zip"
	( cd "$dir" && zip -q -r -y "$out/$zip" . )
	echo "--- $zip"
	unzip -l "$out/$zip"
}

p="$IDIR/plugin"
pack "$NAME-Windows.zip" windows \
	"$p/ostrich/bin-win/$NAME.vst3" "$p/ostrich/bin-win/$NAME.clap" "$p/ostrich/bin-win/$NAME.exe"
pack "$NAME-Linux.zip" linux \
	"$p/bin/$NAME.vst3" "$p/bin/$NAME.clap" "$p/bin/$NAME"
