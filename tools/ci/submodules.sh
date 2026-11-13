#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fetch the submodules an instrument needs (SUBMODULES in its ci.conf), shallow; in full if
# the server refuses a shallow fetch of the pinned commit.
#
#   tools/ci/submodules.sh <instrument>
set -euo pipefail
. "$(dirname "$0")/common.sh"
load_instrument "${1:?instrument}"
cd "$ROOT"
paths=()
for m in "${SUBMODULES[@]}"; do
	path="${m%%:*}"; rec=()
	[ "$m" != "$path" ] && rec=(--recursive)
	paths+=("$path")
	if ! git submodule update --init "${rec[@]}" --depth 1 -- "$path"; then
		echo "::warning title=Submodules::shallow fetch of $path failed, fetching in full"
		git submodule update --init "${rec[@]}" -- "$path"
	fi
done
git submodule status -- "${paths[@]}"
