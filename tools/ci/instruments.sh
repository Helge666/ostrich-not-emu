#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Which instruments a CI run builds, as a JSON list of slugs (for a build matrix).
#
#   tools/ci/instruments.sh <ref> [<base commit>]
#
#   refs/tags/<tag>-v* or <tag>-preview-*   that one instrument (<tag> from its ci.conf)
#   no base, or a base not in the history    all
#   otherwise                                those whose folder changed since <base>; all
#                                            if anything outside instruments/ changed.
#                                            Changes to Markdown files alone build nothing.
set -euo pipefail
. "$(dirname "$0")/common.sh"
ref="${1:?ref}"; base="${2:-}"

json() { local s="" x; for x in "$@"; do s="$s${s:+,}\"$x\""; done; echo "[$s]"; }

case "$ref" in
	refs/tags/*)
		tag="${ref#refs/tags/}"
		for s in $(all_instruments); do
			load_instrument "$s"
			case "$tag" in "$TAG"-v*|"$TAG"-preview-*) json "$SLUG"; exit 0 ;; esac
		done
		echo "tag $tag names no instrument" >&2; exit 1 ;;
esac

if [ -z "$base" ] || [ -z "${base//0/}" ] \
		|| ! git -C "$ROOT" cat-file -e "$base^{commit}" 2>/dev/null; then
	# shellcheck disable=SC2046
	json $(all_instruments); exit 0
fi

picked=()
while IFS= read -r f; do
	case "$f" in
		*.md) ;;
		instruments/*/*)
			s="${f#instruments/}"; s="${s%%/*}"
			[[ " ${picked[*]-} " == *" $s "* ]] || picked+=("$s") ;;
		*)
			# shellcheck disable=SC2046
			json $(all_instruments); exit 0 ;;
	esac
done < <(git -C "$ROOT" diff --name-only "$base" HEAD)
json "${picked[@]+"${picked[@]}"}"
