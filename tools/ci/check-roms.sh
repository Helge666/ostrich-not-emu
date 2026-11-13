#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# No manufacturer data may ever reach the repository: a last look at what is tracked.
# (Ostrich 220's own analysis plots are PDFs; those are allowed.)
#
#   tools/ci/check-roms.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
if git ls-files | grep -iE '\.(bin|rom|ic[0-9]+|zip|7z|syx|wav|flac|mp3|eprom)$'; then
	echo "::error title=ROMs::files of a ROM or recording type are tracked"
	exit 1
fi
echo "no ROM or recording files tracked"
